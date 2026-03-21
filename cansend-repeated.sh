#!/bin/bash
#
# cansend-repeated.sh - Send COUNT CAN frames with incrementing data
#
# Usage: ./cansend-repeated.sh <iface> <can_id> <count> [delay_s] [data_bytes]
#
#   iface      - CAN interface, e.g. can0
#   can_id     - CAN ID in hex, e.g. 810FBFA  (extended) or 7FF (standard)
#   count      - Number of frames to send
#   delay_s    - Delay between frames in seconds (default: 0.001 = 1ms)
#   data_bytes - Payload size in bytes: 1-8 (default: 4)
#
# Example (8-byte payload, 1ms gap):
#   ./cansend-repeated.sh can0 810FBFA 500 0.001 8

IFACE="${1}"
CAN_ID="${2}"
COUNT="${3}"
DELAY="${4:-0.001}"      # default 1 ms between frames
DATA_BYTES="${5:-4}"     # default 4 bytes

if [[ -z "$IFACE" || -z "$CAN_ID" || -z "$COUNT" ]]; then
    echo "Usage: $0 <iface> <can_id> <count> [delay_s=0.001] [data_bytes=4]"
    echo "  data_bytes: 1-8 for classic CAN"
    exit 1
fi

if (( DATA_BYTES < 1 || DATA_BYTES > 8 )); then
    echo "ERROR: data_bytes must be 1-8 for classic CAN (got $DATA_BYTES)"
    exit 1
fi

echo "Sending $COUNT frames on $IFACE  ID=$CAN_ID  payload=${DATA_BYTES}B  delay=${DELAY}s"

for ((i=0; i<COUNT; i++)); do
    total=$((i+1))

    # Build hex payload of exactly DATA_BYTES bytes.
    # Counter value goes into the LAST byte so byte[0] stays 0x00 for small i.
    # Each byte is 2 hex chars; total hex string = DATA_BYTES*2 chars.
    data=""
    for ((b=0; b<DATA_BYTES; b++)); do
        byte_index=$(( DATA_BYTES - 1 - b ))  # which byte position (0=last)
        # Shift counter value into this byte slot
        byte_val=$(( (i >> (byte_index * 8)) & 0xFF ))
        printf -v hex_byte "%02X" "$byte_val"
        data="${hex_byte}${data}"
    done

    if (( total % 100 == 0 )); then
        echo "[$total/$COUNT] cansend $IFACE ${CAN_ID}#${data}"
    fi

    cansend "$IFACE" "${CAN_ID}#${data}"
    sleep "$DELAY"
done

echo "Done. Sent $COUNT frames."