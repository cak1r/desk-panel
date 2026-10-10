"""
Local MQTT connection settings for the deskpanel companion service.

Copy to config.py and fill in. config.py is gitignored.
"""

BROKER_HOST = "127.0.0.1"
BROKER_PORT = 1883
MQTT_USERNAME = "deskpanel"
MQTT_PASSWORD = "CHANGE_ME"

TOPIC_USAGE = "deskpanel/claude/usage"
TOPIC_STATUS = "deskpanel/companion/status"
TOPIC_AUDIO = "deskpanel/audio/state"
TOPIC_HEALTH = "deskpanel/pc/health"

PUBLISH_INTERVAL_S = 5
AUDIO_PUBLISH_INTERVAL_S = 1
HEALTH_PUBLISH_INTERVAL_S = 2
