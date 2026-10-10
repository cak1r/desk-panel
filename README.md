# Desk Panel

ESP32-S3 tabanlı, 7" dokunmatik bir masaüstü bilgi ve kontrol paneli. Boştayken
büyük bir saat gösterir; dokununca Stream Deck benzeri bir tuş ızgarasına ve
canlı bilgi ekranlarına geçer. PC'ye tek bir USB-C kablosuyla bağlanır: aynı
kablo hem güç verir hem de panelin klavye/medya tuşu olarak davranmasını sağlar.

## Özellikler

- **Bekleme ekranı:** büyük saat, tarih, hava; altında çalan şarkı, sıradaki
  namaz vaktine kalan süre ve WhatsApp bildirimleri için bakış kartları.
  5 dakika dokunulmazsa geri iner.
- **Deck:** Stream Deck benzeri 5×3 tuş ızgarası. Kısayol tuşları (Discord
  sağırlaştır, ekran görüntüsü, VS Code, Terminal, PC kilitle) USB HID ile gider;
  canlı tuşlar veri gösterir (çalan şarkı ve kapağı, WhatsApp rozeti, mikrofon
  durumu, Claude token'ı, namaz geri sayımı, hava, ses, CPU).
- **Şimdi çalan:** kapak, ilerleme (dokunarak sarma), ±10 sn, önceki/sonraki ve
  çalan uygulamanın kendi ses seviyesi.
- **Mikser:** her uygulama, hoparlör ve mikrofon için dikey ses sürgüsü ve
  sessize alma.
- **Otomatik profil:** PC'de YouTube, YouTube Music ya da WhatsApp öne gelince
  panel ilgili sayfaya geçer; uygulama arka plana düşünce Deck'e döner.
- **YouTube:** videonun bilgisi ve YouTube'un oynatıcı kısayolları (J/K/L, tam
  ekran, altyazı, hız, sonraki video…).
- **WhatsApp:** PC'deki son bildirimler ve tek dokunuşla açılan sabit sohbetler.
- **Namaz vakitleri:** 6 vakit, saniyeli geri sayım, hicri tarih (Aladhan API,
  Diyanet yöntemi, NVS önbelleği). Günün hadisi kartı companion yayın yaptığında
  görünür.
- **Hava durumu:** anlık hava, saatlik yağış ihtimali ve 10 günlük tahmin
  (Open-Meteo).
- **PC sağlığı, Claude kullanımı** ve PIN kilitli **şifre/makro** ekranı.

## Donanım

- [Waveshare ESP32-S3-Touch-LCD-7](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7):
  800×480 RGB panel, GT911 dokunmatik, CH422G IO genişletici, 8 MB PSRAM
- USB-C ile PC'ye bağlantı (ESP32-S3'ün yerel USB OTG portu)

## Mimari

```
┌──────────────────────────┐  USB HID (klavye + medya tuşları)  ┌───────────────┐
│  Panel (ESP32-S3)        │ ─────────────────────────────────► │      PC       │
│  LVGL 8 arayüz           │                                     │               │
│  TinyUSB HID             │        MQTT (WiFi)                  │   companion/  │
│  WiFi + SNTP + MQTT      │ ◄─────── durum ─────────────────── │   (Python,    │
│  Aladhan, Open-Meteo     │ ──────── komutlar ───────────────► │    Windows)   │
└──────────────────────────┘                                     └───────────────┘
```

- **Komutlar** (kısayollar, makrolar, medya tuşları) USB HID ile gider; PC'de
  yazılım gerekmez.
- **PC'den okunan durum** (çalan şarkı, uygulama sesleri, bildirimler, kullanım)
  ve belirli bir uygulamayı hedefleyen komutlar MQTT ve
  [companion servisi](companion/README.md) üzerinden geçer.
- Namaz vakitleri ve hava durumunu panel doğrudan internetten çeker.

MQTT konularının ve mesaj biçimlerinin tam listesi
[companion/README.md](companion/README.md#mqtt-arayüzü) içinde.

## Firmware'i derleme

Gereken: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) v5.4+.

