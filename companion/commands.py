"""
Shared plumbing for panel -> PC commands. The panel publishes small JSON
objects on deskpanel/<feature>/cmd topics; each feature module validates
its own fields, this module only decodes the envelope and makes sure a bad
message can never take down paho's network thread.
"""

import json
from collections.abc import Callable

MAX_COMMAND_BYTES = 1024


def decode(payload: bytes) -> dict:
    if len(payload) > MAX_COMMAND_BYTES:
        raise ValueError(f"command too large ({len(payload)} bytes)")
    try:
        data = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"command is not valid JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ValueError("command must be a JSON object")
    return data


def make_callback(name: str, handler: Callable[[dict], None]):
    """Wrap a dict handler as a paho message callback that logs and drops
    malformed or failing commands instead of raising."""

    def _on_message(client, userdata, msg) -> None:
        try:
            handler(decode(msg.payload))
        except Exception as exc:
            print(f"{name} command rejected: {exc}")

    return _on_message
