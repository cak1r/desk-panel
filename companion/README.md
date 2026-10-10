# Desk Panel Companion

Windows'ta sistem tepsisinde çalışan küçük bir Python servisi. PC'deki
durumu (çalan şarkı, uygulama sesleri, öndeki uygulama, WhatsApp
bildirimleri, Claude kullanımı, CPU/RAM) MQTT üzerinden panele yayınlar ve
panelden gelen komutları (oynat/duraklat, ses sürgüsü, sohbet aç) uygular.

Klavye kısayolu, makro ve medya tuşları companion gerektirmez: panel bunları
doğrudan USB HID olarak gönderir. Companion yalnızca PC'den **okunması**
gereken veriler ve belirli bir uygulamayı hedefleyen komutlar için var.

## Özellikler

| Özellik | Kaynak | Modül |
|---|---|---|
| Şimdi çalan: başlık, sanatçı, albüm, ilerleme, kapak | Windows medya oturumu (SMTC) | `media_publisher.py`, `media_session.py` |
| Oynat/duraklat, ileri/geri, ±10 sn sarma | SMTC | aynı |
| Uygulama başına ses + sessize alma, çıkış cihazı, mikrofon | Windows ses oturumları (pycaw) | `mixer_publisher.py`, `mixer.py` |
| Öndeki uygulamaya göre panel profili (YouTube, YT Music, WhatsApp) | Win32 ön plan penceresi | `foreground_publisher.py`, `profiles.py` |
| WhatsApp yeni bildirimleri, sabit sohbetleri açma | Windows bildirim dinleyicisi + `whatsapp://` | `whatsapp_publisher.py`, `whatsapp_feed.py` |
| Ana ses seviyesi | pycaw | `audio_publisher.py` |
| CPU / RAM / GPU | psutil, pynvml | `pc_health_publisher.py` |
| Claude Code kullanımı (bugünkü istek/token) | `~/.claude/projects/*.jsonl` | `claude_code_usage.py`, `usage_store.py` |

Her özellik iki parçaya ayrılmıştır: Windows API'lerine dokunan `*_publisher.py`
ve saf mantığı içeren, her işletim sisteminde test edilebilen modül.

## Gereksinimler

- Windows 10 / 11
- **python.org'dan Python 3.11+** — MSYS2 / MinGW Python'u `winrt` paketlerini kuramaz.
  Birden fazla Python varsa `py -3.12` ile doğru olanı seçin.
- Bir MQTT broker'ı (Mosquitto ya da Home Assistant'ın Mosquitto eklentisi)
- Tarayıcıda medya entegrasyonu açık olmalı: Brave/Chrome'da
  `brave://flags/#hardware-media-key-handling` **Enabled** kalmalı, kapatılırsa
  tarayıcı şarkı bilgisini Windows'a vermez.

## Kurulum

PowerShell'de:

```powershell
cd <repo>\companion
py -3.12 -m venv $HOME\deskpanel-venv
$py = "$HOME\deskpanel-venv\Scripts\python.exe"
& $py -m pip install -r requirements.txt
Copy-Item config.example.py config.py   # sonra config.py'yi düzenleyin
& $py companion.py
```

Repo WSL içindeyse yol `\\wsl.localhost\<dağıtım>\home\<kullanıcı>\...` şeklindedir
(dağıtım adını `wsl -l` gösterir). Sanal ortamı Windows diskinde tutmak
(`$HOME\deskpanel-venv`) WSL diskine göre çok daha hızlıdır.

İlk çalıştırmada Windows bildirimlere erişim izni ister; reddedilirse
**Ayarlar › Gizlilik ve güvenlik › Bildirimler** altından açılabilir.
Yalnızca WhatsApp akışı bu izne bağlıdır, diğer özellikler çalışmaya devam eder.

### Windows açılışında başlatma

`Win+R` → `shell:startup` ile açılan klasöre şu hedefe sahip bir kısayol koyun
(`pythonw` konsol penceresi açmaz):

```
C:\Users\<kullanıcı>\deskpanel-venv\Scripts\pythonw.exe <repo>\companion\companion.py
```

"Başlangıç konumu" alanını `<repo>\companion` yapın.

## Ayarlar (`config.py`)

`config.py` git'e girmez (broker şifresi içerir). Repo güncellenip
`config.example.py`'ye yeni ayar eklendiğinde companion açılışta eksik
ayarları listeler ve durur; bunları örnek dosyadan kopyalamanız yeterlidir.

Öne çıkan ayarlar:

