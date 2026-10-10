"""
Reads the current system output volume via pycaw and publishes it to
MQTT so the desk panel's media tile can show (and, via a burst of
volume-up/down HID presses from the panel, approximately steer) the
real PC volume.
"""

import json
import threading

import comtypes
from pycaw.pycaw import AudioUtilities

import config


def _get_volume_interface():
    return AudioUtilities.GetSpeakers().EndpointVolume


def audio_publish_loop(client, stop_event: threading.Event) -> None:
    comtypes.CoInitialize()
    try:
        while not stop_event.is_set():
            try:
                volume = _get_volume_interface()
                level = volume.GetMasterVolumeLevelScalar()
                muted = bool(volume.GetMute())
                payload = {"volume": round(level * 100), "muted": muted}
                client.publish(config.TOPIC_AUDIO, json.dumps(payload), qos=1, retain=True)
            except Exception as exc:  # pycaw/COM calls can transiently fail (device switch, etc.)
                print(f"Audio publish error: {exc}")
            stop_event.wait(config.AUDIO_PUBLISH_INTERVAL_S)
    finally:
        comtypes.CoUninitialize()
