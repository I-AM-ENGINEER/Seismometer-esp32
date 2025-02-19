#!/usr/bin/env python3
import argparse
import serial
import serial.serialutil
import time
import sys
import threading
from collections import deque

# Global variables for statistics
capturing = False
capture_start_time = None
bytes_captured = 0
lines_captured = 0
last_10_lines = deque(maxlen=10)
lock = threading.Lock()
stop_event = threading.Event()


def display_stats():
    """
    Background thread: clear the console every second and display updated statistics,
    including the last 10 captured messages with timestamps.
    """
    while not stop_event.is_set():
        if capturing:
            with lock:
                elapsed = time.time() - capture_start_time if capture_start_time else 0
                stats = (f"Bytes captured: {bytes_captured}, "
                         f"Lines captured: {lines_captured}, "
                         f"Time elapsed: {elapsed:.2f} sec")
                # Copy last 10 messages for display
                last_msgs = list(last_10_lines)
            # Clear the console (ANSI escape sequence)
            sys.stdout.write("\033[2J\033[H")
            sys.stdout.write(stats + "\n")
            sys.stdout.write("Last 10 messages:\n")
            for t_stamp, msg in last_msgs:
                sys.stdout.write(f"[{t_stamp:.2f} sec] {msg}\n")
            sys.stdout.flush()
        time.sleep(1)


def open_serial_port(com_port, baudrate=115200, timeout=1):
    """
    Open the serial port. If the device is not present, wait and retry until connected.
    """
    while True:
        try:
            ser = serial.Serial(com_port, baudrate=baudrate, timeout=timeout)
            print("Device connected!")
            return ser
        except serial.serialutil.SerialException:
            print("Device not found. Waiting for device to be connected...")
            time.sleep(1)


def main():
    global capturing, capture_start_time, bytes_captured, lines_captured, last_10_lines

    parser = argparse.ArgumentParser(
        description="Capture data from a hot-plug USB CDC device. "
                    "Capture starts upon receiving '***START LOG***' and ends when "
                    "'***STOP LOG***' is received, or when max capture time/lines is reached."
    )
    parser.add_argument('com_port', help='COM port device (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('output_file', help='Output file for captured log')
    parser.add_argument('--max_capture_time', type=int, default=0,
                        help='Maximum capture time in seconds (0 for unlimited)')
    parser.add_argument('--max_captured_lines', type=int, default=0,
                        help='Maximum captured lines (0 for unlimited)')
    args = parser.parse_args()

    # Treat 0 as unlimited (None)
    max_capture_time = args.max_capture_time if args.max_capture_time > 0 else None
    max_captured_lines = args.max_captured_lines if args.max_captured_lines > 0 else None

    # Wait until the device is connected
    ser = open_serial_port(args.com_port, baudrate=115200, timeout=1)

    # Start background thread to display statistics
    status_thread = threading.Thread(target=display_stats)
    status_thread.daemon = True
    status_thread.start()

    with open(args.output_file, 'w') as f:
        print("Waiting for log start (looking for '***START LOG***')...")
        while True:
            try:
                line = ser.readline()
            except serial.SerialException:
                print("Device disconnected. Exiting capture.")
                break
            except Exception as e:
                print(f"Error reading from device: {e}")
                break

            if not line:
                continue

            try:
                line_str = line.decode('utf-8')
            except Exception:
                line_str = line.decode('utf-8', errors='replace')

            # Remove extra whitespace and terminators
            line_clean = line_str.strip()

            if not capturing:
                if line_clean == "***START LOG***":
                    with lock:
                        capturing = True
                        capture_start_time = time.time()
                    print("Capture started.")
                else:
                    # Ignore lines until the start marker is received
                    continue
            else:
                if line_clean == "***STOP LOG***":
                    print("Received STOP LOG command.")
                    break

                # Write cleaned line (with a uniform newline) to the output file.
                f.write(line_clean + "\n")
                f.flush()

                with lock:
                    lines_captured += 1
                    bytes_captured += len(line)
                    elapsed = time.time() - capture_start_time
                    last_10_lines.append((elapsed, line_clean))

                if max_captured_lines is not None and lines_captured >= max_captured_lines:
                    print("Maximum captured lines reached.")
                    break
                if max_capture_time is not None and elapsed >= max_capture_time:
                    print("Maximum capture time reached.")
                    break

    # Signal the stats thread to stop and wait for it
    stop_event.set()
    status_thread.join()
    try:
        ser.close()
    except Exception:
        pass

    total_time = time.time() - capture_start_time if capture_start_time else 0
    print("Capture finished.")
    print(f"Total bytes captured: {bytes_captured}, "
          f"Total lines captured: {lines_captured}, "
          f"Total time: {total_time:.2f} seconds")
    print("Last 10 messages:")
    for t_stamp, msg in last_10_lines:
        print(f"[{t_stamp:.2f} sec] {msg}")


if __name__ == '__main__':
    main()
