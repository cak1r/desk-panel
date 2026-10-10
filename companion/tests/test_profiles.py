import pytest

from profiles import DEFAULT_RULES, Debouncer, is_transient, match_profile


# Titles as captured by probe.py on the real PC.
@pytest.mark.parametrize("process, title, expected", [
    ("brave.exe", "Seni Severdim (feat. Yaşar Günaçgün) | YouTube Music - Brave", "ytmusic"),
    ("brave.exe", "YouTube Music - Brave", "ytmusic"),
    ("brave.exe", "Bir video başlığı - YouTube - Brave", "youtube"),
    ("chrome.exe", "Bir video başlığı - YouTube - Google Chrome", "youtube"),
    ("WhatsApp.Root.exe", "WhatsApp", "whatsapp"),
    ("brave.exe", "GitHub - Brave", None),
    ("WindowsTerminal.exe", "Windows PowerShell", None),
    # "YouTube" in a non-browser window title must not trigger the profile
    ("notepad.exe", "youtube notları.txt - Notepad", None),
])
def test_default_rules(process, title, expected):
    assert match_profile(process, title, DEFAULT_RULES) == expected


def test_first_matching_rule_wins():
    rules = [{"profile": "a", "title": "x"}, {"profile": "b", "title": "x"}]
    assert match_profile("any.exe", "x", rules) == "a"


def test_rule_without_conditions_matches_everything():
    assert match_profile("any.exe", "", [{"profile": "fallback"}]) == "fallback"


@pytest.mark.parametrize("process, title", [
    ("explorer.exe", ""),
    ("explorer.exe", "Task Switching"),
    ("explorer.exe", "Program Manager"),
    ("explorer.exe", "Run"),
    ("ShellHost.exe", "Quick settings"),
])
def test_transient_shell_windows(process, title):
    assert is_transient(process, title)


def test_file_explorer_window_is_not_transient():
    assert not is_transient("explorer.exe", "İndirilenler")


class FakeClock:
    def __init__(self):
        self.t = 0.0

    def __call__(self):
        return self.t


def test_debouncer_reports_after_hold():
    clock = FakeClock()
    d = Debouncer(0.6, clock)
    assert d.update("youtube") is False  # just seen
    clock.t = 0.5
    assert d.update("youtube") is False
    clock.t = 0.7
    assert d.update("youtube") is True
    assert d.value == "youtube"
    clock.t = 5
    assert d.update("youtube") is False  # unchanged, no re-report


def test_debouncer_ignores_quick_flicker():
    clock = FakeClock()
    d = Debouncer(0.6, clock)
    d.update("youtube")
    clock.t = 1
    assert d.update("youtube") is True
    clock.t = 1.1
    assert d.update(None) is False  # Alt+Tab passes through another window
    clock.t = 1.3
    assert d.update("youtube") is False  # back before the hold elapsed
    clock.t = 3
    assert d.update("youtube") is False
    assert d.value == "youtube"


def test_debouncer_reports_none_profile():
    clock = FakeClock()
    d = Debouncer(0.0, clock)
    assert d.update(None) is True
    assert d.value is None
