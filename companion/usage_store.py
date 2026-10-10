"""
Tiny local store for today's Claude request/token counts.

Any script that calls the Claude API can bump these counters by importing
this module and calling record_usage(), or by shelling out to
record_usage.py. The companion service just reads get_today() on a timer
and publishes it over MQTT -- it doesn't know or care how the numbers were
produced.
"""

import json
import os
from datetime import date
from pathlib import Path

STORE_PATH = Path(os.environ.get("LOCALAPPDATA", Path.home())) / "DeskPanel" / "usage.json"

_EMPTY = {"date": "", "requests_today": 0, "tokens_today": 0}


def _load() -> dict:
    try:
        with open(STORE_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return dict(_EMPTY)

    today = date.today().isoformat()
    if data.get("date") != today:
        return {"date": today, "requests_today": 0, "tokens_today": 0}
    return data


def _save(data: dict) -> None:
    STORE_PATH.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = STORE_PATH.with_suffix(".tmp")
    with open(tmp_path, "w", encoding="utf-8") as f:
        json.dump(data, f)
    tmp_path.replace(STORE_PATH)


def get_today() -> dict:
    """Returns {"requests_today": int, "tokens_today": int} for today, resetting if the date rolled over."""
    data = _load()
    return {"requests_today": data["requests_today"], "tokens_today": data["tokens_today"]}


def record_usage(tokens: int = 0, requests: int = 1) -> dict:
    """Adds to today's counters (resetting first if the date rolled over) and returns the new totals."""
    data = _load()
    data["date"] = date.today().isoformat()
    data["requests_today"] += requests
    data["tokens_today"] += tokens
    _save(data)
    return get_today()
