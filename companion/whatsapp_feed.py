"""
Platform-independent half of the WhatsApp screen: turns WhatsApp desktop
toast notifications (read via the Windows notification listener) into a
compact feed for the panel, and resolves the panel's "open chat" commands
against a fixed list of pinned contacts.

There is no API for personal WhatsApp accounts; notifications are the only
supported way to see new messages. Consequences worth knowing:
  * the count is "new notifications", not the real unread count -- reading
    a chat on the phone does not clear it, but opening WhatsApp on the PC
    does (Windows drops the app's toasts);
  * nothing arrives while the WhatsApp window is focused;
  * if WhatsApp's own "show preview" setting is off, there is no text.
The panel can only open chats from PINNED_CONTACTS, never an arbitrary
number it sends -- the MQTT broker is not trusted to dial anyone.
"""

import re
from dataclasses import dataclass
from datetime import datetime

# Bidi / zero-width marks WhatsApp sprinkles around names and messages.
_INVISIBLE = re.compile("[‎‏‪-‮⁦-⁩​﻿]")
_PHONE = re.compile(r"^\+?[0-9]{8,15}$")

PREVIEW_MAX_CHARS = 80


@dataclass
class Toast:
    id: int
    created: datetime
    texts: list[str]  # ToastGeneric text elements, in order


def clean(text: str) -> str:
    return re.sub(r"\s+", " ", _INVISIBLE.sub("", text)).strip()


def parse_toast(toast: Toast, include_preview: bool = True) -> dict | None:
    """WhatsApp toasts carry [chat name, message] where group messages read
    'Sender: text'. Returns None for toasts with no usable title."""
    texts = [clean(t) for t in toast.texts if t and clean(t)]
    if not texts:
        return None
    chat = texts[0]
    body = texts[1] if len(texts) > 1 else ""
    sender = None
    if ": " in body:
        head, rest = body.split(": ", 1)
        # A group line is "Name: message"; keep very long heads as plain text
        # so a message that merely contains ': ' isn't mangled.
        if 0 < len(head) <= 40:
            sender, body = head, rest
    entry = {
        "id": toast.id,
        "chat": chat,
        "sender": sender,
        "time": toast.created.astimezone().strftime("%H:%M"),
        "ts": int(toast.created.timestamp()),
    }
    if include_preview:
        entry["text"] = body[:PREVIEW_MAX_CHARS] + ("…" if len(body) > PREVIEW_MAX_CHARS else "")
    return entry


def build_feed(toasts: list[Toast], limit: int, include_preview: bool = True) -> dict:
    """Newest first. 'chats' groups the count per chat so the panel can show
    a badge per conversation."""
    entries = [e for e in (parse_toast(t, include_preview) for t in toasts) if e]
    entries.sort(key=lambda e: e["ts"], reverse=True)
    per_chat: dict[str, int] = {}
    for e in entries:
        per_chat[e["chat"]] = per_chat.get(e["chat"], 0) + 1
    return {
        "count": len(entries),
        "chats": [{"chat": c, "count": n} for c, n in per_chat.items()],
        "items": entries[:limit],
    }


def validate_contacts(contacts: list[dict]) -> list[dict]:
    """PINNED_CONTACTS entries: {"id": "anne", "name": "Annem", "phone": "905xxxxxxxxx"}.
    Raises ValueError on a malformed entry so config mistakes surface at startup."""
    seen = set()
    for c in contacts:
        cid, name, phone = c.get("id"), c.get("name"), c.get("phone")
        if not isinstance(cid, str) or not re.fullmatch(r"[a-z0-9_-]{1,32}", cid or ""):
            raise ValueError(f"contact id must be 1-32 chars of a-z0-9_-: {cid!r}")
        if cid in seen:
            raise ValueError(f"duplicate contact id: {cid}")
        if not isinstance(name, str) or not name:
            raise ValueError(f"contact {cid} needs a name")
        if not isinstance(phone, str) or not _PHONE.match(phone):
            raise ValueError(f"contact {cid} phone must be digits with country code, e.g. 905xxxxxxxxx")
        seen.add(cid)
    return contacts


def public_contacts(contacts: list[dict]) -> list[dict]:
    """What the panel gets to see -- ids and names, never phone numbers."""
    return [{"id": c["id"], "name": c["name"]} for c in contacts]


def chat_uri(contacts: list[dict], contact_id: str) -> str:
    for c in contacts:
        if c["id"] == contact_id:
            return f"whatsapp://send/?phone={c['phone'].lstrip('+')}"
    raise ValueError(f"unknown contact id: {contact_id!r}")


def parse_command(cmd: dict, contacts: list[dict]) -> str:
    """{"action": "open_chat", "contact": "<id>"} or {"action": "focus"} -> URI to launch."""
    action = cmd.get("action")
    if action == "focus":
        return "whatsapp://"
    if action == "open_chat":
        contact = cmd.get("contact")
        if not isinstance(contact, str):
            raise ValueError("open_chat needs a contact id")
        return chat_uri(contacts, contact)
    raise ValueError(f"unknown whatsapp action: {action!r}")
