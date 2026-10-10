"""
Publishes per-app volumes (plus output device and microphone) and applies
mixer commands from the panel via pycaw.

Topics (see config.example.py):
  TOPIC_MIXER_STATE  JSON, retained -- see mixer.build_payload
  TOPIC_MIXER_CMD    panel -> PC: {"target": "brave.exe" | "@output" | "@mic",
                     "volume": 0-100, "mute": true/false}

COM objects are apartment-bound, so commands arriving on paho's thread are
queued and applied on this publisher's own thread.
"""

import json
import queue
import threading
import time

import comtypes
from pycaw.pycaw import AudioUtilities

import config
from mixer import MIC_TARGET, OUTPUT_TARGET, AppSession, build_channels, build_payload, parse_command


def _endpoint(getter):
    dev = getter()
    # pycaw returns the wrapped AudioDevice for speakers but a raw
    # IMMDevice for the microphone on some versions.
    if not hasattr(dev, "EndpointVolume"):
        dev = AudioUtilities.CreateDevice(dev)
    return dev


def _endpoint_state(getter) -> dict | None:
    try:
        dev = _endpoint(getter)
        ep = dev.EndpointVolume
        return {
            "name": getattr(dev, "FriendlyName", None),
            "volume": round(ep.GetMasterVolumeLevelScalar() * 100),
            "muted": bool(ep.GetMute()),
        }
    except Exception as exc:  # no mic plugged in, device switching, ...
        print(f"Mixer endpoint read error: {exc}")
        return None


class MixerPublisher:
    def __init__(self, client, stop_event: threading.Event):
        self._client = client
        self._stop = stop_event
        self._commands: queue.Queue = queue.Queue()
        self._wake = threading.Event()

    def handle_command(self, cmd: dict) -> None:
        self._commands.put(parse_command(cmd))
        self._wake.set()

    def run(self) -> None:
        comtypes.CoInitialize()
        try:
            last_payload = None
            last_publish = 0.0
            while not self._stop.is_set():
                self._drain_commands()
                try:
                    payload = self._read_state()
                    now = time.monotonic()
                    if payload != last_payload or now - last_publish >= config.MIXER_RESYNC_S:
                        self._client.publish(config.TOPIC_MIXER_STATE, json.dumps(payload, ensure_ascii=False), qos=1, retain=True)
                        last_payload = payload
                        last_publish = now
                except Exception as exc:
                    print(f"Mixer publish error: {exc}")
                # Wake early when a command arrives so the panel sees the
                # result of a slider drag right away.
                self._wake.wait(config.MIXER_POLL_INTERVAL_S)
                self._wake.clear()
        finally:
            comtypes.CoUninitialize()

    def _read_state(self) -> dict:
        sessions = [
            AppSession(
                process=s.Process.name() if s.Process else None,
                volume=s.SimpleAudioVolume.GetMasterVolume(),
                muted=bool(s.SimpleAudioVolume.GetMute()),
            )
            for s in AudioUtilities.GetAllSessions()
        ]
        channels = build_channels(sessions, config.MIXER_DISPLAY_NAMES, config.MIXER_HIDDEN_APPS)
        return build_payload(
            _endpoint_state(AudioUtilities.GetSpeakers),
            _endpoint_state(AudioUtilities.GetMicrophone),
            channels,
        )

    def _drain_commands(self) -> None:
        while True:
            try:
                target, volume, mute = self._commands.get_nowait()
            except queue.Empty:
                return
            try:
                self._apply(target, volume, mute)
            except Exception as exc:
                print(f"Mixer command for '{target}' failed: {exc}")

    def _apply(self, target: str, volume: int | None, mute: bool | None) -> None:
        if target in (OUTPUT_TARGET, MIC_TARGET):
            getter = AudioUtilities.GetSpeakers if target == OUTPUT_TARGET else AudioUtilities.GetMicrophone
            ep = _endpoint(getter).EndpointVolume
            if volume is not None:
                ep.SetMasterVolumeLevelScalar(volume / 100, None)
            if mute is not None:
                ep.SetMute(mute, None)
            return

        matched = False
        for s in AudioUtilities.GetAllSessions():
            if s.Process and s.Process.name().lower() == target:
                matched = True
                if volume is not None:
                    s.SimpleAudioVolume.SetMasterVolume(volume / 100, None)
                if mute is not None:
                    s.SimpleAudioVolume.SetMute(mute, None)
        if not matched:
            print(f"Mixer: no audio session for '{target}'")
