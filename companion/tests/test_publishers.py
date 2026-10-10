"""
Exercises the Windows glue of the media and mixer publishers against fakes
shaped like the pywinrt / pycaw objects probe.py saw on the real PC, so the
wiring is checked on any OS.
"""

import io
import json
import sys
import threading
import types
from datetime import datetime, timedelta, timezone

import pytest
from PIL import Image

import config


class FakeClient:
    def __init__(self):
        self.published = []

    def publish(self, topic, payload, qos=0, retain=False):
        self.published.append((topic, payload, retain))

    def on(self, topic):
        return [p for t, p, _ in self.published if t == topic]


# ---------------------------------------------------------------- media ----

def _png(color) -> bytes:
    buf = io.BytesIO()
    Image.new("RGB", (120, 120), color).save(buf, format="PNG")
    return buf.getvalue()


class FakeMediaSession:
    def __init__(self, app="Brave", title="İncir", thumb=b"cover-1", status="PLAYING"):
        self.source_app_user_model_id = app
        self.title = title
        self.thumb = thumb
        self.status = status
        self.position = timedelta(seconds=3.1)
        self.calls = []

    async def try_get_media_properties_async(self):
        return types.SimpleNamespace(title=self.title, artist="İlyas Yalçıntaş",
                                     album_title="İçimdeki Duman", thumbnail=self.thumb)

    def get_playback_info(self):
        return types.SimpleNamespace(
            playback_status=types.SimpleNamespace(name=self.status),
            controls=types.SimpleNamespace(is_playback_position_enabled=True),
        )

    def get_timeline_properties(self):
        return types.SimpleNamespace(position=self.position, end_time=timedelta(seconds=270.1),
                                     last_updated_time=datetime.now(timezone.utc))

    def __getattr__(self, name):
        if name.startswith("try_"):
            async def _call(*args):
                self.calls.append((name, args))
                return True
            return _call
        raise AttributeError(name)


class FakeManager:
    def __init__(self, sessions):
        self.sessions = sessions

    def get_sessions(self):
        return list(self.sessions)

    def get_current_session(self):
        return self.sessions[0] if self.sessions else None


@pytest.fixture
def media(monkeypatch):
    import media_publisher

    covers = {b"cover-1": _png((255, 0, 0)), b"cover-2": _png((0, 0, 255))}

    async def fake_read(ref):
        return covers[ref]

    monkeypatch.setattr(media_publisher, "_read_stream", fake_read)
    client = FakeClient()
    pub = media_publisher.MediaPublisher(client, threading.Event())
    return pub, client


def _tick(pub, manager):
    pub._manager = manager
    pub._loop.run_until_complete(pub._tick())


def test_media_publishes_track_and_art(media):
    pub, client = media
    _tick(pub, FakeManager([FakeMediaSession()]))

    (art,) = client.on(config.TOPIC_MEDIA_ART)
    assert len(art) == config.MEDIA_ART_SIZE ** 2 * 2
    (raw,) = client.on(config.TOPIC_MEDIA_NOWPLAYING)
    payload = json.loads(raw)
    assert payload["title"] == "İncir"
    assert payload["source"] == "Brave"
    assert payload["status"] == "playing"
    assert payload["art"]["id"]
    assert "İncir" in raw  # sent as UTF-8, not \u escapes


def test_media_skips_unchanged_ticks_and_art(media):
    pub, client = media
    manager = FakeManager([FakeMediaSession()])
    _tick(pub, manager)
    _tick(pub, manager)
    assert len(client.on(config.TOPIC_MEDIA_NOWPLAYING)) == 1
    assert len(client.on(config.TOPIC_MEDIA_ART)) == 1


def test_media_new_track_gets_new_art(media):
    pub, client = media
    session = FakeMediaSession()
    manager = FakeManager([session])
    _tick(pub, manager)
    first_id = json.loads(client.on(config.TOPIC_MEDIA_NOWPLAYING)[-1])["art"]["id"]
    session.title, session.thumb = "Seni Severdim", b"cover-2"
    _tick(pub, manager)
    assert len(client.on(config.TOPIC_MEDIA_ART)) == 2
    assert json.loads(client.on(config.TOPIC_MEDIA_NOWPLAYING)[-1])["art"]["id"] != first_id


def test_media_late_artwork_is_retried(media):
    pub, client = media
    session = FakeMediaSession(thumb=None)
    manager = FakeManager([session])
    _tick(pub, manager)
    assert client.on(config.TOPIC_MEDIA_ART) == []
    session.thumb = b"cover-1"  # Chromium attaches it a moment later
    _tick(pub, manager)
    assert len(client.on(config.TOPIC_MEDIA_ART)) == 1


def test_media_prefers_configured_app(media):
    pub, client = media
    _tick(pub, FakeManager([FakeMediaSession(app="Spotify.exe", title="Spotify şarkısı"),
                            FakeMediaSession(app="Brave", title="Brave şarkısı")]))
    assert json.loads(client.on(config.TOPIC_MEDIA_NOWPLAYING)[-1])["title"] == "Brave şarkısı"


