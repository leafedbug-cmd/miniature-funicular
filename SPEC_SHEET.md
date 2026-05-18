# Lonely Binary ESP32-S3 N16R8 IPEX Board Spec Sheet

Document status: working reference for the board this repository targets.

## 1. Board Identity

- Product family: Lonely Binary ESP32-S3 Type-C IPEX Board
- Board variant: `ESP32-S3-DEVKITC-1U-N16R8`
- Module: `ESP32-S3-WROOM-1U`
- MCU: Espressif ESP32-S3
- Memory configuration: 16 MB flash, 8 MB PSRAM
- Antenna: external antenna through IPEX / u.FL connector
- Onboard user LED: WS2812 RGB LED on `GPIO48`
- Primary role: general-purpose ESP32-S3 development board with external-antenna wireless support

## 2. High-Level Capabilities

- Dual-core Xtensa LX7 CPU
- Up to 240 MHz CPU clock
- 2.4 GHz Wi-Fi 802.11 b/g/n
- Bluetooth 5 (LE)
- 45 programmable GPIOs on the SoC
- Native USB support on ESP32-S3
- UART-based programming and console support
- Onboard addressable RGB indicator LED for status and feedback

## 3. Confirmed Vendor Details

The current Lonely Binary listing identifies this board as the external-antenna variant with the following published characteristics:

- 16 MB Flash
- 8 MB PSRAM
- Dual core
- 240 MHz
- Wi-Fi
- Bluetooth
- 45 GPIO
- SKU: `ESP32-S3-DEVKITC-1U-N16R8`

The product page also states that Wi-Fi and Bluetooth connectivity use the external antenna path via the IPEX connector.

## 4. Core Silicon Specifications

These are the ESP32-S3 platform capabilities the board inherits from the chip family:

- CPU architecture: Xtensa LX7
- CPU count: 2 cores
- Maximum clock frequency: 240 MHz
- Internal SRAM: 512 KB
- ROM: 384 KB
- Wireless: 2.4 GHz Wi-Fi and Bluetooth 5 LE
- Programmable GPIOs: 45
- Peripheral support includes:
  - SPI
  - I2S
  - I2C
  - PWM
  - RMT
  - ADC
  - DAC
  - UART
  - SD/MMC host
  - TWAI
  - USB 2.0 OTG / USB Serial-JTAG support

## 5. Memory and Storage

### 5.1 Onboard Flash

- Size: 16 MB
- Use: program storage, filesystem, assets, OTA images, and data logging

### 5.2 Onboard PSRAM

- Size: 8 MB
- Use: frame buffers, large heaps, audio buffers, display buffers, networking bursts, and other memory-heavy workloads

### 5.3 Practical Implications

- 16 MB flash is enough for larger applications, multiple assets, and OTA update partitions.
- 8 MB PSRAM makes the board suitable for camera, display, audio, and web-stack projects that exceed internal SRAM alone.

## 6. Wireless and RF

### 6.1 Wi-Fi

- Band: 2.4 GHz
- Standards: 802.11 b/g/n
- Antenna path: external antenna through IPEX connector

### 6.2 Bluetooth

- Bluetooth version: 5
- Mode: Bluetooth Low Energy
- Antenna path: external antenna through IPEX connector

### 6.3 RF Notes

- Because the board uses an external antenna configuration, RF performance depends on the antenna, feed cable, and connector quality.
- The antenna must be connected for normal wireless operation.
- For bench use, keep the antenna attached before transmitting.

## 7. Power

### 7.1 Power Rails Exposed on the Board

- `3V3`
- `5V0`
- `GND`

### 7.2 Voltage Domain

- Logic level: 3.3 V class
- ESP32-S3 supply range: 3.0 V to 3.6 V on the relevant chip rails

### 7.3 Practical Power Guidance

- Use a stable 5 V source into the board only if the board’s regulator path is intended for it.
- Use `3V3` only when you are intentionally powering the board from a clean regulated 3.3 V source and understand the board’s power architecture.
- Do not drive GPIOs above the board’s logic level.

