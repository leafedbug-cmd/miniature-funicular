# miniature-funicular

This repository is being rebuilt around the Lonely Binary ESP32-S3 N16R8 IPEX board.

Start with [SPEC_SHEET.md](SPEC_SHEET.md) for the board reference this codebase targets.

## University RF Faraday Lab Testing

This board will be used for university RF Faraday lab testing under supervised conditions.

- All equipment is signed out and must remain in the lab.
- The lab contains simulated BLE, Bluetooth, and Wi-Fi devices for controlled testing.
- All test activities are performed under the supervision of a professor.
- All tests are authorized and limited to approved lab work.

The goal is to keep the work controlled, repeatable, and compliant with lab procedures while validating wireless behavior on the ESP32-S3 platform.

## Current Firmware Behavior

The firmware now runs the ESP32-S3 as a local Wi-Fi access point and RF dashboard host.

- AP SSID: `bumptima`
- AP role: serves a live web UI on `http://192.168.4.1`
- Wi-Fi role: scans nearby Wi-Fi networks (SSID, BSSID, channel, RSSI, security)
- BLE role: scans nearby BLE advertisers (name, address, RSSI)
- nRF24 role: performs fast 2.4 GHz channel sweeps using RPD energy detection

The ESP32-S3 fuses all inputs into a single spectrum model:

- `nRF24` raw energy hits per channel (`0..125`)
- `Wi-Fi` channel occupancy mapped onto 2.4 GHz bins
- `BLE` advertiser activity blended across BLE advertising channels

The web UI displays:

- fused spectrum view (interpreted RF load)
- raw nRF24 sweep bars
- Wi-Fi network list with identifiers
- BLE device list with identifiers

## Notes About Hardware Roles

The nRF24L01+PA+LNA module is used here for RF energy sensing, not protocol-level decode.
Device naming and protocol-aware identification come from the ESP32-S3 Wi-Fi/BLE radios, while the nRF24 contributes wide-channel energy context.
