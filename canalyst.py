## In file canalyst.py
import can
import sys


def main():
    # Configuration
    CHANNEL = 1      # 0 = CAN1, 1 = CAN2
    BITRATE = 250000

    try:
        # Initialize CAN bus
        bus = can.Bus(
            interface="canalystii",
            channel=CHANNEL,
            bitrate=BITRATE
        )
    except Exception as e:
        print(f"Failed to initialize CAN interface: {e}")
        sys.exit(1)

    print(f"Listening on CANalyst-II channel {CHANNEL} @ {BITRATE} bps")

    try:
        count = 0
        while True:
            
            msg = can.Message(
            
                    arbitration_id=0xC0FFEE, data=[count % 256], is_extended_id=True
            
                    )
           
            #bus.send(msg)
            msg = bus.recv(timeout=1.0)

            if msg is None:
                continue
            count+=1
            # Format data bytes
            data_str = " ".join(f"{byte:02X}" for byte in msg.data)

            print(
                f"Count ={count} | "
                f"RX ID=0x{msg.arbitration_id:08X} | "
                f"DLC={msg.dlc} | "
                f"EXT={msg.is_extended_id} | "
                f"DATA={data_str}"
            )

    except KeyboardInterrupt:
        print("\nStopped by user")

    except can.CanError as e:
        print(f"CAN Error: {e}")

    except Exception as e:
        print(f"Unexpected error: {e}")

    finally:
        try:
            bus.shutdown()
        except Exception:
            pass
        print("Done.")


if __name__ == "__main__":
    main()