def test_media_no_session(media):
    pub, client = media
    _tick(pub, FakeManager([]))
    assert json.loads(client.on(config.TOPIC_MEDIA_NOWPLAYING)[-1]) == {"active": False}


def test_media_commands(media):
    pub, _ = media
    session = FakeMediaSession()
    pub._manager = FakeManager([session])
    run = pub._loop.run_until_complete
    run(pub._execute("play_pause", None))
    run(pub._execute("next", None))
    run(pub._execute("seek", -10.0))
    names = [c[0] for c in session.calls]
    assert names == ["try_toggle_play_pause_async", "try_skip_next_async", "try_change_playback_position_async"]
    ticks = session.calls[-1][1][0]
    assert ticks == 0  # 3.1 s - 10 s clamps to the start


def test_media_handle_command_validates_before_scheduling(media):
    pub, _ = media
    with pytest.raises(ValueError):
        pub.handle_command({"action": "format_c"})


# ---------------------------------------------------------------- mixer ----

class FakeVolume:
    def __init__(self, level, muted=False):
        self.level, self.muted = level, muted

    def GetMasterVolume(self):
        return self.level

    def SetMasterVolume(self, v, ctx):
        self.level = v

    def GetMasterVolumeLevelScalar(self):
        return self.level

    def SetMasterVolumeLevelScalar(self, v, ctx):
        self.level = v

    def GetMute(self):
        return int(self.muted)

    def SetMute(self, m, ctx):
        self.muted = bool(m)


def _session(name, level, muted=False):
    proc = types.SimpleNamespace(name=lambda: name) if name else None
    return types.SimpleNamespace(Process=proc, SimpleAudioVolume=FakeVolume(level, muted))


@pytest.fixture
def mixer(monkeypatch):
    speakers = types.SimpleNamespace(FriendlyName="Hoparlör (Razer)", EndpointVolume=FakeVolume(0.42))
    mic_raw = object()  # raw IMMDevice, like pycaw returned on the real PC
    mic = types.SimpleNamespace(FriendlyName="Mikrofon (Razer)", EndpointVolume=FakeVolume(1.0))
    sessions = [_session(None, 1.0), _session("steam.exe", 1.0), _session("brave.exe", 0.64),
                _session("Discord.exe", 0.5), _session("Discord.exe", 0.8)]
    audio_utils = types.SimpleNamespace(
        GetAllSessions=lambda: sessions,
        GetSpeakers=lambda: speakers,
        GetMicrophone=lambda: mic_raw,
        CreateDevice=lambda dev: mic if dev is mic_raw else dev,
    )
    monkeypatch.setitem(sys.modules, "comtypes", types.SimpleNamespace(CoInitialize=lambda: None,
                                                                       CoUninitialize=lambda: None))
    monkeypatch.setitem(sys.modules, "pycaw", types.ModuleType("pycaw"))
    monkeypatch.setitem(sys.modules, "pycaw.pycaw", types.SimpleNamespace(AudioUtilities=audio_utils))
    sys.modules.pop("mixer_publisher", None)
    import mixer_publisher

    client = FakeClient()
    pub = mixer_publisher.MixerPublisher(client, threading.Event())
    yield pub, client, sessions, speakers, mic
    sys.modules.pop("mixer_publisher", None)


def test_mixer_state(mixer):
    pub, _, _, _, _ = mixer
    state = pub._read_state()
    assert state["output"] == {"name": "Hoparlör (Razer)", "volume": 42, "muted": False}
    assert state["mic"]["name"] == "Mikrofon (Razer)"
    assert [a["id"] for a in state["apps"]] == ["brave.exe", "discord.exe", "steam.exe"]


def test_mixer_commands_apply_to_every_session_of_app(mixer):
    pub, _, sessions, speakers, mic = mixer
    pub.handle_command({"target": "discord.exe", "volume": 20, "mute": True})
    pub.handle_command({"target": "@output", "volume": 70})
    pub.handle_command({"target": "@mic", "mute": True})
    pub._drain_commands()
    discord = [s for s in sessions if s.Process and s.Process.name() == "Discord.exe"]
    assert all(s.SimpleAudioVolume.level == pytest.approx(0.2) and s.SimpleAudioVolume.muted for s in discord)
    assert speakers.EndpointVolume.level == pytest.approx(0.7)
    assert mic.EndpointVolume.muted is True


def test_mixer_run_publishes_and_stops(mixer):
    pub, client, _, _, _ = mixer
    def stop_after_first_publish(*a, **kw):
        FakeClient.publish(client, *a, **kw)
        pub._stop.set()

    client.publish = stop_after_first_publish
    pub.run()
    (raw,) = client.on(config.TOPIC_MIXER_STATE)
    assert "Hoparlör" in raw
