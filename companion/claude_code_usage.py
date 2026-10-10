"""
Reads today's Claude Code CLI usage straight from its local session
transcripts (~/.claude/projects/*/*.jsonl) -- free, no API key or credit
balance needed, since Claude Code already logs this for every session on
this machine.

Each assistant turn's content blocks are logged as separate JSONL lines
but share the same message id and repeat the same usage totals -- dedupe
by message id (falling back to the entry's own uuid) so multi-block turns
aren't double-counted.
"""

import json
import os
from datetime import date, datetime
from pathlib import Path

CLAUDE_PROJECTS_DIR = Path(os.environ.get("USERPROFILE", str(Path.home()))) / ".claude" / "projects"


def _parse_local_date(timestamp: str) -> date | None:
    try:
        dt = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
    except ValueError:
        return None
    return dt.astimezone().date()


def get_today_totals() -> dict:
    today = date.today()
    seen_ids = set()
    input_tokens = 0
    output_tokens = 0
    requests = 0

    if not CLAUDE_PROJECTS_DIR.exists():
        return {"requests_today": 0, "tokens_today": 0}

    for jsonl_path in CLAUDE_PROJECTS_DIR.glob("*/*.jsonl"):
        try:
            with open(jsonl_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        entry = json.loads(line)
                    except json.JSONDecodeError:
                        continue

                    if entry.get("type") != "assistant":
                        continue
                    message = entry.get("message") or {}
                    usage = message.get("usage")
                    if not usage:
                        continue

                    timestamp = entry.get("timestamp")
                    if not timestamp or _parse_local_date(timestamp) != today:
                        continue

                    dedup_key = message.get("id") or entry.get("uuid")
                    if dedup_key in seen_ids:
                        continue
                    seen_ids.add(dedup_key)

                    input_tokens += usage.get("input_tokens", 0)
                    output_tokens += usage.get("output_tokens", 0)
                    requests += 1
        except OSError:
            continue

    return {"requests_today": requests, "tokens_today": input_tokens + output_tokens}
