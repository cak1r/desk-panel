"""
Publishes what is playing on the PC (Windows SMTC -- the same source as
the volume flyout's media card) and executes transport commands sent back
from the panel. Works for any SMTC-aware player; Chromium browsers such as
Brave expose YouTube / YouTube Music tabs through it automatically.

Topics (see config.example.py):
  TOPIC_MEDIA_NOWPLAYING  JSON, retained -- see media_session.build_payload
  TOPIC_MEDIA_ART         raw RGB565 cover, retained, MEDIA_ART_SIZE^2 * 2 bytes
  TOPIC_MEDIA_CMD         panel -> PC: {"action": "play_pause" | "next" |
                          "previous" | "play" | "pause" | "seek", "delta_s": -10}
"""

import asyncio
import json
import threading
import time

import config
from media_session import (
    MediaSnapshot,
    art_id,
    art_to_rgb565,
    build_payload,
    current_position,
    parse_command,
    track_key,
)

THUMBNAIL_RETRIES = 5
TICKS_PER_SECOND = 10_000_000


def _seconds(value) -> float | None:
    """WinRT TimeSpan arrives as datetime.timedelta on current pywinrt."""
    if value is None:
        return None
    if hasattr(value, "total_seconds"):
        return value.total_seconds()
    return value.duration / TICKS_PER_SECOND


async def _read_stream(thumb_ref) -> bytes:
    from winrt.windows.storage.streams import Buffer, InputStreamOptions

    stream = await thumb_ref.open_read_async()
    buf = Buffer(stream.size)
    result = await stream.read_async(buf, stream.size, InputStreamOptions.READ_AHEAD)
    return bytes(memoryview(result))[: result.length]


class MediaPublisher:
    def __init__(self, client, stop_event: threading.Event):
        self._client = client
        self._stop = stop_event
        self._loop = asyncio.new_event_loop()
        self._manager = None
        self._last_key = None
        self._last_publish = 0.0
        self._track = None  # (title, artist, album) the current art belongs to
        self._art_id = None
        self._thumb_attempts = 0
        self._pending_thumb = None

    # -- thread entry -----------------------------------------------------

    def run(self) -> None:
        asyncio.set_event_loop(self._loop)
        try:
            self._loop.run_until_complete(self._main())
        finally:
            self._loop.close()

    def handle_command(self, cmd: dict) -> None:
        """Called on paho's thread; hops onto the asyncio loop that owns the
        WinRT session objects."""
        action, delta = parse_command(cmd)
        asyncio.run_coroutine_threadsafe(self._execute(action, delta), self._loop)

    # -- internals --------------------------------------------------------

    async def _main(self) -> None:
        from winrt.windows.media.control import (
            GlobalSystemMediaTransportControlsSessionManager as MediaManager,
        )

        self._manager = await MediaManager.request_async()
        while not self._stop.is_set():
            try:
                await self._tick()
            except Exception as exc:  # session vanished mid-read, COM hiccup, ...
                print(f"Media publish error: {exc}")
            await asyncio.sleep(config.MEDIA_POLL_INTERVAL_S)

    def _pick_session(self):
        sessions = list(self._manager.get_sessions())
        for preferred in config.MEDIA_PREFERRED_APPS:
            for s in sessions:
                if preferred.lower() in s.source_app_user_model_id.lower():
                    return s
        return self._manager.get_current_session()

    async def _snapshot(self, session) -> MediaSnapshot:
        props = await session.try_get_media_properties_async()
        playback = session.get_playback_info()
        timeline = session.get_timeline_properties()
        status = getattr(playback.playback_status, "name", str(playback.playback_status))
        self._pending_thumb = props.thumbnail
        return MediaSnapshot(
            source_app=session.source_app_user_model_id,
            title=props.title or "",
            artist=props.artist or "",
            album=props.album_title or "",
            status=status,
            position_s=_seconds(timeline.position),
            duration_s=_seconds(timeline.end_time) or None,
            last_updated=timeline.last_updated_time,
            can_seek=bool(playback.controls.is_playback_position_enabled),
        )

    async def _update_art(self, snap: MediaSnapshot) -> None:
        track = (snap.title, snap.artist, snap.album)
        if track != self._track:
            self._track = track
            self._art_id = None
            self._thumb_attempts = 0
        # Chromium sometimes attaches artwork a moment after the title, so
        # keep retrying a few polls before giving up on this track.
        if self._art_id or self._thumb_attempts >= THUMBNAIL_RETRIES:
            return
        self._thumb_attempts += 1
        if self._pending_thumb is None:
            return
        raw = await _read_stream(self._pending_thumb)
        new_id = art_id(raw)
        pixels = art_to_rgb565(raw, config.MEDIA_ART_SIZE)
        self._client.publish(config.TOPIC_MEDIA_ART, pixels, qos=1, retain=True)
        self._art_id = new_id

    async def _tick(self) -> None:
        session = self._pick_session()
        snap = None
        if session is not None:
            snap = await self._snapshot(session)
            await self._update_art(snap)

        payload = build_payload(snap, self._art_id, config.MEDIA_ART_SIZE)
        key = track_key(payload)
        now = time.monotonic()
        # Publish immediately on any track/state change, otherwise resync
        # the position periodically so panel-side extrapolation can't drift.
        if key != self._last_key or now - self._last_publish >= config.MEDIA_RESYNC_S:
            self._client.publish(config.TOPIC_MEDIA_NOWPLAYING, json.dumps(payload, ensure_ascii=False), qos=1, retain=True)
            self._last_key = key
            self._last_publish = now

    async def _execute(self, action: str, delta: float | None) -> None:
        try:
            session = self._pick_session()
            if session is None:
                return
            if action == "play_pause":
                await session.try_toggle_play_pause_async()
            elif action == "play":
                await session.try_play_async()
            elif action == "pause":
                await session.try_pause_async()
            elif action == "next":
                await session.try_skip_next_async()
            elif action == "previous":
                await session.try_skip_previous_async()
            elif action == "seek":
                snap = await self._snapshot(session)
                pos = current_position(snap)
                if pos is None or not snap.can_seek:
                    return
                target = max(0.0, pos + delta)
                if snap.duration_s:
                    target = min(target, snap.duration_s)
                await session.try_change_playback_position_async(int(target * TICKS_PER_SECOND))
            self._last_key = None  # force a publish on the next tick
        except Exception as exc:
            print(f"Media command '{action}' failed: {exc}")
