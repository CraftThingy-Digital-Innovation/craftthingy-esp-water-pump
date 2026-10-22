# CraftThingy Over-The-Air (OTA) Rescue & Offline Upgrade Guide (v1.1.5 / Core v1.1.0)

## Why is a Rescue Tool Needed?
1. **Legacy ESP8266 v1.0.0 Firmware**:
   ESP-01S (1MB) devices running firmware `v1.0.0` were compiled with a 4,096-byte (4KB) TLS input buffer and legacy RSA-CBC ciphers. When downloading from GitHub / AWS CloudFront (`release-assets.githubusercontent.com`), AWS transmits 16KB TLS records and requires ECDHE ciphers. This causes BearSSL to abort with `BR_ERR_TOO_LARGE` or SSL connection handshake failures.
2. **Zero-Internet / Isolated Environments**:
   When upgrading devices deployed in the field without router internet access, the rescue bridge acts as a local simulated GitHub releases endpoint.
3. **100% Wireless Recovery**:
   No USB cable, disassembly, or UART programmer is needed. The tool streams firmware in safe **1,024-byte chunks with 5ms pacing** directly into the ESP's flash.

---

## Scenario A: ESP Connected to Local Wi-Fi (Recommended)
If your ESP is connected to your local network (e.g. `192.168.2.228`):

1. Open PowerShell or Terminal with Administrator privileges (needed to bind port 53/443).
2. Run the rescue bridge pointing to the ESP:
   ```powershell
   python tools/ota_rescue_bridge.py --esp 192.168.2.228
   ```
3. The tool will:
   - Generate temporary local TLS certificates matching GitHub hostnames.
   - Serve simulated GitHub release metadata for the target release (e.g. `v1.1.6`).
   - Stream the firmware binary in 1,024-byte paced TLS records.
   - Monitor the ESP's flash write and verify reboot into `v1.1.6`.

---

## Scenario B: ESP in Hotspot Mode / Field Recovery (Zero Internet Needed!)
If the ESP is in fallback AP mode (`CraftThingy-5371` or `CraftThingy-Water-Pump-AP` at `192.168.4.1`):

### Method 1: Using your Laptop's Mobile Hotspot
1. Turn on **Mobile Hotspot** on your laptop (e.g. SSID: `RescueAP`, Password: `password123`).
2. Run the rescue tool providing hotspot credentials:
   ```powershell
   python tools/ota_rescue_bridge.py --esp 192.168.4.1 --wifi-ssid "RescueAP" --wifi-pass "password123"
   ```
3. The tool provisions the ESP to join your laptop's hotspot and streams the firmware locally.

### Method 2: Browser Web Manager (`tools/esp_manager.html`)
1. Connect your laptop or phone Wi-Fi directly to the ESP's hotspot (`CraftThingy-5371`).
2. Open [esp_manager.html](file:///tools/esp_manager.html) in your browser.
3. Under **3. WiFi Provisioning**, enter your Wi-Fi SSID and password and click **Save & Connect WiFi**.
4. Once connected to your local Wi-Fi, run:
   ```powershell
   python tools/ota_rescue_bridge.py --esp <Assigned-ESP-IP>
   ```

---

## Command Options

| Argument | Description | Default |
|---|---|---|
| `--esp` | Target ESP IP address | `192.168.2.228` |
| `--key` | Master 64-hex Secret Key | Default developer key |
| `--bin` | Path to custom firmware `.bin` | Auto-detected from local build or release |
| `--tag` | Release tag to simulate / target | `v1.1.5` |
| `--target` | Role target (`water_pump` or `core`) | `water_pump` |
| `--bridge-ip` | Host IP address for rescue bridge | Auto-detected |
| `--gateway` | Local Network Gateway IP | `192.168.2.1` |
| `--wifi-ssid` | Provision SSID (for AP mode rescue) | `None` |
| `--wifi-pass` | Provision Password (for AP mode rescue) | `None` |
| `--version` | Display rescue tool version | — |
