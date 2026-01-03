#!/usr/bin/env python3

import socket
import statistics
import struct
import time

# KONFIGURACJA
TARGET_IP = "192.168.50.2"  # Wpisz IP swojego ESP32 (Ethernet lub WiFi)
TARGET_PORT = 5004
COUNT = 1000  # Liczba próbek

# Przygotowanie gniazda
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(1.0)


# Struktura pakietu MIDI Network (AppleMIDI / RTP-MIDI)
# Signature (4s), Command (H), Sequence (H), Timestamp (I), SSRC (I)
# CMD 0x20 = IN_SYNC (PING)
def build_ping_packet(seq, timestamp):
    # Format:
    # >      Big Endian
    # 4s     "MIDI" (4 bytes)
    # B      Command Code (1 byte) -> 0x20 (PING)
    # B      Payload Length words (1 byte) -> 1
    # H      Command Specific Data / Sequence (2 bytes)
    # I      Payload Word / Timestamp (4 bytes)

    return struct.pack(">4sBBHI", b"MIDI", 0x20, 1, seq, timestamp)


rtt_values = []

print(f"Rozpoczynam test opóźnień dla {TARGET_IP}...")

for seq in range(COUNT):
    try:
        # 1. Pobierz czas startu (w nanosekundach dla precyzji)
        t_start = time.perf_counter_ns()

        # 2. Wyślij pakiet (Timestamp w nagłówku jest tu dummy, liczymy czas hosta)
        packet = build_ping_packet(seq, 0)
        sock.sendto(packet, (TARGET_IP, TARGET_PORT))

        # 3. Odbierz odpowiedź
        data, addr = sock.recvfrom(1024)
        t_end = time.perf_counter_ns()

        # 4. Oblicz RTT w milisekundach
        rtt_ms = (t_end - t_start) / 1_000_000
        rtt_values.append(rtt_ms)

        # Krótka pauza, żeby nie zalać sieci (opcjonalnie)
        time.sleep(0.005)

    except socket.timeout:
        print(f"Timeout dla pakietu {seq}")
    except Exception as e:
        print(f"Błąd: {e}")

# WYNIKI
if rtt_values:
    min_val = min(rtt_values)
    avg_val = statistics.mean(rtt_values)
    max_val = max(rtt_values)
    jitter = statistics.stdev(rtt_values)

    print("-" * 30)
    print(f"Wyniki dla {COUNT} próbek:")
    print(f"Min: {min_val:.3f} ms")
    print(f"Max: {max_val:.3f} ms")
    print(f"Średnia: {avg_val:.3f} ms")
    print(f"Jitter (StDev): {jitter:.3f} ms")
    print("-" * 30)
else:
    print("Brak danych pomiarowych.")