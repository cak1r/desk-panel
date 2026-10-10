"""
One-shot capability probe for the planned panel screens. Run it on the
Windows PC (not WSL) while something plays in Brave, and it reports which
data sources actually work on this machine:

  1. SMTC (now playing): source app, title/artist, timeline, thumbnail
  2. Per-app audio sessions + mic state (pycaw)
  3. Foreground window detection (YouTube auto-profile)
  4. Windows notification listener (WhatsApp unread feed)
  5. whatsapp:// URI handler registration

Nothing is published to MQTT. A summary is written to probe_out/report.json
and the SMTC thumbnail (if any) to probe_out/.

Usage:  python probe.py            (start YouTube Music in Brave first, keep it PLAYING)
        python probe.py --no-wait  (skip the foreground-window countdown)
"""

import asyncio
import ctypes
import ctypes.wintypes
import io
import json
import sys
import time
import winreg
from pathlib import Path

import psutil
from PIL import Image

OUT_DIR = Path(__file__).parent / "probe_out"
PREVIEW_CHARS = 60
FOREGROUND_WATCH_S = 15

report: dict = {}


def ok(msg: str) -> None:
    print(f"  [OK]   {msg}")


def warn(msg: str) -> None:
    print(f"  [??]   {msg}")


def fail(msg: str) -> None:
    print(f"  [YOK]  {msg}")


def section(title: str) -> None:
    print(f"\n=== {title} ===")


def seconds(value) -> float | None:
    """WinRT TimeSpan arrives as datetime.timedelta on current pywinrt,
    as a raw 100ns-tick struct on older builds."""
    if value is None:
        return None
    if hasattr(value, "total_seconds"):
        return round(value.total_seconds(), 1)
    if hasattr(value, "duration"):
        return round(value.duration / 10_000_000, 1)
    return None


# --------------------------------------------------------------------------
# 1. SMTC
# --------------------------------------------------------------------------

async def read_thumbnail(thumb_ref) -> bytes:
    from winrt.windows.storage.streams import Buffer, InputStreamOptions

    stream = await thumb_ref.open_read_async()
    size = stream.size
    buf = Buffer(size)
    result = await stream.read_async(buf, size, InputStreamOptions.READ_AHEAD)
    return bytes(memoryview(result))[: result.length]


def timeline_snapshot(session) -> dict:
    tl = session.get_timeline_properties()
    return {
        "position_s": seconds(tl.position),
        "end_s": seconds(tl.end_time),
        "last_updated": str(tl.last_updated_time) if tl.last_updated_time else None,
    }


async def probe_smtc() -> None:
    section("1. Şimdi çalan (SMTC)")
    r = report.setdefault("smtc", {})
    try:
        from winrt.windows.media.control import (
            GlobalSystemMediaTransportControlsSessionManager as MediaManager,
        )
    except ImportError as exc:
        fail(f"winrt paketleri yüklü değil: {exc}  ->  pip install -r requirements.txt")
        r["error"] = str(exc)
        return

    manager = await MediaManager.request_async()
    sessions = list(manager.get_sessions())
    r["sessions"] = [s.source_app_user_model_id for s in sessions]
    if not sessions:
        fail("Hiç medya oturumu yok -- Brave'de bir şey çalıyor mu? brave://flags'te hardware-media-key-handling açık mı?")
        return
    ok(f"{len(sessions)} oturum: {', '.join(r['sessions'])}")

    current = manager.get_current_session()
    brave = [s for s in sessions if "brave" in s.source_app_user_model_id.lower()]
    session = brave[0] if brave else current
    if not brave:
        warn("Brave oturumu bulunamadı, Windows'un 'geçerli' oturumu kullanılıyor")
    r["chosen"] = session.source_app_user_model_id
    print(f"  İncelenen oturum: {r['chosen']}")

    props = await session.try_get_media_properties_async()
    r["title"] = props.title
    r["artist"] = props.artist
    r["album"] = props.album_title
    (ok if props.title else fail)(f"Başlık: {props.title!r}")
    (ok if props.artist else warn)(f"Sanatçı/kanal: {props.artist!r}")
    (ok if props.album_title else warn)(f"Albüm: {props.album_title!r}")

    playback = session.get_playback_info()
    r["status"] = getattr(playback.playback_status, "name", str(playback.playback_status))
    ok(f"Durum: {r['status']}")
    ctrl = playback.controls
    r["controls"] = {
        "play_pause": ctrl.is_play_pause_toggle_enabled,
        "next": ctrl.is_next_enabled,
        "previous": ctrl.is_previous_enabled,
        "seek": ctrl.is_playback_position_enabled,
        "shuffle": ctrl.is_shuffle_enabled,
        "repeat": ctrl.is_repeat_enabled,
    }
    print(f"  Desteklenen kontroller: {r['controls']}")

    # Browsers tend to report position only on state changes; sample twice
    # to see whether it advances on its own or must be extrapolated.
    first = timeline_snapshot(session)
    print(f"  Zaman çizelgesi #1: {first}")
    await asyncio.sleep(3)
    second = timeline_snapshot(session)
    print(f"  Zaman çizelgesi #2: {second}")
    r["timeline"] = [first, second]
    if not first["end_s"]:
        fail("Süre bilgisi yok -- ilerleme çubuğu gösterilemez")
    elif first["position_s"] != second["position_s"]:
        ok("Konum kendiliğinden ilerliyor")
    elif first["last_updated"] and r["status"] == "PLAYING":
        warn("Konum 3 sn'de değişmedi -- panel last_updated'dan tahmin ederek ilerletmeli (yine de kullanılabilir)")
    else:
        warn("Konum değişmedi (çalmıyor olabilir)")

    if props.thumbnail is None:
        fail("Kapak resmi yok")
        r["thumbnail"] = None
        return
    try:
        data = await read_thumbnail(props.thumbnail)
        img = Image.open(io.BytesIO(data))
        OUT_DIR.mkdir(exist_ok=True)
        path = OUT_DIR / f"thumbnail.{(img.format or 'bin').lower()}"
        path.write_bytes(data)
        r["thumbnail"] = {"format": img.format, "size": img.size, "bytes": len(data)}
        ok(f"Kapak: {img.format} {img.size[0]}x{img.size[1]}, {len(data)} bayt -> {path}")
    except Exception as exc:
        fail(f"Kapak okunamadı: {exc}")
        r["thumbnail"] = {"error": str(exc)}


