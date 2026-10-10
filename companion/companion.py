"""
Desk panel PC companion service.

Publishes Claude usage and an online/offline status (via MQTT last-will)
so the ESP32 desk panel's Claude tile has something to show. Usage is the
sum of two free sources: Claude Code CLI's own local session transcripts
(claude_code_usage.py -- no API key or credit balance needed) plus
whatever usage_store.py has recorded (manual record_usage() calls, or
TrackedAnthropic if the Anthropic API is ever used directly). Runs from
the system tray; right-click it to quit.
"""

import json
import threading
import time

import paho.mqtt.client as mqtt
from PIL import Image, ImageDraw
from pystray import Icon, Menu, MenuItem

import audio_publisher
import claude_code_usage
import config
import pc_health_publisher
import usage_store

_stop_event = threading.Event()


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
    client = build_client()
    client.loop_start()

    pub_thread = threading.Thread(target=publish_loop, args=(client,), daemon=True)
    pub_thread.start()

    audio_thread = threading.Thread(
        target=audio_publisher.audio_publish_loop, args=(client, _stop_event), daemon=True
    )
    audio_thread.start()

    health_thread = threading.Thread(
        target=pc_health_publisher.health_publish_loop, args=(client, _stop_event), daemon=True
    )
    health_thread.start()

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
