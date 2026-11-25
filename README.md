# ESP32 DeskHub (Yatay 2.4" TFT + Rotary Encoder)

ESP32 DevKit, **TJCTM24024-SPI (2.4" 240×320 ILI9341)** yatay ekran ve **Rotary Encoder** ile çalışan masaüstü bilgi istasyonu:

1. **Yaprak Saat (Flip Clock):** Her dakika geçişinde 3D gölgeli mekanik yaprak dönme animasyonu, Türkçe tarih, alt özet hava durumu ve 60 sn ilerleme çubuğu.
2. **Hava Durumu (Open-Meteo):** API anahtarı gerektirmeyen anlık sıcaklık, hissedilen, nem, rüzgar, Türkçe hava durumu ikonu ve 3 günlük tahmin.
3. **Google Haberler (Türkiye RSS):** Gündem, Teknoloji, Ekonomi, Bilim ve Spor kategorilerinde Türkçe manşet akışı.

## Donanım Bağlantı Şeması

### 1. Ekran (`TJCTM24024-SPI` → ESP32 DevKit)
| Ekran Pini | ESP32 Pini | Açıklama |
|---|---|---|
| `VCC` | **5V (VIN)** | Kart arkasındaki `U1` (XC6206 3.3V regülatör) için 5V |
| `GND` | `GND` | Ortak toprak |
| `CS` | `GPIO 5` | TFT Chip Select |
| `RESET` | `GPIO 17` | TFT Reset |
| `DC` | `GPIO 16` | Data / Command |
| `SDI (MOSI)` | `GPIO 23` | VSPI MOSI |
| `SCK` | `GPIO 18` | VSPI SCLK |
| `LED` | **3.3V** | Arka ışık (veya 100 Ω direnç ile 3.3V) |
| `SDO (MISO)` | `GPIO 19` | VSPI MISO |

> **Önemli:** Ekran üzerindeki `T_CLK`, `T_CS`, `T_DIN`, `T_DO`, `T_IRQ` (dokunmatik) ve `SD_*` pinleri bu projede kullanılmaz, boş bırakılabilir.

### 2. Rotary Encoder (KY-040 / EC11 → ESP32 DevKit)
| Encoder Pini | ESP32 Pini | Açıklama |
|---|---|---|
| `CLK` | `GPIO 32` | Faz A (Kesme) |
| `DT` | `GPIO 33` | Faz B (Kesme) |
| `SW` | `GPIO 25` | Buton (Dahili `INPUT_PULLUP`) |
| `+` | **3.3V** | **5V bağlamayın** (ESP32 GPIO 3.3V lojiktir) |
| `GND` | `GND` | Ortak toprak |

## Rotary Encoder Kontrolleri

| Sayfa / Durum | Döndürme | Kısa Basma | Uzun Basma (1.1 sn) | Çok Uzun Basma (4 sn) |
|---|---|---|---|---|
| **Yaprak Saat** | Sayfa değiştir | Demo yaprak animasyonu | NTP + Hava + Haber yenile | Kurulum Portalı |
| **Hava Durumu** | Sayfa değiştir | Hava verisini yenile | Hava verisini yenile | Kurulum Portalı |
| **Google Haberler** | Sayfa değiştir *(Normal)* / Haber gez *(Gezme Modu)* | Haber Gezme Modu Aç/Kapat | Kategori değiştir (Gündem/Teknoloji/Ekonomi/Bilim/Spor) | Kurulum Portalı |

## İlk Kurulum (Wi-Fi ve Şehir)

1. Cihaz ilk kez açıldığında (veya açılışta encoder butonuna basılı tutulduğunda) **`ESP32-DeskHub`** adlı şifresiz Wi-Fi ağı açar.
2. Telefon/PC ile bağlanıp **`http://192.168.4.1`** adresinden Wi-Fi bilgilerini, şehri (hazır Türk şehirleri listesi veya özel koordinat) ve varsayılan haber kategorisini kaydedin.
3. İsterseniz derleme öncesi `include/config.h` içindeki `DEFAULT_WIFI_SSID` / `DEFAULT_WIFI_PASS` değerlerini de doğrudan düzenleyebilirsiniz.

## Derleme ve Yükleme (PlatformIO)

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```
