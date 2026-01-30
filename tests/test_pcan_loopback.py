#!/usr/bin/env python3
import can
import time
import threading
import sys

"""
PCAN-USB Pro FD Verification Script (SocketCAN Loopback)
"""

def receive_worker(bus, stop_event, expected_ids, received_ids):
    while not stop_event.is_set():
        msg = bus.recv(timeout=0.1)
        if msg:
            print(f"[{bus.channel_info}] RX: {msg}")
            if msg.arbitration_id in expected_ids:
                received_ids.add(msg.arbitration_id)

def test_bidirectional_loopback():
    print("Initializing CAN interfaces (can0, can1)...")
    try:
        bus1 = can.Bus(interface='socketcan', channel='can0')
        bus2 = can.Bus(interface='socketcan', channel='can1')
    except OSError as e:
        print(f"Error opening CAN interfaces: {e}")
        sys.exit(1)

    stop_event = threading.Event()
    received_on_bus2 = set()
    received_on_bus1 = set()

    t2 = threading.Thread(target=receive_worker, args=(bus2, stop_event, [0x123], received_on_bus2))
    t1 = threading.Thread(target=receive_worker, args=(bus1, stop_event, [0x456], received_on_bus1))
    t2.start()
    t1.start()

    time.sleep(1)

    print("\n--- Test 1: CAN1 -> CAN2 ---")
    msg1 = can.Message(
        arbitration_id=0x123,
        data=[0x11, 0x22, 0x33, 0x44],
        is_extended_id=False
    )
    bus1.send(msg1)
    print(f"[{bus1.channel_info}] TX: {msg1}")

    time.sleep(0.5)

    print("PASS" if 0x123 in received_on_bus2 else "FAIL")

    print("\n--- Test 2: CAN2 -> CAN1 ---")
    msg2 = can.Message(
        arbitration_id=0x456,
        data=[0xAA, 0xBB, 0xCC],
        is_extended_id=False
    )
    bus2.send(msg2)
    print(f"[{bus2.channel_info}] TX: {msg2}")

    time.sleep(0.5)

    print("PASS" if 0x456 in received_on_bus1 else "FAIL")

    stop_event.set()
    t1.join()
    t2.join()
    bus1.shutdown()
    bus2.shutdown()

    if 0x123 in received_on_bus2 and 0x456 in received_on_bus1:
        print("\n=== SUCCESS: Bidirectional Communication Verified ===")
        sys.exit(0)
    else:
        print("\n=== FAILURE ===")
        sys.exit(1)

if __name__ == "__main__":
    test_bidirectional_loopback()
