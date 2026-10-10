"""
Publishes the WhatsApp desktop notifications currently sitting in the
Windows notification center, and opens pinned chats on request. See
whatsapp_feed.py for what this can and cannot know.

Requires notification access: on first run Windows asks; if it was denied,
enable it under Settings > Privacy & security > Notifications.

Topics (see config.example.py):
  TOPIC_WHATSAPP_FEED      JSON, retained -- see whatsapp_feed.build_feed
  TOPIC_WHATSAPP_CONTACTS  JSON, retained: [{"id", "name"}] (no phone numbers)
  TOPIC_WHATSAPP_CMD       panel -> PC: {"action": "open_chat", "contact": "<id>"}
                           or {"action": "focus"}
"""

import asyncio
import json
import os
import threading

import config
from whatsapp_feed import Toast, build_feed, parse_command, public_contacts, validate_contacts


class WhatsAppPublisher:
    def __init__(self, client, stop_event: threading.Event):
        self._client = client
        self._stop = stop_event
        self._contacts = validate_contacts(config.WHATSAPP_PINNED_CONTACTS)

    def run(self) -> None:
        asyncio.run(self._main())

    def handle_command(self, cmd: dict) -> None:
        uri = parse_command(cmd, self._contacts)
        os.startfile(uri)  # hands the URI to WhatsApp desktop, which also brings it to front

    def publish_contacts(self) -> None:
        self._client.publish(config.TOPIC_WHATSAPP_CONTACTS, json.dumps(public_contacts(self._contacts), ensure_ascii=False),
                             qos=1, retain=True)

    async def _main(self) -> None:
        from winrt.windows.ui.notifications import KnownNotificationBindings, NotificationKinds
        from winrt.windows.ui.notifications.management import (
            UserNotificationListener,
            UserNotificationListenerAccessStatus,
        )

        # Pinned chats work through the URI handler alone, so they are
        # published even if notification access ends up denied.
        self.publish_contacts()

        listener = UserNotificationListener.current
        status = await listener.request_access_async()
        if status != UserNotificationListenerAccessStatus.ALLOWED:
            print("WhatsApp feed disabled: notification access not granted "
                  "(Settings > Privacy & security > Notifications)")
            return

        last = None
        while not self._stop.is_set():
            try:
                toasts = []
                for n in await listener.get_notifications_async(NotificationKinds.TOAST):
                    try:
                        app = n.app_info.display_info.display_name
                    except Exception:
                        continue
                    if "whatsapp" not in app.lower():
                        continue
                    binding = n.notification.visual.get_binding(KnownNotificationBindings.toast_generic)
                    texts = [t.text for t in binding.get_text_elements()] if binding else []
                    toasts.append(Toast(id=n.id, created=n.creation_time, texts=texts))

                feed = build_feed(toasts, config.WHATSAPP_FEED_LIMIT, config.WHATSAPP_INCLUDE_PREVIEW)
                if feed != last:
                    self._client.publish(config.TOPIC_WHATSAPP_FEED, json.dumps(feed, ensure_ascii=False),
                                         qos=1, retain=True)
                    last = feed
            except Exception as exc:
                print(f"WhatsApp publish error: {exc}")
            await asyncio.sleep(config.WHATSAPP_POLL_INTERVAL_S)