### 7.4 Boot and Reset

- `BOOT` button is tied to the boot strap path.
- `RESET` button resets the board.
- `GPIO0` is the boot strap pin.
- `GPIO45` and `GPIO46` are also strapping-related on ESP32-S3 and should not be assumed free at boot.

## 8. USB and Programming

### 8.1 Ports

- The board exposes two USB-C connectors:
  - one labeled `UART`
  - one labeled `USB`

### 8.2 Programming Paths

- Upload via USB
- Upload via UART
- Console output can be routed through USB CDC when enabled in firmware/tooling

### 8.3 Tooling Support

- Arduino IDE
- PlatformIO
- MicroPython
- ESP-IDF

### 8.4 Driver Note

- Lonely Binary documents CH34X driver installation for host-side UART support.

## 9. Onboard Indicators and Controls

### 9.1 RGB LED

- Type: WS2812 addressable RGB LED
- Data pin: `GPIO48`
- Behavior: single-pixel status LED
- Library usage:
  - MicroPython `neopixel`
  - Arduino `FastLED`
  - Arduino `Adafruit_NeoPixel`

### 9.2 Buttons

- `BOOT`
- `RESET`

### 9.3 LED Design Constraint

- `GPIO48` is consumed by the onboard WS2812 LED.
- Treat it as an occupied pin unless you deliberately plan to replace or repurpose the LED circuit.

## 10. Pinout

This board exposes a mixed-use pinout with GPIO, analog, touch, RTC, USB, and peripheral-specific alternate functions.
The header map below is transcribed from Lonely Binary's published pinout image for the N16R8 IPEX board.

### 10.1 Left Header

Top to bottom:

- `3V3`
- `3V3`
- `RST`
- `GPIO4` / `ADC1_3` / `TOUCH4` / `RTC`
- `GPIO5` / `ADC1_4` / `TOUCH5` / `RTC`
- `GPIO6` / `ADC1_5` / `TOUCH6` / `RTC`
- `GPIO7` / `ADC1_6` / `TOUCH7` / `RTC`
- `GPIO15` / `ADC1_4` / `U0RTS` / `RTC`
- `GPIO16` / `ADC1_5` / `U0RTS` / `RTC`
- `GPIO17` / `ADC1_6` / `U1TXD` / `RTC`
- `GPIO18` / `ADC1_7` / `U1RXD` / `RTC`
- `GPIO8` / `ADC1_7` / `TOUCH8` / `RTC`
- `GPIO3` / `ADC1_2` / `TOUCH3` / `RTC`
- `GPIO46` / `LOG`
- `GPIO9` / `ADC1_8` / `TOUCH9` / `RTC`
- `GPIO10` / `ADC1_9` / `TOUCH10` / `RTC`
- `GPIO11` / `ADC1_0` / `TOUCH11` / `RTC`
- `GPIO12` / `ADC1_1` / `TOUCH12` / `RTC`
- `GPIO13` / `ADC1_2` / `TOUCH13` / `RTC`
- `GPIO14` / `ADC1_3` / `TOUCH14` / `RTC`
- `5V0`
- `GND`

### 10.2 Right Header

Top to bottom:

- `GND`
- `U0TXD` / `GPIO43` / `CLK_OUT1`
- `U0RXD` / `GPIO44` / `CLK_OUT2`
- `GPIO1`
- `GPIO2`
- `MTMS` / `GPIO42`
- `MTDI` / `GPIO41` / `CLK_OUT1`
- `MTDO` / `GPIO40` / `CLK_OUT2`
- `MTCK` / `GPIO39` / `CLK_OUT3` / `SUBSPICS1`
- `GPIO38` / `FSPIWP` / `SUBSPIWP`
- `GPIO37` / `SPIDQS` / `FSPIQ` / `SUBSPIQ`
- `GPIO36` / `SPIIO7` / `FSPICLK` / `SUBSPICLK`
- `GPIO35` / `SPIIO6` / `FSPID` / `SUBSPID`
- `GPIO0` / `BOOT`
- `GPIO45` / `VSPI`
- `GPIO48` / `SPICLK_N` / `RGB_LED`
- `GPIO47` / `SPICLK_P`
- `GPIO21` / `RTC`
- `USB_D+` / `GPIO20` / `RTC` / `U1CTS` / `ADC2_9`
- `USB_D-` / `GPIO19` / `RTC` / `U1RTS` / `ADC2_8`
- `GND`
- `GND`

