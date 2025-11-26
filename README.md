# TODO

- STYLE THE CODE!!!!!
- PCB
- optimize for latency
- fix looks of qsynth and logic

## midi_core

- Add Active Sensing support
- Add more robust System Exclusive support 
- add length to midi message type for easier serializing

## midi_uart

- add optocoupler to eliminate noise on rx
- add test melody loop to transmiter

## midi_wifi

- add wifi provisioning(avoid hardcoding wifi credentials)

## midi_network

- add authentication option to MIDI2 session
- add ipv6 support
- client mode (???)
- FEC
- ump stream responder.h check

---

- [x] UART RX -> UART TX 
- [x] UART RX -> USB 
- [x] UART RX -> Network

- [x] USB -> UART TX (amidiplay -> logic analyzer)
- [x] USB -> Network (amidiplay -> pymidi2)

- [x] Network -> UART TX (pymidi2 -> logic analyzer)
- [x] Network -> USB (pymidi2 -> qsynth)
 