# --------------------------------------------------------------------------
# 2. Audio sessions
# --------------------------------------------------------------------------

def probe_audio() -> None:
    section("2. Uygulama başına ses (pycaw)")
    r = report.setdefault("audio", {})
    import comtypes
    from pycaw.pycaw import AudioUtilities

    comtypes.CoInitialize()
    try:
        apps = []
        for s in AudioUtilities.GetAllSessions():
            name = s.Process.name() if s.Process else "(sistem sesleri)"
            vol = s.SimpleAudioVolume
            apps.append({
                "app": name,
                "volume": round(vol.GetMasterVolume() * 100),
                "muted": bool(vol.GetMute()),
            })
        r["sessions"] = apps
        for a in apps:
            print(f"    {a['app']:<28} %{a['volume']:<4} {'sessiz' if a['muted'] else ''}")
        names = {a["app"].lower() for a in apps}
        (ok if "brave.exe" in names else warn)("brave.exe oturumu " + ("var" if "brave.exe" in names else "yok (ses çalarken tekrar dene)"))
        (ok if any("whatsapp" in n for n in names) else warn)(
            "WhatsApp oturumu " + ("var" if any("whatsapp" in n for n in names) else "yok -- yalnızca ses çaldığında/çağrıdayken görünür")
        )

        for label, getter in (("Hoparlör", AudioUtilities.GetSpeakers), ("Mikrofon", AudioUtilities.GetMicrophone)):
            try:
                dev = getter()
                # Some pycaw versions return the wrapped AudioDevice from
                # GetSpeakers() but a raw IMMDevice from GetMicrophone().
                if not hasattr(dev, "EndpointVolume"):
                    dev = AudioUtilities.CreateDevice(dev)
                ep = dev.EndpointVolume
                info = {
                    "name": getattr(dev, "FriendlyName", None),
                    "volume": round(ep.GetMasterVolumeLevelScalar() * 100),
                    "muted": bool(ep.GetMute()),
                }
                r[label.lower()] = info
                ok(f"{label}: {info['name']!r} %{info['volume']} {'(sessiz)' if info['muted'] else ''}")
            except Exception as exc:
                fail(f"{label} okunamadı: {exc}")
                r[label.lower()] = {"error": str(exc)}
    finally:
        comtypes.CoUninitialize()


# --------------------------------------------------------------------------
# 3. Foreground window
# --------------------------------------------------------------------------

def foreground_window() -> tuple[str, str]:
    user32 = ctypes.windll.user32
    hwnd = user32.GetForegroundWindow()
    length = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, buf, length + 1)
    pid = ctypes.wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    try:
        proc = psutil.Process(pid.value).name()
    except psutil.Error:
        proc = "?"
    return buf.value, proc


