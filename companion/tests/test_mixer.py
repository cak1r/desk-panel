import pytest

from mixer import AppSession, build_channels, build_payload, display_name, parse_command


def test_display_names():
    assert display_name("brave.exe") == "Brave"
    assert display_name("WhatsApp.Root.exe") == "WhatsApp"
    assert display_name("Discord.exe") == "Discord"
    assert display_name("cs2.exe") == "Cs2"
    assert display_name("cs2.exe", {"CS2.exe": "Oyun"}) == "Oyun"
    assert display_name("brave.exe", {"brave.exe": "Tarayıcı"}) == "Tarayıcı"


def test_channels_skip_system_sounds_and_hidden():
    sessions = [
        AppSession(None, 1.0, False),
        AppSession("steam.exe", 1.0, False),
        AppSession("brave.exe", 0.64, False),
    ]
    channels = build_channels(sessions, hidden=["Steam.exe"])
    assert [c["id"] for c in channels] == ["brave.exe"]


def test_channels_merge_sessions_of_one_app():
    sessions = [
        AppSession("Discord.exe", 0.3, True),
        AppSession("Discord.exe", 0.8, False),
    ]
    (ch,) = build_channels(sessions)
    assert ch == {"id": "discord.exe", "name": "Discord", "volume": 80, "muted": False}


def test_channels_muted_only_if_all_sessions_muted():
    sessions = [AppSession("discord.exe", 0.5, True), AppSession("Discord.exe", 0.5, True)]
    assert build_channels(sessions)[0]["muted"] is True


def test_channels_sorted_by_name():
    sessions = [AppSession(p, 0.5, False) for p in ("whatsapp.root.exe", "brave.exe", "discord.exe")]
    assert [c["name"] for c in build_channels(sessions)] == ["Brave", "Discord", "WhatsApp"]


def test_payload_shape():
    out = {"name": "Hoparlör", "volume": 42, "muted": False}
    assert build_payload(out, None, []) == {"output": out, "mic": None, "apps": []}


@pytest.mark.parametrize("cmd, expected", [
    ({"target": "Brave.exe", "volume": 55}, ("brave.exe", 55, None)),
    ({"target": "@mic", "mute": True}, ("@mic", None, True)),
    ({"target": "@output", "volume": 140}, ("@output", 100, None)),
    ({"target": "@output", "volume": -5}, ("@output", 0, None)),
    ({"target": "discord.exe", "volume": 33.6, "mute": False}, ("discord.exe", 34, False)),
])
def test_parse_command_valid(cmd, expected):
    assert parse_command(cmd) == expected


@pytest.mark.parametrize("cmd", [
    {"volume": 10},
    {"target": "", "volume": 10},
    {"target": "x" * 65, "volume": 10},
    {"target": "brave.exe"},
    {"target": "brave.exe", "volume": "50"},
    {"target": "brave.exe", "volume": True},
    {"target": "brave.exe", "mute": "yes"},
])
def test_parse_command_invalid(cmd):
    with pytest.raises(ValueError):
        parse_command(cmd)
