# Desk Panel

ESP32-S3 tabanlı, 7" dokunmatik bir masaüstü bilgi ve kontrol paneli. Boştayken
büyük bir saat gösterir; dokununca Stream Deck benzeri bir tuş ızgarasına ve
canlı bilgi ekranlarına geçer. PC'ye tek bir USB-C kablosuyla bağlanır: aynı
kablo hem güç verir hem de panelin klavye/medya tuşu olarak davranmasını sağlar.

## Özellikler

- **Bekleme ekranı:** büyük saat ve tarih; belirli bir süre dokunulmazsa
  otomatik olarak geri döner.
- **Tuş ızgarası:** ekranlara geçiş, klavye makroları, medya tuşları.
- **Namaz vakitleri:** günün 6 vakti ve sıradaki vakte geri sayım (Aladhan API,
  Diyanet hesaplama yöntemi, NVS'te önbelleklenir).
- **Hava durumu:** Open-Meteo'dan anlık hava.
- **Medya:** ses seviyesi, oynat/duraklat/ileri/geri.
- **PC sağlığı:** CPU / RAM / GPU kullanımı.
- **Claude kullanımı:** günlük istek ve token sayısı.
- **Şifre/makro ekranı:** PIN ile kilitli, metni USB klavye olarak yazar.

Yapım aşamasında (companion tarafı hazır, panel ekranları geliştiriliyor):
şimdi çalan şarkı + kapak, uygulama başına ses mikseri, öndeki uygulamaya göre
otomatik sayfa (YouTube / YouTube Music / WhatsApp), WhatsApp bildirimleri ve
sabit sohbetler, 10 günlük hava durumu, günün hadisi.

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
  ui.c, ui_theme.c     tileview, ekran geçişleri, tema
  screen_*.c           her ekran
  *_state.c            MQTT/HTTP'den gelen veri modelleri
  app_mqtt.c           MQTT istemcisi ve abonelik yönlendirme
  usb_hid.c            TinyUSB klavye + consumer control
  prayer_times.c       Aladhan istemcisi + NVS önbellek
  fonts/, images/      LVGL'e dönüştürülmüş fontlar ve görseller
components/            yerel olarak tutulan bileşenler (LVGL, GT911)
assets/                kaynak fontlar, ikonlar, gradyan üretici
companion/             PC tarafı servis (Python) — kendi README'si var
```

## Yol haritası

1. ~~LVGL + tileview iskeleti ve USB HID~~
2. ~~Namaz vakti ekranı~~
3. PC companion servisi — temel sürüm hazır; şimdi çalan, mikser, otomatik
   profil ve WhatsApp eklendi, panel tarafı geliştiriliyor
4. Güvenlik sertleştirme: flash + NVS şifreleme, PIN kilidi
5. Cilalama: WiFi kurulumu (provisioning), OTA, güç yönetimi, ayarlar ekranı
