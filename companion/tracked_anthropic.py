"""
Drop-in wrapper around anthropic.Anthropic() that automatically records
token usage to the desk panel's local usage store (usage_store.py) after
every real API call -- no manual record_usage() calls needed in your own
scripts.

Usage:
    from tracked_anthropic import TrackedAnthropic

    client = TrackedAnthropic()  # same constructor args as anthropic.Anthropic()
    response = client.messages.create(
        model="claude-opus-5",
        max_tokens=1024,
        messages=[{"role": "user", "content": "Hello"}],
    )

    # Streaming works the same way:
    with client.messages.stream(model="claude-opus-5", max_tokens=1024, messages=[...]) as stream:
        for text in stream.text_stream:
            print(text, end="")

Every other attribute (client.messages.count_tokens, client.models, etc.)
passes straight through to the real anthropic.Anthropic() client.
"""

import anthropic

import usage_store


def _record(response) -> None:
    usage = getattr(response, "usage", None)
    if usage is None:
        return
    tokens = (usage.input_tokens or 0) + (usage.output_tokens or 0)
    usage_store.record_usage(tokens=tokens, requests=1)


class _TrackedStreamManager:
    def __init__(self, real_manager):
        self._real_manager = real_manager
        self._stream = None

    def __enter__(self):
        self._stream = self._real_manager.__enter__()
        return self._stream

    def __exit__(self, exc_type, exc_val, exc_tb):
        result = self._real_manager.__exit__(exc_type, exc_val, exc_tb)
        if exc_type is None:
            _record(self._stream.get_final_message())
        return result


class _TrackedMessages:
    def __init__(self, real_messages):
        self._real = real_messages

    def create(self, *args, **kwargs):
        response = self._real.create(*args, **kwargs)
        _record(response)
        return response

    def stream(self, *args, **kwargs):
        return _TrackedStreamManager(self._real.stream(*args, **kwargs))

    def __getattr__(self, name):
        return getattr(self._real, name)


class TrackedAnthropic:
    def __init__(self, *args, **kwargs):
        self._client = anthropic.Anthropic(*args, **kwargs)
        self.messages = _TrackedMessages(self._client.messages)

    def __getattr__(self, name):
        return getattr(self._client, name)
