"""
Example: how to call Claude from your own scripts so the desk panel's
Claude tile picks up real usage numbers.

Just swap `anthropic.Anthropic()` for `TrackedAnthropic()` -- everything
else about the SDK is unchanged. Picks up ANTHROPIC_API_KEY from the
environment if set, otherwise falls back to anthropic_config.py.
"""

import os

from anthropic_config import API_KEY
from tracked_anthropic import TrackedAnthropic

client = TrackedAnthropic(api_key=os.environ.get("ANTHROPIC_API_KEY") or API_KEY)

response = client.messages.create(
    model="claude-opus-5",
    max_tokens=1024,
    messages=[{"role": "user", "content": "Say hello in five words."}],
)

for block in response.content:
    if block.type == "text":
        print(block.text)

print(f"Tokens used this call: {response.usage.input_tokens + response.usage.output_tokens}")
