import io
from datetime import datetime, timedelta, timezone

import pytest
from PIL import Image

from media_session import (
    MediaSnapshot,
    art_id,
    art_to_rgb565,
    build_payload,
    current_position,
    parse_command,
    source_label,
    track_key,
)

T0 = datetime(2026, 10, 10, 10, 28, 2, tzinfo=timezone.utc)


def snap(**kw) -> MediaSnapshot:
    base = dict(source_app="Brave", title="Şarkı", artist="Sanatçı", album="Albüm", status="PLAYING",
                position_s=3.1, duration_s=270.1, last_updated=T0, can_seek=True)
    base.update(kw)
    return MediaSnapshot(**base)


def png(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


@pytest.mark.parametrize("app_id, expected", [
    ("Brave", "Brave"),
    ("Spotify.exe", "Spotify"),
    ("chrome", "Chrome"),
    ("C:\\Program Files\\Foo\\player.exe", "Player"),
    ("Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic", "Microsoft.ZuneMusic"),
    ("", ""),
])
def test_source_label(app_id, expected):
    assert source_label(app_id) == expected


def test_position_extrapolates_while_playing():
    # The probe showed Brave keeps reporting the same position while playing.
    assert current_position(snap(), now=T0 + timedelta(seconds=10)) == pytest.approx(13.1)


def test_position_frozen_while_paused():
    assert current_position(snap(status="PAUSED"), now=T0 + timedelta(seconds=10)) == pytest.approx(3.1)


def test_position_clamped_to_duration():
    assert current_position(snap(), now=T0 + timedelta(hours=1)) == pytest.approx(270.1)


def test_position_never_runs_backwards_on_clock_skew():
    assert current_position(snap(), now=T0 - timedelta(seconds=5)) == pytest.approx(3.1)


def test_position_unknown():
    assert current_position(snap(position_s=None)) is None


def test_payload_inactive_without_session():
    assert build_payload(None, None, 168) == {"active": False}


def test_payload_fields():
    p = build_payload(snap(), "abc123", 168, now=T0 + timedelta(seconds=2))
    assert p["active"] is True
    assert p["source"] == "Brave"
    assert (p["title"], p["artist"], p["album"]) == ("Şarkı", "Sanatçı", "Albüm")
    assert p["status"] == "playing"
    assert p["position_s"] == pytest.approx(5.1)
    assert p["duration_s"] == pytest.approx(270.1)
    assert p["can_seek"] is True
    assert p["art"] == {"id": "abc123", "w": 168, "h": 168, "format": "rgb565le"}


def test_payload_without_art():
    assert build_payload(snap(), None, 168, now=T0)["art"] is None


def test_track_key_ignores_position_but_not_state():
    a = build_payload(snap(), "x", 168, now=T0)
    b = build_payload(snap(), "x", 168, now=T0 + timedelta(seconds=30))
    assert track_key(a) == track_key(b)
    assert track_key(a) != track_key(build_payload(snap(status="PAUSED"), "x", 168, now=T0))
    assert track_key(a) != track_key(build_payload(snap(), "y", 168, now=T0))
    assert track_key(a) != track_key(build_payload(snap(title="Başka"), "x", 168, now=T0))


def test_rgb565_size_and_byte_order():
    data = art_to_rgb565(png(Image.new("RGB", (120, 120), (255, 0, 0))), 8)
    assert len(data) == 8 * 8 * 2
    # pure red = 0xF800, little endian
    assert data[:2] == b"\x00\xf8"
    assert set(data[i:i + 2] for i in range(0, len(data), 2)) == {b"\x00\xf8"}


def test_rgb565_center_crops_non_square():
    img = Image.new("RGB", (300, 100), (255, 0, 0))
    img.paste((0, 255, 0), (100, 0, 200, 100))  # green middle third
    data = art_to_rgb565(png(img), 4)
    # 0x07E0 green; the red side bands must be cropped away entirely
    assert set(data[i:i + 2] for i in range(0, len(data), 2)) == {b"\xe0\x07"}


def test_art_id_is_stable_and_content_based():
    a = png(Image.new("RGB", (4, 4), (1, 2, 3)))
    b = png(Image.new("RGB", (4, 4), (3, 2, 1)))
    assert art_id(a) == art_id(a)
    assert art_id(a) != art_id(b)
    assert len(art_id(a)) == 12


@pytest.mark.parametrize("cmd, expected", [
    ({"action": "play_pause"}, ("play_pause", None)),
    ({"action": "next"}, ("next", None)),
    ({"action": "seek", "delta_s": -10}, ("seek", -10.0)),
    ({"action": "seek", "delta_s": 2.5}, ("seek", 2.5)),
])
def test_parse_command_valid(cmd, expected):
    assert parse_command(cmd) == expected


@pytest.mark.parametrize("cmd", [
    {},
    {"action": "shutdown"},
    {"action": "seek"},
    {"action": "seek", "delta_s": "10"},
    {"action": "seek", "delta_s": True},
    {"action": "seek", "delta_s": 10_000},
])
def test_parse_command_invalid(cmd):
    with pytest.raises(ValueError):
        parse_command(cmd)


def test_rgb565_bit_packing():
    # r=0x12 g=0x34 b=0x56 -> (0x10<<8)|(0x34<<3)|(0x56>>3) = 0x11AA
    data = art_to_rgb565(png(Image.new("RGB", (2, 2), (0x12, 0x34, 0x56))), 1)
    assert data == b"\xaa\x11"
