#!/usr/bin/env python3
import can
import time
import threading
import sys

"""
PCAN-USB Pro FD Verification Script (SocketCAN Loopback)

Prerequisites:
- Linux Host
- PEAK-System driver loaded (should auto-load via USB VID/PID)
- Network interfaces can0 and can1 up
  sudo ip link set can0 up type can bitrate 500000
  sudo ip link set can1 up type can bitrate 500000
- Hardware loopback: Connect CAN1 H/L to CAN2 H/L
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
        bus1 = can.interface.Bus(channel='can0', bustype='socketcan')
        bus2 = can.interface.Bus(channel='can1', bustype='socketcan')
    except OSError as e:
        print(f"Error opening CAN interfaces: {e}")
        print("Ensure interfaces are up: 'sudo ip link set can0 up type can bitrate 500000'")
        sys.exit(1)

    stop_event = threading.Event()
    received_on_bus2 = set()
    received_on_bus1 = set()

    # Start receivers
    t2 = threading.Thread(target=receive_worker, args=(bus2, stop_event, [0x123], received_on_bus2))
    t1 = threading.Thread(target=receive_worker, args=(bus1, stop_event, [0x456], received_on_bus1))
    t2.start()
    t1.start()

    time.sleep(1)

    # Test 1: Send on CAN1 (ID 0x123), Expect on CAN2
    print("\n--- Test 1: CAN1 -> CAN2 (Classic Frame) ---")
    msg1 = can.Message(arbitration_id=0x123, data=[0x11, 0x22, 0x33, 0x44], is_extended_id=False)
    try:
        bus1.send(msg1)
        print(f"[{bus1.channel_info}] TX: {msg1}")
    except can.CanError:
        print("TX Error on CAN1")

    time.sleep(0.5)

    if 0x123 in received_on_bus2:
        print("PASS: CAN2 received message from CAN1")
    else:
        print("FAIL: CAN2 did not receive message from CAN1")

    # Test 2: Send on CAN2 (ID 0x456), Expect on CAN1
    print("\n--- Test 2: CAN2 -> CAN1 (Classic Frame) ---")
    msg2 = can.Message(arbitration_id=0x456, data=[0xAA, 0xBB, 0xCC], is_extended_id=False)
    try:
        bus2.send(msg2)
        print(f"[{bus2.channel_info}] TX: {msg2}")
    except can.CanError:
        print("TX Error on CAN2")

    time.sleep(0.5)

    if 0x456 in received_on_bus1:
        print("PASS: CAN1 received message from CAN2")
    else:
        print("FAIL: CAN1 did not receive message from CAN2")

    stop_event.set()
    t1.join()
    t2.join()
    
    bus1.shutdown()
    bus2.shutdown()

    if 0x123 in received_on_bus2 and 0x456 in received_on_bus1:
        print("\n=== SUCCESS: Bidirectional Communication Verified ===")
        sys.exit(0)
    else:
        print("\n=== FAILURE: Communication Test Failed ===")
        sys.exit(1)

if __name__ == "__main__":
    test_bidirectional_loopback()
