# CraftThingy ESP8266 v1.0.0 Over-The-Air (OTA) Rescue Guide

## Why is a Rescue Tool Needed for v1.0.0?
Firmware version `v1.0.0` running on ESP8266 (ESP-01S 1MB) was built with BearSSL configured with a 4,096-byte (4KB) TLS input buffer. 

When attempting to download releases directly from GitHub/AWS S3 (`release-assets.githubusercontent.com`), AWS S3 transmits TLS records in standard **16,384-byte (16KB) chunks** without supporting RFC 6066 Max Fragment Length Negotiation (MFLN). This causes BearSSL to abort the TLS session with `BR_ERR_TOO_LARGE`, producing:
```
Update Failed: OTA verification failed (0)
```

## Solution: 100% Over-The-Air Rescue Bridge
You do **NOT** need to connect a USB cable, disassemble the enclosure, or re-flash over serial. 
The included `tools/ota_rescue_bridge.py` script serves as a local proxy that intercepts GitHub release requests and streams the firmware in safe **1,024-byte chunks** directly to the ESP flash.

---

## Scenario A: ESP Connected to Local Wi-Fi (Recommended)
If your ESP is already on your home Wi-Fi (e.g. `192.168.2.228`):

1. Open PowerShell or Terminal with Administrator privileges (needed to bind port 53/443).
2. Run the rescue bridge pointing to the ESP's IP:
   ```powershell
   python tools/ota_rescue_bridge.py --esp 192.168.2.228
   ```
3. The tool will:
   - Generate temporary local TLS certificates.
   - Serve simulated GitHub release metadata for `v1.1.5`.
   - Stream the firmware binary in 1024-byte paced chunks.
   - Wait for the ESP to flash and reboot into `v1.1.5`.

---

## Scenario B: ESP in Hotspot Mode / Field Recovery (Zero Internet Needed!)
If the ESP is in fallback AP mode (`CraftThingy-5371` or `CraftThingy-Water-Pump-AP` at `192.168.4.1`), or you are in the field with no home router:

### Method 1: Using your Laptop's Mobile Hotspot
1. Turn on **Mobile Hotspot** on your laptop (e.g. SSID: `RescueAP`, Password: `password123`).
2. Run the rescue tool providing the hotspot credentials:
   ```powershell
   python tools/ota_rescue_bridge.py --esp 192.168.4.1 --wifi-ssid "RescueAP" --wifi-pass "password123"
   ```
3. The tool provisions the ESP to join your laptop's hotspot, where it immediately fetches the 1024-byte chunked update from your laptop without touching the external internet.

### Method 2: Browser Web Manager (`tools/esp_manager.html`)
1. Connect your laptop or phone Wi-Fi directly to the ESP's hotspot (`CraftThingy-5371` or `CraftThingy-Water-Pump-AP`).
2. Double-click to open [esp_manager.html](file:///tools/esp_manager.html) in your browser.
3. Under **3. WiFi Provisioning**, enter your Wi-Fi SSID and password and click **Save & Connect WiFi**.
4. Once the ESP is connected to your Wi-Fi, run:
   ```powershell
   python tools/ota_rescue_bridge.py --esp <Assigned-ESP-IP>
   ```

---

## Command Options

| Argument | Description | Default |
|---|---|---|
| `--esp` | Target ESP IP address | `192.168.2.228` |
| `--key` | Master 64-hex Secret Key | Default developer key |
| `--bin` | Path to firmware `.bin` | Auto-detected from build or latest release |
| `--wifi-ssid` | Provision SSID (for AP mode rescue) | `None` |
| `--wifi-pass` | Provision Password (for AP mode rescue) | `None` |
| `--gateway` | Network Gateway IP | `192.168.2.1` |
