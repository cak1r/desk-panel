"""
Watches the foreground window and publishes which panel profile matches
it (see profiles.py), so the panel can switch to e.g. its YouTube page
while a YouTube tab is focused.

Topic (see config.example.py):
  TOPIC_PROFILE  JSON, retained: {"profile": "youtube" | "ytmusic" | ... | null,
                                  "app": "brave.exe"}
Window titles are deliberately not published -- they can contain document
names, chat names, etc.
"""

import ctypes
import ctypes.wintypes
import json
import threading

import psutil

import config
from profiles import Debouncer, is_transient, match_profile


def foreground_window() -> tuple[str, str]:
    """(process name, window title) of the current foreground window."""
    user32 = ctypes.windll.user32
    hwnd = user32.GetForegroundWindow()
    length = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, buf, length + 1)
    pid = ctypes.wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    try:
        proc = psutil.Process(pid.value).name()
    except psutil.Error:
        proc = ""
    return proc, buf.value


def foreground_publish_loop(client, stop_event: threading.Event) -> None:
    debounce = Debouncer(config.PROFILE_DEBOUNCE_S)
    while not stop_event.is_set():
        try:
            proc, title = foreground_window()
            if proc and not is_transient(proc, title):
                state = (match_profile(proc, title, config.PROFILE_RULES), proc.lower())
                if debounce.update(state):
                    profile, app = debounce.value
                    client.publish(config.TOPIC_PROFILE, json.dumps({"profile": profile, "app": app}),
                                   qos=1, retain=True)
        except Exception as exc:
            print(f"Foreground publish error: {exc}")
        stop_event.wait(config.PROFILE_POLL_INTERVAL_S)
