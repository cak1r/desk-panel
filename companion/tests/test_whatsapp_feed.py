from datetime import datetime, timezone

import pytest

from whatsapp_feed import (
    PREVIEW_MAX_CHARS,
    Toast,
    build_feed,
    chat_uri,
    clean,
    parse_command,
    parse_toast,
    public_contacts,
    validate_contacts,
)

LRM = "‎"
CONTACTS = [
    {"id": "anne", "name": "Annem", "phone": "905001112233"},
    {"id": "is-grubu", "name": "İş grubu", "phone": "+905004445566"},
]


def toast(texts, minute=0, id_=1) -> Toast:
    return Toast(id=id_, created=datetime(2026, 10, 10, 10, minute, tzinfo=timezone.utc), texts=texts)


def test_clean_strips_bidi_marks_and_collapses_space():
    assert clean(f"Ayşe{LRM}:  merhaba{LRM} ") == "Ayşe: merhaba"


def test_group_message_splits_sender():
    # Shape seen on the real PC: [chat, "Sender‎: text‎"]
    e = parse_toast(toast(["Aile grubu", f"Ayşe{LRM}: akşam yemeği hazır{LRM}"]))
    assert e["chat"] == "Aile grubu"
    assert e["sender"] == "Ayşe"
    assert e["text"] == "akşam yemeği hazır"
    assert e["ts"] == int(datetime(2026, 10, 10, 10, 0, tzinfo=timezone.utc).timestamp())
    assert e["time"] == datetime(2026, 10, 10, 10, 0, tzinfo=timezone.utc).astimezone().strftime("%H:%M")


def test_direct_message_has_no_sender():
    e = parse_toast(toast(["Ayşe", "geliyorum"]))
    assert e["sender"] is None
    assert e["text"] == "geliyorum"


def test_colon_inside_long_message_is_not_a_sender():
    body = "Toplantı notu şu şekilde olacak ve uzun bir cümle: saat üçte"
    assert parse_toast(toast(["Ayşe", body]))["sender"] is None


def test_title_only_toast():
    e = parse_toast(toast(["Ayşe"]))
    assert e["chat"] == "Ayşe"
    assert e["text"] == ""


def test_empty_toast_is_dropped():
    assert parse_toast(toast([])) is None
    assert parse_toast(toast([LRM, "  "])) is None


def test_preview_can_be_disabled():
    assert "text" not in parse_toast(toast(["Ayşe", "gizli"]), include_preview=False)


def test_preview_is_truncated():
    e = parse_toast(toast(["Ayşe", "a" * 200]))
    assert e["text"] == "a" * PREVIEW_MAX_CHARS + "…"


def test_feed_newest_first_with_counts_and_limit():
    toasts = [
        toast(["Aile grubu", "Ayşe: 1"], minute=1, id_=1),
        toast(["Mehmet", "2"], minute=5, id_=2),
        toast(["Aile grubu", "Ali: 3"], minute=9, id_=3),
        toast([], minute=10, id_=4),
    ]
    feed = build_feed(toasts, limit=2)
    assert feed["count"] == 3
    assert [i["id"] for i in feed["items"]] == [3, 2]
    assert {c["chat"]: c["count"] for c in feed["chats"]} == {"Aile grubu": 2, "Mehmet": 1}


def test_empty_feed():
    assert build_feed([], limit=6) == {"count": 0, "chats": [], "items": []}


def test_validate_contacts_accepts_config():
    assert validate_contacts(CONTACTS) == CONTACTS
    assert validate_contacts([]) == []


@pytest.mark.parametrize("bad", [
    [{"id": "Anne", "name": "x", "phone": "905001112233"}],  # uppercase id
    [{"id": "anne", "name": "", "phone": "905001112233"}],
    [{"id": "anne", "name": "x", "phone": "0500 111 22 33"}],
    [{"id": "anne", "name": "x", "phone": "905001112233; rm"}],
    [{"id": "anne", "name": "x", "phone": "905001112233"}, {"id": "anne", "name": "y", "phone": "905001112234"}],
])
def test_validate_contacts_rejects(bad):
    with pytest.raises(ValueError):
        validate_contacts(bad)


def test_public_contacts_hide_phone_numbers():
    assert public_contacts(CONTACTS) == [{"id": "anne", "name": "Annem"}, {"id": "is-grubu", "name": "İş grubu"}]


def test_chat_uri():
    assert chat_uri(CONTACTS, "anne") == "whatsapp://send/?phone=905001112233"
    assert chat_uri(CONTACTS, "is-grubu") == "whatsapp://send/?phone=905004445566"
    with pytest.raises(ValueError):
        chat_uri(CONTACTS, "yok")


def test_commands():
    assert parse_command({"action": "focus"}, CONTACTS) == "whatsapp://"
    assert parse_command({"action": "open_chat", "contact": "anne"}, CONTACTS).endswith("905001112233")


@pytest.mark.parametrize("cmd", [
    {"action": "open_chat"},
    {"action": "open_chat", "contact": "905009999999"},  # raw numbers are never dialled
    {"action": "send_message", "contact": "anne"},
    {},
])
def test_commands_rejected(cmd):
    with pytest.raises(ValueError):
        parse_command(cmd, CONTACTS)