1. Git'e girmeyen ayar dosyalarını örneklerinden oluşturup doldurun:

   ```bash
   cp main/wifi_credentials.h.example main/wifi_credentials.h   # WiFi adı/şifresi
   cp main/mqtt_config.h.example      main/mqtt_config.h        # broker adresi (boş = MQTT kapalı)
   cp main/secrets_config.h.example   main/secrets_config.h     # kilit PIN'i ve makrolar
   ```

2. Konum ayarları kodda: namaz için `main/prayer_times.c` (`PRAYER_CITY`,
   `PRAYER_COUNTRY`), hava için `main/weather_state.c` (`WEATHER_LAT`, `WEATHER_LON`).

3. Derleyip yükleyin:

   ```bash
   idf.py set-target esp32s3
   idf.py -p <PORT> build flash monitor
   ```

   İlk derlemede bileşenler (LVGL vb.) `managed_components/` altına indirilir.
   `dependencies.lock` yerel bileşenlerin mutlak yolunu içerdiği için her
   makinede derleme sırasında değişir; bu değişikliği commit'lemeyin.

### Fontlar ve ikonlar

`main/fonts/` altındaki LVGL fontları ve `main/ui_icons.h` elle düzenlenmez,
`assets/gen_fonts.py` ile üretilir (Node.js gerekir, `lv_font_conv`'u `npx` ile
çalıştırır). Yeni bir Tabler ikonu eklemek için adını script'teki `ICONS`
listesine ekleyip çalıştırın:

```bash
python3 assets/gen_fonts.py
```

### Testler

Donanımdan bağımsız firmware kodu (biçimlendirme, MQTT JSON yardımcıları)
bilgisayarda test edilir:

```bash
. $IDF_PATH/export.sh     # cJSON için
make -C test/host
```

GitHub Actions'ta `firmware` iş akışı her değişiklikte firmware'i ESP-IDF
v5.4.4 ile derler ve bu testleri çalıştırır; `companion tests` iş akışı
companion testlerini çalıştırır.

Bölümleme tablosu (`partitions.csv`) 8 MB flash için iki OTA bölümü içerir;
diğer seçenekler `partitions_notes.txt` dosyasında.

> ⚠️ `secrets_config.h` şu an düz metin. Gerçek şifre saklamadan önce flash ve
> NVS şifrelemesi açılmalı (yol haritası 4. adım).

## PC companion servisi

Windows'ta tepside çalışan Python servisi. Kurulum, ayarlar, MQTT arayüzü,
testler ve bilinen kısıtlar için: **[companion/README.md](companion/README.md)**.

## Repo yapısı

```
main/                  firmware kaynakları
  main.c               başlatma sırası
  ui.c                 gezinme: saat perdesi, Deck, sayfalar, otomatik profil
  ui_theme.c           renkler, fontlar, ortak stil yardımcıları
  ui_art.c             tüm ekranların paylaştığı albüm kapağı
  screen_*.c           her ekran
  *_state.c            MQTT/HTTP'den gelen veri modelleri
  app_mqtt.c           MQTT istemcisi, parçalı mesaj birleştirme, komut gönderme
  usb_hid.c            TinyUSB klavye + consumer control (Türkçe Q eşlemesi)
  hid_actions.c        HID kısayollarını arayüzü bloklamadan gönderen kuyruk
  prayer_times.c       Aladhan istemcisi + NVS önbellek
  fonts/               assets/gen_fonts.py ile üretilen LVGL fontları
components/            yerel olarak tutulan bileşenler (LVGL, GT911)
assets/                kaynak fontlar, Tabler ikon fontu, font üretici
test/host/             firmware için bilgisayarda çalışan birim testleri
companion/             PC tarafı servis (Python) — kendi README'si var
```

## Yol haritası

1. ~~LVGL + tileview iskeleti ve USB HID~~
2. ~~Namaz vakti ekranı~~
3. ~~PC companion servisi~~ — şimdi çalan, mikser, otomatik profil ve WhatsApp
   hem companion'da hem panelde hazır; günün hadisi için kaynak bekleniyor
4. Güvenlik sertleştirme: flash + NVS şifreleme, PIN kilidi
5. Cilalama: WiFi kurulumu (provisioning), OTA, güç yönetimi, ayarlar ekranı
