import types

import pytest

import commands
import settings_check


def test_decode_valid():
    assert commands.decode(b'{"action": "next"}') == {"action": "next"}


@pytest.mark.parametrize("payload", [b"not json", b"[1, 2]", b"\xff\xfe", b"{" + b" " * 2000 + b"}"])
def test_decode_invalid(payload):
    with pytest.raises(ValueError):
        commands.decode(payload)


def test_callback_passes_decoded_dict():
    seen = []
    cb = commands.make_callback("test", seen.append)
    cb(None, None, types.SimpleNamespace(payload=b'{"a": 1}'))
    assert seen == [{"a": 1}]


def test_callback_swallows_bad_payload_and_handler_errors(capsys):
    def boom(cmd):
        raise ValueError("nope")

    cb = commands.make_callback("test", boom)
    cb(None, None, types.SimpleNamespace(payload=b"garbage"))
    cb(None, None, types.SimpleNamespace(payload=b"{}"))
    out = capsys.readouterr().out
    assert out.count("test command rejected") == 2


def test_example_config_names_found():
    names = settings_check.example_setting_names()
    for expected in ("BROKER_HOST", "TOPIC_MEDIA_CMD", "MIXER_HIDDEN_APPS", "WHATSAPP_PINNED_CONTACTS"):
        assert expected in names


def test_missing_settings_reported(tmp_path):
    example = tmp_path / "config.example.py"
    example.write_text("A = 1\nB: int = 2\n_private = 3\nlower = 4\n", encoding="utf-8")
    cfg = types.SimpleNamespace(A=1)
    assert settings_check.missing_settings(cfg, example) == ["B"]


def test_example_config_is_importable_and_consistent():
    import importlib.util

    spec = importlib.util.spec_from_file_location("config_example", settings_check.EXAMPLE_PATH)
    cfg = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cfg)

    topics = [getattr(cfg, n) for n in dir(cfg) if n.startswith("TOPIC_")]
    assert all(t.startswith("deskpanel/") for t in topics)
    assert len(topics) == len(set(topics)), "topics must be unique"
    assert settings_check.missing_settings(cfg) == []

    from whatsapp_feed import validate_contacts
    validate_contacts(cfg.WHATSAPP_PINNED_CONTACTS)
