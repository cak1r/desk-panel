# Proje: ESP32-S3 masaüstü panel

## Donanım
- Waveshare ESP32-S3-Touch-LCD-7 (800x480 RGB, GT911 dokunmatik, CH422G IO expander)
- USB-C ile PC'ye bağlı — güç ve veri aynı kablodan gidiyor
- ESP-IDF v5.4+

## Mevcut durum
- Custom display/touch driver zaten yazılı (RGB parallel + CH422G + GT911, PSRAM framebuffer)
- LVGL henüz entegre değil, bu proje onu ekliyor

## Hedef
Masaüstü bilgi paneli: idle saat ekranı, uzun inaktivite sonrası dokununca yukarı kayan,
sağa/sola kaydırmalı widget ekranları.

## Ekranlar (lv_tileview)
1. **Ana ekran**: büyük saat + küçük tarih, inaktivitede otomatik buraya döner
2. **Namaz vakitleri**: yatay dizilmiş 6 vakit, üstte sıradaki vakite canlı geri sayım
   (Aladhan API'den günde 1 kez çekilip NVS'e cache'lenir)
3. **Claude API kullanımı**: kendi loglanan istek/token sayıları (resmi metering API yok,
   kendi script'lerinde tutulan log companion servis üzerinden okunacak)
4. **Ses/medya kontrolü**: ses %, play/pause/next

## Mimari kararlar
- **Komutlar** (klavye makrosu, şifre yazma, ses/medya tuşları): ESP32-S3 native USB OTG
  üzerinden TinyUSB HID — Keyboard + Consumer Control report descriptor. PC'de yazılım
  gerekmez, doğrudan USB kablosu üzerinden gider.
  - Şifre saklanacaksa: flash encryption + NVS encryption şart, makro ekranına PIN/desen kilidi eklenmeli.
- **Durum verisi** (ses %, çalan şarkı, Claude kullanımı): WiFi + MQTT.
  - Broker: mevcut Home Assistant MQTT broker'ı varsa o kullanılabilir.
  - Örnek topic'ler: `deskpanel/audio/state`, `deskpanel/media/nowplaying`,
    `deskpanel/claude/usage`, `deskpanel/companion/status` (LWT).
- **PC companion servisi** (Python): `pycaw` (ses seviyesi), `winsdk`/SMTC (now playing —
  Chrome'da YouTube Music sekmesi otomatik expose eder), kendi Claude API kullanım logu.
  `pystray` ile sistem tepsisinde çalışır, Windows başlangıcına eklenir.
- BLE'ye gerek yok — cihaz zaten USB-C ile bağlı olacağı için HID kablo üzerinden gidiyor,
  BLE radyosu bu projede kullanılmıyor.

## UI tasarım yönü
- Koyu tema: arka plan `#0c0e11`, vurgu rengi `#35d4a4` (teal-green)
- Saat rakamı için ince/orta ağırlıklı bir Google Font (Inter veya Manrope), LVGL font
  converter'da `bpp=4` ile dönüştürülecek (büyük punto antialiasing için kritik)
- İkonlar: Tabler/Feather outline seti, font olarak dönüştürülüp kullanılacak
- Tileview geçişlerinde ease-out animasyon, buton basımlarında hafif state geçişi

## Faz sırası
1. **Şu anki öncelik**: LVGL + tileview iskeleti VE USB HID katmanı birlikte
2. Namaz vakti ekranı
3. PC companion servisi
4. Güvenlik sertleştirme (flash/NVS encryption, PIN kilidi)
5. Cilalama (WiFi provisioning, OTA, güç yönetimi, ayarlar ekranı)