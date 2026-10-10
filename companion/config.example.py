"""
Local MQTT connection settings for the deskpanel companion service.

Copy to config.py and fill in. config.py is gitignored. When updating from
an older checkout, the companion lists any setting your config.py is
missing at startup -- copy those over from this file.
"""

from profiles import DEFAULT_RULES

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

# --- Now playing (Windows SMTC) ---------------------------------------------
TOPIC_MEDIA_NOWPLAYING = "deskpanel/media/nowplaying"
TOPIC_MEDIA_ART = "deskpanel/media/art"
TOPIC_MEDIA_CMD = "deskpanel/media/cmd"
MEDIA_POLL_INTERVAL_S = 1
MEDIA_RESYNC_S = 5  # republish position at least this often
MEDIA_ART_SIZE = 168  # px, square; RGB565 -> 168*168*2 = 56 KB per cover
# Sessions to prefer when several players are open (substring of the SMTC app id).
MEDIA_PREFERRED_APPS = ["brave", "chrome", "spotify"]

# --- Per-app volume mixer ---------------------------------------------------
TOPIC_MIXER_STATE = "deskpanel/mixer/state"
TOPIC_MIXER_CMD = "deskpanel/mixer/cmd"
MIXER_POLL_INTERVAL_S = 0.5
MIXER_RESYNC_S = 10
MIXER_DISPLAY_NAMES = {}  # e.g. {"cs2.exe": "Oyun"}; merged over mixer.DEFAULT_DISPLAY_NAMES
MIXER_HIDDEN_APPS = []  # process names to leave off the panel, e.g. ["steam.exe"]

# --- Foreground app -> panel profile ----------------------------------------
TOPIC_PROFILE = "deskpanel/profile"
PROFILE_POLL_INTERVAL_S = 0.3
PROFILE_DEBOUNCE_S = 0.6
PROFILE_RULES = DEFAULT_RULES  # see profiles.py for the rule format

# --- WhatsApp (notifications + pinned chats) ---------------------------------
TOPIC_WHATSAPP_FEED = "deskpanel/whatsapp/feed"
TOPIC_WHATSAPP_CONTACTS = "deskpanel/whatsapp/contacts"
TOPIC_WHATSAPP_CMD = "deskpanel/whatsapp/cmd"
WHATSAPP_POLL_INTERVAL_S = 2
WHATSAPP_FEED_LIMIT = 6
# Message previews travel over MQTT to the panel. Set False to send only
# chat names, senders and times.
WHATSAPP_INCLUDE_PREVIEW = True
# Chats the panel may open. Phone numbers stay on the PC; the panel only
# ever sees id + name. Phone: digits with country code, no spaces.
WHATSAPP_PINNED_CONTACTS = [
    # {"id": "anne", "name": "Annem", "phone": "905xxxxxxxxx"},
]
