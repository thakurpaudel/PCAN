#!/bin/bash
#
# cansend-repeated.sh
# Send COUNT CAN frames with all payload bytes filled

IFACE="${1}"
CAN_ID="${2}"
COUNT="${3}"
DELAY="${4:-0.001}"
DATA_BYTES="${5:-8}"

if [[ -z "$IFACE" || -z "$CAN_ID" || -z "$COUNT" ]]; then
    echo "Usage: $0 <iface> <can_id> <count> [delay_s=0.001] [data_bytes=8]"
    exit 1
fi

if (( DATA_BYTES < 1 || DATA_BYTES > 8 )); then
    echo "ERROR: data_bytes must be 1-8"
    exit 1
fi

echo "Sending $COUNT frames on $IFACE ID=$CAN_ID payload=${DATA_BYTES}B delay=${DELAY}s"

for ((i=0; i<COUNT; i++)); do
    total=$((i+1))
    data=""

    for ((b=0; b<DATA_BYTES; b++)); do
        byte_val=$(( (i + b) & 0xFF ))
        printf -v hex_byte "%02X" "$byte_val"
        data="${data}${hex_byte}"
    done

    if (( total % 100 == 0 )); then
        echo "[$total/$COUNT] cansend $IFACE ${CAN_ID}#${data}"
    fi

    cansend "$IFACE" "${CAN_ID}#${data}"
    sleep "$DELAY"
done

echo "Done. Sent $COUNT frames."