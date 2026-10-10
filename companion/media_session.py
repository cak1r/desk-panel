"""
Platform-independent half of the now-playing feature: turns a snapshot of
the Windows SMTC session into the MQTT payload the panel reads, converts
cover art into a raw pixel blob LVGL can draw directly, and validates
transport commands coming back from the panel.

Kept free of any winrt import so it can be unit tested on Linux; the
Windows glue lives in media_publisher.py.
"""

import hashlib
import io
from dataclasses import dataclass
from datetime import datetime, timezone

from PIL import Image

SEEK_LIMIT_S = 600

# Actions the panel may send on the media command topic.
ACTIONS = ("play_pause", "play", "pause", "next", "previous", "seek")


@dataclass
class MediaSnapshot:
    source_app: str
    title: str
    artist: str
    album: str
    status: str  # "PLAYING", "PAUSED", "STOPPED", ...
    position_s: float | None
    duration_s: float | None
    last_updated: datetime | None
    can_seek: bool = False


def source_label(app_id: str) -> str:
    """'Brave' / 'Chrome.exe' / 'Spotify.exe' style SMTC app ids -> a short
    name for the panel ('Brave', 'Chrome', 'Spotify')."""
    name = app_id.rsplit("\\", 1)[-1].rsplit("!", 1)[-1]
    if name.lower().endswith(".exe"):
        name = name[:-4]
    return name[:1].upper() + name[1:] if name else ""


def current_position(snap: MediaSnapshot, now: datetime | None = None) -> float | None:
    """Chromium only reports the timeline on state changes (play, pause,
    seek), so while playing the real position has to be extrapolated from
    the last report. Clamped to [0, duration]."""
    if snap.position_s is None:
        return None
    pos = snap.position_s
    if snap.status == "PLAYING" and snap.last_updated is not None:
        now = now or datetime.now(timezone.utc)
        pos += max(0.0, (now - snap.last_updated).total_seconds())
    if snap.duration_s:
        pos = min(pos, snap.duration_s)
    return max(0.0, pos)


def build_payload(snap: MediaSnapshot | None, art_id: str | None, art_size: int,
                  now: datetime | None = None) -> dict:
    """JSON for the now-playing topic. Position is sent pre-extrapolated
    together with the moment it was computed, so the panel only has to keep
    counting while status is PLAYING."""
    if snap is None:
        return {"active": False}
    now = now or datetime.now(timezone.utc)
    pos = current_position(snap, now)
    return {
        "active": True,
        "source": source_label(snap.source_app),
        "title": snap.title,
        "artist": snap.artist,
        "album": snap.album,
        "status": snap.status.lower(),
        "position_s": round(pos, 1) if pos is not None else None,
        "duration_s": round(snap.duration_s, 1) if snap.duration_s else None,
        "can_seek": snap.can_seek,
        "art": {"id": art_id, "w": art_size, "h": art_size, "format": "rgb565le"} if art_id else None,
    }


def track_key(payload: dict) -> tuple:
    """Fields whose change warrants an immediate publish. Position is left
    out on purpose -- it changes every tick and the panel extrapolates it."""
    return tuple(payload.get(k) for k in ("active", "source", "title", "artist", "album", "status", "duration_s")) + (
        (payload.get("art") or {}).get("id"),
    )


def art_to_rgb565(image_bytes: bytes, size: int) -> bytes:
    """Decode any PIL-readable image, center-crop to a square, resize to
    size x size and pack as little-endian RGB565 -- the byte order LVGL
    expects for LV_IMG_CF_TRUE_COLOR with LV_COLOR_16_SWAP off."""
    img = Image.open(io.BytesIO(image_bytes)).convert("RGB")
    w, h = img.size
    side = min(w, h)
    left, top = (w - side) // 2, (h - side) // 2
    img = img.crop((left, top, left + side, top + side)).resize((size, size), Image.LANCZOS)

    rgb = img.tobytes()
    out = bytearray(size * size * 2)
    for px in range(size * size):
        r, g, b = rgb[px * 3], rgb[px * 3 + 1], rgb[px * 3 + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[px * 2] = v & 0xFF
        out[px * 2 + 1] = v >> 8
    return bytes(out)


def art_id(image_bytes: bytes) -> str:
    return hashlib.sha1(image_bytes).hexdigest()[:12]


def parse_command(payload: dict) -> tuple[str, float | None]:
    """Validate a media command from the panel. Returns (action, seek
    delta in seconds or None); raises ValueError on anything else."""
    action = payload.get("action")
    if action not in ACTIONS:
        raise ValueError(f"unknown media action: {action!r}")
    if action != "seek":
        return action, None
    delta = payload.get("delta_s")
    if not isinstance(delta, (int, float)) or isinstance(delta, bool):
        raise ValueError("seek needs a numeric delta_s")
    if abs(delta) > SEEK_LIMIT_S:
        raise ValueError(f"seek delta out of range: {delta}")
    return action, float(delta)
