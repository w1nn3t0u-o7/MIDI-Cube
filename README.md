# MIDI Cube

**MIDI Cube** is a multi-transport MIDI Interface built on the ESP32-S3 platform. It bridges the gap between MIDI 1.0 devices and the modern MIDI 2.0 ecosystem.

The project seamlessly routes and translates MIDI messages between:
- **USB** (Device Mode via TinyUSB)
- **UART** (Classic DIN-5 MIDI 1.0)
- **Network** (MIDI 2.0 UMP over UDP via WiFi or Ethernet)

## 🚀 Features

- **Multi-Transport Support:**
  - **USB MIDI:** Acts as a standard USB MIDI Class compliant device.
  - **Serial MIDI:** Classic 31.250 baud MIDI over UART (configurable pins).
  - **Network MIDI 2.0:** UMP (Universal MIDI Packet) transport over UDP with session management and mDNS discovery (`_midi2._udp`).
  - **Ethernet:** Low-latency wired connection support using W5500 (SPI).
  - **WiFi:** Wireless MIDI support for portable setups.

- **Intelligent Routing & Translation:**
  - **Centralized Router:** Handles message passing between all active interfaces using FreeRTOS queues.
  - **Bi-directional Translation:** Automatically converts between MIDI 1.0 byte streams and MIDI 2.0 UMP packets.
  - **High-Resolution Scaling:** Implements MIDI 2.0 Min-Center-Max algorithms for accurate upscaling of 7-bit values to 16/32-bit resolution.

- **Configurable:**
  - Full configuration via Kconfig (WiFi credentials, Pin mapping, IP settings).

## 🛠️ Hardware Requirements

- **MCU:** Espressif ESP32-S3
- **Ethernet:** WIZnet W5500 (SPI Interface)

## 📂 Project Structure

The project is structured as an ESP-IDF component-based application:

```text
├── main/                   # Application entry point
├── components/
│   ├── midi_core/          # Parsers, message definitions, and UMP handling
│   ├── midi_router/        # Routing logic and queues
│   ├── midi_translator/    # MIDI 1.0 <-> MIDI 2.0 translation logic
│   ├── midi_uart/          # UART driver for DIN MIDI
│   ├── midi_usb_device/    # TinyUSB wrapper
│   ├── midi_network/       # UDP transport and session management
│   ├── midi_wifi/          # WiFi station manager
│   └── midi_ethernet/      # W5500 Ethernet driver

```

## ⚙️ Configuration

Use the ESP-IDF configuration menu to set up your environment:

```bash
idf.py menuconfig

```

### Key Configuration Options:

1. **MIDI WiFi Configuration:**
* Set `SSID` and `Password` for your network.


2. **MIDI Ethernet Configuration:**
* Choose between `DHCP` (Router) or `Static IP` (Direct PC connection).


3. **MIDI UART Configuration:**
* Default TX Pin: `GPIO 17`
* Default RX Pin: `GPIO 16`
* Baud Rate: `31250`


4. **MIDI Network:**
* Default Port: `5004`



## 🏗️ Build and Flash

This project requires **ESP-IDF v6.0.0**.

1. **Build the project:**
```bash
idf.py build

```


2. **Flash to ESP32-S3:**
```bash
idf.py -p (PORT) flash

```


3. **Monitor Output:**
```bash
idf.py monitor

```



## 🔌 Interfacing

### Network Discovery

The device advertises itself via mDNS as `midi-cube._midi2._udp.local`. You can discover it using MIDI 2.0 capable software or tools that support Network MIDI discovery.

### USB

Connect the ESP32-S3 USB port to your computer. It will appear as "MIDI Cube - USB MIDI Device".

## ✅ TODO / Roadmap

* [ ] **Code Style:** Refactor and standardize code style.
* [ ] **PCB Design:** Create a dedicated PCB layout.
* [ ] **Latency:** Optimization for critical paths.
* [ ] **MIDI Core:**
* Add Active Sensing support.
* Robust System Exclusive (SysEx) support.


* [ ] **MIDI UART:** Hardware optocoupler integration for noise elimination.
* [ ] **Network:**
* Add authentication to MIDI 2.0 sessions.
* IPv6 support.
* Forward Error Correction (FEC).


```
 