| Ayar | Açıklama |
|---|---|
| `BROKER_HOST`, `BROKER_PORT`, `MQTT_USERNAME`, `MQTT_PASSWORD` | Broker bağlantısı |
| `MEDIA_PREFERRED_APPS` | Birden fazla oynatıcı açıksa öncelik sırası (SMTC uygulama kimliğinin bir parçası) |
| `MEDIA_ART_SIZE` | Panele gönderilen kapak boyutu (px, kare) |
| `MIXER_DISPLAY_NAMES`, `MIXER_HIDDEN_APPS` | Mikserde görünen adlar / gizlenecek uygulamalar |
| `PROFILE_RULES` | Öndeki pencere → panel profili kuralları (biçim: `profiles.py`) |
| `WHATSAPP_PINNED_CONTACTS` | Panelden açılabilecek sohbetler |
| `WHATSAPP_INCLUDE_PREVIEW` | `False` yapılırsa mesaj metni panele gönderilmez |

Sabit sohbet örneği — telefon ülke koduyla, boşluksuz:

```python
WHATSAPP_PINNED_CONTACTS = [
    {"id": "anne", "name": "Annem", "phone": "905xxxxxxxxx"},
]
```

## Tanılama: `probe.py`

Yeni bir PC'de neyin çalıştığını tek seferde gösterir: SMTC oturumu ve kapak
resmi, uygulama ses oturumları, öndeki pencere (15 sn izler), bildirim izni,
`whatsapp://` kaydı. MQTT'ye bir şey göndermez.

```powershell
& $py probe.py
```

Sonuç `probe_out/report.json` dosyasına yazılır (mesaj metinleri hariç).

## MQTT arayüzü

Tüm durum konuları `retain` ile yayınlanır; panel yeniden bağlandığında son
durumu hemen alır. JSON'lar UTF-8'dir.

### Durum (PC → panel)

| Konu | İçerik |
|---|---|
| `deskpanel/companion/status` | `online` / `offline` (LWT) |
| `deskpanel/claude/usage` | `{"requests_today": 12, "tokens_today": 184000}` |
| `deskpanel/audio/state` | `{"volume": 42, "muted": false}` |
| `deskpanel/pc/health` | `{"cpu": 12, "ram": 48, "gpu": 3}` |
| `deskpanel/media/nowplaying` | aşağıda |
| `deskpanel/media/art` | ham kapak pikselleri, aşağıda |
| `deskpanel/mixer/state` | aşağıda |
| `deskpanel/profile` | `{"profile": "ytmusic", "app": "brave.exe"}` — `profile` eşleşme yoksa `null` |
| `deskpanel/whatsapp/feed` | aşağıda |
| `deskpanel/whatsapp/contacts` | `[{"id": "anne", "name": "Annem"}]` — telefon numarası gönderilmez |

**`deskpanel/media/nowplaying`**

```json
{
  "active": true,
  "source": "Brave",
  "title": "İncir",
  "artist": "İlyas Yalçıntaş",
  "album": "İçimdeki Duman",
  "status": "playing",
  "position_s": 143.6,
  "duration_s": 277.1,
  "can_seek": true,
  "art": {"id": "3f2a9c01b7de", "w": 168, "h": 168, "format": "rgb565le"}
}
```

Oynatıcı yoksa yalnızca `{"active": false}`. Tarayıcılar konumu yalnızca
oynat/duraklat/sarma anında bildirdiği için companion konumu kendisi ilerletip
en az `MEDIA_RESYNC_S` saniyede bir yeniden yayınlar; panel `status ==
"playing"` iken aradaki süreyi saymaya devam etmelidir.

**`deskpanel/media/art`** — `w × h × 2` bayt, little-endian RGB565 (LVGL'in
`LV_COLOR_16_SWAP` kapalıyken beklediği `LV_IMG_CF_TRUE_COLOR` düzeni).
Kaynak kare değilse ortadan kırpılır. `nowplaying` içindeki `art.id`
değiştiğinde panel yeni pikselleri kullanmalıdır. 168 px için 56 KB'tır;
panelin MQTT tamponu ya parçalı mesajları birleştirmeli ya da bu boyuttan büyük
olmalıdır.

**`deskpanel/mixer/state`**

```json
{
  "output": {"name": "Hoparlör (Razer BlackShark V2 HS 2.4)", "volume": 42, "muted": false},
  "mic": {"name": "Mikrofon (Razer BlackShark V2 HS 2.4)", "volume": 100, "muted": false},
  "apps": [
    {"id": "brave.exe", "name": "Brave", "volume": 64, "muted": false},
    {"id": "discord.exe", "name": "Discord", "volume": 80, "muted": false}
  ]
}
```