def probe_foreground(wait: bool) -> None:
    section("3. Öndeki pencere (otomatik profil)")
    # Watch for a while and log every foreground change, the same way the
    # companion would drive auto-profiles, instead of a single snapshot.
    seen: list[dict] = []
    duration = FOREGROUND_WATCH_S if wait else 0
    if wait:
        print(f"  {duration} sn boyunca öndeki pencere izleniyor -- Brave'de YouTube / YouTube Music sekmelerine tıkla...")
    deadline = time.monotonic() + duration
    while True:
        title, proc = foreground_window()
        if not seen or seen[-1]["title"] != title:
            seen.append({"title": title, "process": proc})
            print(f"    {proc:<22} {title!r}")
        if time.monotonic() >= deadline:
            break
        time.sleep(0.3)
    report["foreground"] = seen
    yt = [s for s in seen if "youtube" in s["title"].lower()]
    if yt:
        ok(f"YouTube algılandı ({yt[0]['process']}) -- otomatik profil başlıktan çalışır")
    else:
        warn("Hiçbir pencere başlığında 'YouTube' görülmedi")


# --------------------------------------------------------------------------
# 4. Notification listener
# --------------------------------------------------------------------------

async def probe_notifications() -> None:
    section("4. Windows bildirimleri (WhatsApp)")
    r = report.setdefault("notifications", {})
    try:
        from winrt.windows.ui.notifications import KnownNotificationBindings, NotificationKinds
        from winrt.windows.ui.notifications.management import (
            UserNotificationListener,
            UserNotificationListenerAccessStatus,
        )
    except ImportError as exc:
        fail(f"winrt paketleri yüklü değil: {exc}")
        r["error"] = str(exc)
        return

    try:
        listener = UserNotificationListener.current
        status = await listener.request_access_async()
    except Exception as exc:
        # The likely failure mode for an unpackaged Python process.
        fail(f"Dinleyici başlatılamadı: {exc}")
        r["error"] = str(exc)
        return

    r["access"] = getattr(status, "name", str(status))
    if status != UserNotificationListenerAccessStatus.ALLOWED:
        fail(f"Erişim: {r['access']} -- Ayarlar > Gizlilik > Bildirimler'den izin verip tekrar dene")
        return
    ok("Erişim izni var")

    notifs = await listener.get_notifications_async(NotificationKinds.TOAST)
    per_app: dict[str, int] = {}
    whatsapp = []
    for n in notifs:
        try:
            app = n.app_info.display_info.display_name
        except Exception:
            app = "(bilinmiyor)"
        per_app[app] = per_app.get(app, 0) + 1
        if "whatsapp" in app.lower():
            texts = []
            try:
                binding = n.notification.visual.get_binding(KnownNotificationBindings.toast_generic)
                texts = [t.text[:PREVIEW_CHARS] for t in binding.get_text_elements()]
            except Exception as exc:
                texts = [f"(metin okunamadı: {exc})"]
            whatsapp.append({"time": str(n.creation_time), "texts": texts})

    r["per_app"] = per_app
    r["whatsapp_count"] = len(whatsapp)
    print(f"  Bildirim merkezindeki bildirimler: {per_app or 'hiç yok'}")
    if whatsapp:
        ok(f"{len(whatsapp)} WhatsApp bildirimi okundu:")
        for w in whatsapp:
            print(f"    {w['time']}  {w['texts']}")
    else:
        warn("WhatsApp bildirimi yok -- WhatsApp penceresi kapalıyken kendine mesaj attırıp tekrar dene")


# --------------------------------------------------------------------------
# 5. whatsapp:// handler
# --------------------------------------------------------------------------

def probe_whatsapp_uri() -> None:
    section("5. whatsapp:// bağlantısı")
    try:
        with winreg.OpenKey(winreg.HKEY_CLASSES_ROOT, "whatsapp"):
            pass
        report["whatsapp_uri"] = True
        ok("whatsapp:// kayıtlı. Elle dene: Win+R -> whatsapp://send/?phone=90XXXXXXXXXX")
    except OSError:
        report["whatsapp_uri"] = False
        fail("whatsapp:// kayıtlı değil (WhatsApp masaüstü kurulu mu?)")


async def main() -> None:
    if sys.platform != "win32":
        print("Bu script Windows'ta çalışmalı (WSL'de değil).")
        sys.exit(1)

    await probe_smtc()
    probe_audio()
    probe_foreground(wait="--no-wait" not in sys.argv)
    await probe_notifications()
    probe_whatsapp_uri()

    OUT_DIR.mkdir(exist_ok=True)
    out = OUT_DIR / "report.json"
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False, default=str), encoding="utf-8")
    print(f"\nRapor: {out}")


if __name__ == "__main__":
    asyncio.run(main())
