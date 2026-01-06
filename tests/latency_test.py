#!/usr/bin/env python3

import socket
import statistics
import struct
import time

TARGET_IP = "192.168.50.2"
TARGET_PORT = 5004
COUNT = 1000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(1.0)

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

print(f"Beginning latency test for {TARGET_IP}...")

for seq in range(COUNT):
    try:
        # 1. Measure time before sending
        t_start = time.perf_counter_ns()

        # 2. Send ping packet
        packet = build_ping_packet(seq, 0)
        sock.sendto(packet, (TARGET_IP, TARGET_PORT))

        # 3. Receive response
        data, addr = sock.recvfrom(1024)
        t_end = time.perf_counter_ns()

        # 4. Calculate RTT in milliseconds
        rtt_ms = (t_end - t_start) / 1_000_000
        rtt_values.append(rtt_ms)

        # 5. Short delay between packets
        time.sleep(0.005)

    except socket.timeout:
        print(f"Timeout for a packet number {seq}")
    except Exception as e:
        print(f"Error: {e}")

if rtt_values:
    min_val = min(rtt_values)
    avg_val = statistics.mean(rtt_values)
    max_val = max(rtt_values)

    print("-" * 30)
    print(f"Results for {COUNT} samples:")
    print(f"Min: {min_val:.3f} ms")
    print(f"Max: {max_val:.3f} ms")
    print(f"Average: {avg_val:.3f} ms")
    print("-" * 30)
else:
    print("No measurement data available.")