Bir uygulama listede yalnızca Windows'ta açık bir ses oturumu varken görünür
(ör. Brave ses çalarken). Tarayıcının tüm sekmeleri tek kanaldır.

**`deskpanel/whatsapp/feed`**

```json
{
  "count": 3,
  "chats": [{"chat": "Aile grubu", "count": 2}, {"chat": "Ayşe", "count": 1}],
  "items": [
    {"id": 41, "chat": "Aile grubu", "sender": "Ali", "text": "...", "time": "14:28", "ts": 1791628080}
  ]
}
```

### Komutlar (panel → PC)

| Konu | Örnek |
|---|---|
| `deskpanel/media/cmd` | `{"action": "play_pause"}` · `next` · `previous` · `play` · `pause` · `{"action": "seek", "delta_s": -10}` |
| `deskpanel/mixer/cmd` | `{"target": "brave.exe", "volume": 55}` · `{"target": "@mic", "mute": true}` · `{"target": "@output", "volume": 30}` |
| `deskpanel/whatsapp/cmd` | `{"action": "open_chat", "contact": "anne"}` · `{"action": "focus"}` |

Geçersiz komutlar loglanır ve yok sayılır; 1 KB'tan büyük mesajlar reddedilir.

## Bilinen kısıtlar

- **Tarayıcı:** karıştırma, tekrar ve beğenme SMTC'de yok. Oynatma hızı okunamaz.
- **Kapak:** Brave kapağı 120×120 veriyor; daha büyük boyutlar büyütülerek üretilir.
- **WhatsApp:** kişisel hesaplar için API yok, veri bildirimlerden gelir. Sayı
  "yeni bildirim" sayısıdır, gerçek okunmamış sayısı değildir: telefonda okumak
  silmez, WhatsApp'ı PC'de açmak siler. WhatsApp penceresi öndeyken bildirim
  gelmez. Mesaj gönderme bilerek yok — resmi olmayan istemciler hesabın
  kapatılmasına yol açabilir.

## Güvenlik ve gizlilik

- Broker'a erişen herkes durum konularını okuyabilir ve komut gönderebilir.
  Broker'ı yalnızca yerel ağda tutun ve kullanıcı adı/şifre kullanın.
- Panel yalnızca `WHATSAPP_PINNED_CONTACTS` listesindeki sohbetleri açabilir;
  komutta gönderilen rastgele bir numara asla aranmaz/açılmaz. Telefon
  numaraları PC'den çıkmaz.
- Pencere başlıkları yayınlanmaz, yalnızca eşleşen profil adı ve süreç adı gider.
- Mesaj önizlemeleri istenmiyorsa `WHATSAPP_INCLUDE_PREVIEW = False`.

## Testler

Saf mantık modülleri ve publisher'ların Windows bağlantı kodu (sahte
pywinrt/pycaw nesneleriyle) her işletim sisteminde test edilir:

```bash
cd companion
python -m venv .venv && .venv/bin/pip install -r requirements-dev.txt   # Windows: .venv\Scripts\pip
.venv/bin/python -m pytest
```

GitHub Actions (`.github/workflows/companion-tests.yml`) testleri Ubuntu ve
Windows'ta çalıştırır; Windows işi ayrıca gerçek `requirements.txt`'yi kurup
`winrt` ve `pycaw` içe aktarımlarını doğrular.

## Dosyalar

```
companion.py             giriş noktası: MQTT bağlantısı, iş parçacıkları, tepsi ikonu
commands.py              panel komutlarının ortak çözümlemesi
settings_check.py        config.py'de eksik ayar kontrolü
media_session.py         SMTC verisi → JSON, kapak → RGB565, komut doğrulama
media_publisher.py       SMTC okuma/komut (Windows)
mixer.py                 ses oturumlarını uygulama kanallarına birleştirme, komut doğrulama
mixer_publisher.py       pycaw okuma/komut (Windows)
profiles.py              pencere → profil kuralları, titreşim önleyici (debounce)
foreground_publisher.py  ön plan penceresi izleme (Windows)
whatsapp_feed.py         bildirim → akış, sabit sohbetler
whatsapp_publisher.py    bildirim dinleyici, whatsapp:// açma (Windows)
audio_publisher.py       ana ses seviyesi
pc_health_publisher.py   CPU/RAM/GPU
claude_code_usage.py     Claude Code oturum kayıtlarından günlük kullanım
usage_store.py, record_usage.py, tracked_anthropic.py   API kullanımını elle/otomatik kaydetme
probe.py                 tanılama aracı
tests/                   pytest testleri
```
