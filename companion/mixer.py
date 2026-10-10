"""
Platform-independent half of the per-app volume mixer: folds the raw
Windows audio sessions into one channel per app, builds the MQTT payload
and validates mixer commands from the panel. The pycaw glue lives in
mixer_publisher.py.
"""

from dataclasses import dataclass

OUTPUT_TARGET = "@output"
MIC_TARGET = "@mic"

DEFAULT_DISPLAY_NAMES = {
    "brave.exe": "Brave",
    "chrome.exe": "Chrome",
    "msedge.exe": "Edge",
    "firefox.exe": "Firefox",
    "discord.exe": "Discord",
    "whatsapp.root.exe": "WhatsApp",
    "whatsapp.exe": "WhatsApp",
    "spotify.exe": "Spotify",
    "steam.exe": "Steam",
}


@dataclass
class AppSession:
    process: str | None  # None for the "System Sounds" session
    volume: float  # 0.0 - 1.0
    muted: bool


def display_name(process: str, overrides: dict[str, str] | None = None) -> str:
    key = process.lower()
    names = {**DEFAULT_DISPLAY_NAMES, **{k.lower(): v for k, v in (overrides or {}).items()}}
    if key in names:
        return names[key]
    stem = process[:-4] if key.endswith(".exe") else process
    return stem[:1].upper() + stem[1:]


def build_channels(sessions: list[AppSession], overrides: dict[str, str] | None = None,
                   hidden: list[str] | None = None) -> list[dict]:
    """One channel per process. Apps like Discord open several sessions;
    they are merged (loudest volume shown, muted only if all are muted) and
    a command on the channel is applied to every session of that process."""
    hidden_set = {h.lower() for h in (hidden or [])}
    merged: dict[str, dict] = {}
    for s in sessions:
        if not s.process or s.process.lower() in hidden_set:
            continue
        key = s.process.lower()
        ch = merged.get(key)
        if ch is None:
            merged[key] = {"id": key, "name": display_name(s.process, overrides),
                           "volume": s.volume, "muted": s.muted}
        else:
            ch["volume"] = max(ch["volume"], s.volume)
            ch["muted"] = ch["muted"] and s.muted
    channels = sorted(merged.values(), key=lambda c: c["name"].lower())
    for ch in channels:
        ch["volume"] = round(ch["volume"] * 100)
    return channels


def build_payload(output: dict | None, mic: dict | None, channels: list[dict]) -> dict:
    """output / mic: {"name": str, "volume": 0-100, "muted": bool} or None."""
    return {"output": output, "mic": mic, "apps": channels}


def parse_command(cmd: dict) -> tuple[str, int | None, bool | None]:
    """{"target": "brave.exe" | "@output" | "@mic", "volume": 0-100, "mute": bool}
    -> (target, volume, mute). At least one of volume / mute is required."""
    target = cmd.get("target")
    if not isinstance(target, str) or not target or len(target) > 64:
        raise ValueError(f"invalid mixer target: {target!r}")

    volume = cmd.get("volume")
    if volume is not None:
        if isinstance(volume, bool) or not isinstance(volume, (int, float)):
            raise ValueError("volume must be a number")
        volume = max(0, min(100, round(volume)))

    mute = cmd.get("mute")
    if mute is not None and not isinstance(mute, bool):
        raise ValueError("mute must be true or false")

    if volume is None and mute is None:
        raise ValueError("mixer command needs volume and/or mute")
    return target.lower(), volume, mute
