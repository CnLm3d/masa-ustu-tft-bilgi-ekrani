# ESP32 DeskHub (Yatay 2.4" Dokunmatik TFT)

ESP32 DevKit ve **TJCTM24024-SPI (2.4" 240×320 ILI9341 + XPT2046 Dokunmatik)** ekran ile harici buton/encoder gerektirmeyen masaüstü bilgi istasyonu:

1. **Yaprak Saat (Flip Clock):** Her dakika geçişinde 3D gölgeli mekanik yaprak dönme animasyonu, Türkçe tarih, alt özet hava durumu ve 60 sn ilerleme çubuğu.
2. **Hava Durumu (Open-Meteo):** API anahtarsız anlık sıcaklık, hissedilen, nem, rüzgar, Türkçe hava durumu ikonu ve 3 günlük tahmin.
3. **Google Haberler (Türkiye RSS):** Gündem, Teknoloji, Ekonomi, Bilim ve Spor kategorilerinde Türkçe manşet akışı ve dokunmatik gezinme butonları.

## Donanım Bağlantı Şeması (`TJCTM24024-SPI`)

Ekran ve dahili **XPT2046 dokunmatik kontrolcüsü** aynı SPI hattını farklı `CS` pinleriyle paylaşır:

| Ekran Pini | ESP32 DevKit / NodeMCU-32S (`esp32dev`) | ESP32-C3 Mini (`esp32c3`) | Açıklama |
|---|---|---|---|
| `VCC` | **5V (VIN)** | **5V (VBUS)** | Kart arkasındaki `U1` (XC6206 3.3V regülatör) için 5V |
| `GND` | `GND` | `GND` | Ortak toprak |
| `CS` | `GPIO 5` | `GPIO 7` | TFT Chip Select |
| `RESET` | `GPIO 17` | `GPIO 3` | TFT Reset |
| `DC` | `GPIO 16` | `GPIO 2` | Data / Command |
| `SDI (MOSI)` | `GPIO 23` | `GPIO 6` | SPI MOSI |
| `SCK` | `GPIO 18` | `GPIO 4` | SPI SCLK |
| `LED` | **3.3V** | **3.3V** | Arka ışık |
| `SDO (MISO)` | `GPIO 19` | `GPIO 5` | SPI MISO |
| `T_CLK` | `GPIO 18` | `GPIO 4` | Dokunmatik SPI Clock (`SCK` ile paralel) |
| `T_CS` | `GPIO 27` | `GPIO 10` | Dokunmatik Chip Select |
| `T_DIN` | `GPIO 23` | `GPIO 6` | Dokunmatik MOSI (`SDI` ile paralel) |
| `T_DO` | `GPIO 19` | `GPIO 5` | Dokunmatik MISO (`SDO` ile paralel) |
| `T_IRQ` | *Boş* | *Boş* | Kullanılmıyor (yoklama modu) |

## Dokunmatik Kullanım

- **Alt Sekme Çubuğu (`Saat | Hava | Haber`):** Tek dokunuşla sayfalar arası geçiş.
- **Sağa / Sola Kaydırma:** Saat/Hava'da sayfa değiştirir; Haberler'de önceki/sonraki manşete geçer.
- **Yaprak Saat:** Dakika kartına dokun → demo yaprak animasyonu; Saat kartına dokun → NTP/Hava/Haber yenile; Tarih/Hava bandına dokun → Hava sayfasına geç.
- **Hava Durumu:** Ana karta veya `Yenile` butonuna dokun → hava verisini güncelle.
- **Google Haberler:** `< Önceki` / `Sonraki >` ile haberleri gez, `Konu Değiş` ile kategoriyi değiştir.
- **Üst Sağ `[Ayar]` Butonu:** Wi-Fi & Şehir Kurulum Portalını (`ESP32-DeskHub`, `http://192.168.4.1`) veya 4 köşe dokunmatik kalibrasyonu yeniden başlat.

## İlk Açılış ve Kalibrasyon

1. İlk açılışta ekranda 4 köşe dokunmatik kalibrasyonu çıkar; oklara sırayla dokunun (NVS'e kaydedilir, sonraki açılışlarda tekrar sorulmaz).
2. Wi-Fi ayarlı değilse **`ESP32-DeskHub`** ağı açılır; telefon/PC ile bağlanıp **`http://192.168.4.1`** üzerinden Wi-Fi ve şehir bilgisini kaydedin.

## Derleme ve Yükleme (PlatformIO)

**ESP32 DevKit / NodeMCU-32S (`esp32dev`):**
```powershell
pio run -e esp32dev
pio run -e esp32dev -t upload
```

**ESP32-C3 Mini (`esp32c3`):**
```powershell
pio run -e esp32c3
pio run -e esp32c3 -t upload
```

Seri monitör:
```powershell
pio device monitor -b 115200
```
