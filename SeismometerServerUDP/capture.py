#!/usr/bin/env python3
import argparse
import serial
import time
import sys
import threading
from collections import deque

# Global variables for statistics (they will be shared between threads)
capturing = False
capture_start_time = None
bytes_captured = 0
lines_captured = 0
last_10_lines = deque(maxlen=10)
lock = threading.Lock()
stop_event = threading.Event()


def display_stats():
    """
    This function runs in a separate thread.
    Every second it clears the console and prints updated statistics along with the
    last 10 captured messages (each with the elapsed time from the log start).
    """
    while not stop_event.is_set():
        if capturing:
            with lock:
                elapsed = time.time() - capture_start_time if capture_start_time else 0
                stats = (f"Bytes captured: {bytes_captured}, "
                         f"Lines captured: {lines_captured}, "
                         f"Time elapsed: {elapsed:.2f} sec")
                # Create a local copy of the last 10 messages
                last_msgs = list(last_10_lines)
            # Clear screen (ANSI escape sequence)
            sys.stdout.write("\033[2J\033[H")
            sys.stdout.write(stats + "\n")
            sys.stdout.write("Last 10 messages:\n")
            for t, msg in last_msgs:
                sys.stdout.write(f"[{t:.2f} sec] {msg}\n")
            sys.stdout.flush()
        time.sleep(1)


def main():
    global capturing, capture_start_time, bytes_captured, lines_captured, last_10_lines

    parser = argparse.ArgumentParser(
        description="Capture data from CDC USB device. "
                    "Capture starts upon receiving '***START LOG***\\n' and ends when "
                    "'***STOP LOG***\\n' is received, or when max capture time/lines is reached."
    )
    parser.add_argument('com_port', help='COM port device (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('output_file', help='Output file for captured log')
    parser.add_argument('--max_capture_time', type=int, default=0,
                        help='Maximum capture time in seconds (0 for unlimited)')
    parser.add_argument('--max_captured_lines', type=int, default=0,
                        help='Maximum captured lines (0 for unlimited)')
    args = parser.parse_args()

    # If 0 is passed then treat as unlimited (None)
    max_capture_time = args.max_capture_time if args.max_capture_time > 0 else None
    max_captured_lines = args.max_captured_lines if args.max_captured_lines > 0 else None

    try:
        # Adjust baudrate and timeout as necessary for your device
        ser = serial.Serial(args.com_port, baudrate=115200, timeout=1)
    except Exception as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    # Start the background thread to display stats
    status_thread = threading.Thread(target=display_stats)
    status_thread.daemon = True
    status_thread.start()

    with open(args.output_file, 'w') as f:
        print("Waiting for log start (looking for '***START LOG***')...")
        while True:
            try:
                # Read a line from the serial port (blocking until timeout)
                line = ser.readline()
                if not line:
                    continue  # No data received in this cycle
                try:
                    line_str = line.decode('utf-8')
                except Exception:
                    line_str = line.decode('utf-8', errors='replace')

                if not capturing:
                    if line_str == "***START LOG***\n":
                        with lock:
                            capturing = True
                            capture_start_time = time.time()
                        print("Capture started.")
                    else:
                        # Ignore lines until the start marker is received
                        continue
                else:
                    # If we see the stop marker, then finish capture.
                    if line_str == "***STOP LOG***\n":
                        print("Received STOP LOG command.")
                        break

                    # Write the received line to the output file.
                    f.write(line_str)
                    f.flush()

                    # Update statistics
                    with lock:
                        lines_captured += 1
                        bytes_captured += len(line)
                        elapsed = time.time() - capture_start_time
                        last_10_lines.append((elapsed, line_str.strip()))

                    # Check if we've reached a specified capture limit.
                    if max_captured_lines is not None and lines_captured >= max_captured_lines:
                        print("Maximum captured lines reached.")
                        break
                    if max_capture_time is not None and elapsed >= max_capture_time:
                        print("Maximum capture time reached.")
                        break

            except KeyboardInterrupt:
                print("KeyboardInterrupt detected, stopping capture.")
                break

    # Signal the status thread to stop and wait for it to finish.
    stop_event.set()
    status_thread.join()
    ser.close()

    total_time = time.time() - capture_start_time if capture_start_time else 0
    print("Capture finished.")
    print(f"Total bytes captured: {bytes_captured}, "
          f"Total lines captured: {lines_captured}, "
          f"Total time: {total_time:.2f} seconds")
    print("Last 10 messages:")
    for t, msg in last_10_lines:
        print(f"[{t:.2f} sec] {msg}")


if __name__ == '__main__':
    main()