## 11. Pin Usage Constraints

### 11.1 Occupied or Sensitive Pins

- `GPIO48`: onboard WS2812 RGB LED
- `GPIO0`: boot strap pin
- `GPIO45`: strap-related and commonly involved in boot / peripheral configuration
- `GPIO46`: log-related pin on the published pinout
- `GPIO35` to `GPIO38`: tied to flash/PSRAM-related functions on the published pinout
- `GPIO19` and `GPIO20`: native USB data pins on the published pinout
- `GPIO39` to `GPIO42`: JTAG-related pins

### 11.2 Design Implications

- Do not plan critical external pull-ups or pull-downs on strap pins without checking boot behavior.
- Avoid reassigning flash/PSRAM-related pins unless you have verified the exact module schematic and memory routing.
- Use `GPIO48` only if you are prepared to lose the onboard RGB LED.

## 12. Recommended Project Fit

This board is a strong fit for:

- Wi-Fi connected sensors and controllers
- Bluetooth Low Energy devices
- status-rich embedded interfaces
- LED feedback systems
- display-driven dashboards
- networked automation nodes
- audio and UI projects that benefit from PSRAM
- OTA-capable firmware deployments

## 13. Notable Strengths

- External antenna path improves RF flexibility.
- N16R8 memory configuration is generous for ESP32-S3 class projects.
- Two USB-C ports simplify development and deployment workflows.
- Onboard WS2812 LED provides immediate visual feedback.
- Board exposes a broad set of GPIO and peripheral functions.

## 14. Design Caveats

- External antenna is required for wireless use.
- The onboard RGB LED occupies `GPIO48`.
- Some exposed pins are tied to boot or high-speed internal functions.
- Pin reuse should be checked against the module and board schematic before hardware design.

## 15. Primary References

- Lonely Binary product page for the external antenna board: [https://lonelybinary.com/products/esp32-s3-ipex?variant=43699253674141](https://lonelybinary.com/products/esp32-s3-ipex?variant=43699253674141)
- Lonely Binary pinout image for the N16R8 IPEX board: [https://lonelybinary.com/cdn/shop/files/LB-ESP32S3-N16R8-IPEX-modifed.jpg?v=1755829941](https://lonelybinary.com/cdn/shop/files/LB-ESP32S3-N16R8-IPEX-modifed.jpg?v=1755829941)
- Lonely Binary WS2812 LED tutorial: [https://lonelybinary.com/en-us/blogs/micropython-magic-with-esp32/03-project-blinking-the-built-in-ws2812-led](https://lonelybinary.com/en-us/blogs/micropython-magic-with-esp32/03-project-blinking-the-built-in-ws2812-led)
- Espressif ESP32-S3 product overview: [https://www.espressif.com/en/node/4993](https://www.espressif.com/en/node/4993)
- Espressif ESP32-S3-WROOM-1 module page: [https://www.espressif.com/en/products/modules/esp32-s3/esp32-s3-wroom-1](https://www.espressif.com/en/products/modules/esp32-s3/esp32-s3-wroom-1)
- Espressif ESP32-S3 hardware design guidelines: [https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32s3/esp-hardware-design-guidelines-en-master-esp32s3.pdf](https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32s3/esp-hardware-design-guidelines-en-master-esp32s3.pdf)
