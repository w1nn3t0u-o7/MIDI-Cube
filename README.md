# MIDI Cube

**MIDI Cube** is a multi-transport MIDI Interface built on the ESP32-S3 platform. It bridges the gap between MIDI 1.0 devices and the modern MIDI 2.0 ecosystem.

The project seamlessly routes and translates MIDI messages between:
- **USB** (Device Mode via TinyUSB)
- **UART** (Classic DIN-5 MIDI 1.0)
- **Network** (MIDI 2.0 UMP over UDP via WiFi or Ethernet)

## Features

- **USB MIDI:** Acts as a standard USB MIDI Class compliant device.
- **Serial MIDI:** Classic 31.250 baud MIDI over UART.
- **Network MIDI 2.0:** UMP (Universal MIDI Packet) transport over UDP with session management and mDNS discovery (`_midi2._udp`).
- **Ethernet:** Low-latency wired connection support using W5500.
- **WiFi:** Wireless MIDI support for portable setups.
- **Bi-directional Translation:** Automatically converts between MIDI 1.0 byte streams and MIDI 2.0 UMP packets.

## Hardware Requirements

- **MCU:** Espressif ESP32-S3
- **Ethernet:** WIZnet W5500


## Configuration

Use the ESP-IDF configuration menu to set up your environment:

```bash
idf.py menuconfig

```

### Key Configuration Options:

1. **MIDI WiFi Configuration:**
* Set `SSID` and `Password` for your network.


2. **MIDI Ethernet Configuration:**
* Choose between `DHCP` or `Static IP`.


3. **MIDI UART Configuration:**
* Default TX Pin: `GPIO 17`
* Default RX Pin: `GPIO 16`
* Baud Rate: `31250`


4. **MIDI Network:**
* Default Port: `5004`



## Build and Flash

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

## TODO

* [ ] **Code Style:** Refactor and standardize code style.
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
 
