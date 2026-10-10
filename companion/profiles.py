"""
Maps the foreground window to a panel profile (the page the panel should
jump to), Stream Deck "smart profile" style. Pure logic -- the Win32 polling
lives in foreground_publisher.py.

A rule is a dict:
  {"profile": "ytmusic", "process": ["brave.exe", ...], "title": "YouTube Music"}
"process" (any of, case-insensitive) and "title" (substring,
case-insensitive) are both optional; the first matching rule wins, so put
more specific rules first.
"""

import time

BROWSERS = ["brave.exe", "chrome.exe", "msedge.exe", "firefox.exe"]

DEFAULT_RULES = [
    {"profile": "ytmusic", "process": BROWSERS, "title": "YouTube Music"},
    {"profile": "youtube", "process": BROWSERS, "title": "YouTube"},
    {"profile": "whatsapp", "process": ["whatsapp.root.exe", "whatsapp.exe"]},
]

# Shell surfaces that briefly take focus (Alt+Tab, Win+R, quick settings,
# the desktop) -- switching profile for them would make the panel flicker.
_TRANSIENT = {
    ("explorer.exe", ""),
    ("explorer.exe", "task switching"),
    ("explorer.exe", "program manager"),
    ("explorer.exe", "run"),
}
_TRANSIENT_PROCESSES = {"shellhost.exe", "shellexperiencehost.exe", "searchhost.exe", "startmenuexperiencehost.exe"}


def is_transient(process: str, title: str) -> bool:
    p = process.lower()
    return p in _TRANSIENT_PROCESSES or (p, title.strip().lower()) in _TRANSIENT


def match_profile(process: str, title: str, rules: list[dict]) -> str | None:
    p = process.lower()
    t = title.lower()
    for rule in rules:
        procs = rule.get("process")
        if procs and p not in {x.lower() for x in procs}:
            continue
        needle = rule.get("title")
        if needle and needle.lower() not in t:
            continue
        return rule["profile"]
    return None


class Debouncer:
    """Only reports a value once it has stayed the same for `hold_s`, so a
    quick Alt+Tab through other windows doesn't bounce the panel around."""

    def __init__(self, hold_s: float, clock=time.monotonic):
        self._hold = hold_s
        self._clock = clock
        self._candidate = object()
        self._since = 0.0
        self.value = object()  # sentinel: nothing reported yet

    def update(self, value) -> bool:
        """Feed the latest raw value; returns True when the reported value changed."""
        now = self._clock()
        if value != self._candidate:
            self._candidate = value
            self._since = now
        if self._candidate != self.value and now - self._since >= self._hold:
            self.value = self._candidate
            return True
        return False
