"""
Desk panel PC companion service.

Publishes Claude usage and an online/offline status (via MQTT last-will)
so the ESP32 desk panel's Claude tile has something to show. Usage is the
sum of two free sources: Claude Code CLI's own local session transcripts
(claude_code_usage.py -- no API key or credit balance needed) plus
whatever usage_store.py has recorded (manual record_usage() calls, or
TrackedAnthropic if the Anthropic API is ever used directly).

Also publishes what is playing (media_publisher), per-app volumes
(mixer_publisher), the foreground app's panel profile
(foreground_publisher) and WhatsApp notifications (whatsapp_publisher),
and executes the commands the panel sends back for those. Runs from the
system tray; right-click it to quit.
"""

import json
import sys
import threading
import time

import paho.mqtt.client as mqtt
from PIL import Image, ImageDraw
from pystray import Icon, Menu, MenuItem

import audio_publisher
import claude_code_usage
import commands
import config
import foreground_publisher
import pc_health_publisher
import settings_check
import usage_store
from media_publisher import MediaPublisher
from mixer_publisher import MixerPublisher
from whatsapp_publisher import WhatsAppPublisher

_stop_event = threading.Event()
_command_topics: list[str] = []


def make_icon_image() -> Image.Image:
    size = 64
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.ellipse((4, 4, size - 4, size - 4), fill=(53, 212, 164, 255))  # accent teal-green
    return img


def on_connect(client, userdata, connect_flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to broker")
        client.publish(config.TOPIC_STATUS, "online", qos=1, retain=True)
        # (Re)subscribe on every connect -- subscriptions don't survive a
        # reconnect with a clean session.
        for topic in _command_topics:
            client.subscribe(topic, qos=1)
    else:
        print(f"Connect failed: {reason_code}")


def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    print(f"Disconnected: {reason_code}")


def publish_loop(client: mqtt.Client) -> None:
    while not _stop_event.is_set():
        code_usage = claude_code_usage.get_today_totals()
        manual_usage = usage_store.get_today()
        totals = {
            "requests_today": code_usage["requests_today"] + manual_usage["requests_today"],
            "tokens_today": code_usage["tokens_today"] + manual_usage["tokens_today"],
        }
        client.publish(config.TOPIC_USAGE, json.dumps(totals), qos=1, retain=True)
        _stop_event.wait(config.PUBLISH_INTERVAL_S)


def build_client() -> mqtt.Client:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="deskpanel-companion")
    client.username_pw_set(config.MQTT_USERNAME, config.MQTT_PASSWORD)
    client.will_set(config.TOPIC_STATUS, "offline", qos=1, retain=True)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.connect(config.BROKER_HOST, config.BROKER_PORT, keepalive=30)
    return client


def main() -> None:
    missing = settings_check.missing_settings(config)
    if missing:
        print("config.py is missing settings added in config.example.py -- copy these over:")
        for name in missing:
            print(f"  {name}")
        sys.exit(1)

    client = build_client()

    media = MediaPublisher(client, _stop_event)
    mixer = MixerPublisher(client, _stop_event)
    whatsapp = WhatsAppPublisher(client, _stop_event)
    for topic, name, publisher in (
        (config.TOPIC_MEDIA_CMD, "media", media),
        (config.TOPIC_MIXER_CMD, "mixer", mixer),
        (config.TOPIC_WHATSAPP_CMD, "whatsapp", whatsapp),
    ):
        client.message_callback_add(topic, commands.make_callback(name, publisher.handle_command))
        _command_topics.append(topic)

    client.loop_start()

    workers = [
        (publish_loop, (client,)),
        (audio_publisher.audio_publish_loop, (client, _stop_event)),
        (pc_health_publisher.health_publish_loop, (client, _stop_event)),
        (foreground_publisher.foreground_publish_loop, (client, _stop_event)),
        (media.run, ()),
        (mixer.run, ()),
        (whatsapp.run, ()),
    ]
    for target, args in workers:
        threading.Thread(target=target, args=args, daemon=True).start()

    def on_quit(icon: Icon, item: MenuItem) -> None:
        _stop_event.set()
        client.publish(config.TOPIC_STATUS, "offline", qos=1, retain=True)
        time.sleep(0.2)
        client.loop_stop()
        client.disconnect()
        icon.stop()

    icon = Icon(
        "deskpanel-companion",
        make_icon_image(),
        "Desk Panel Companion",
        menu=Menu(MenuItem("Quit", on_quit)),
    )
    icon.run()


if __name__ == "__main__":
    main()
