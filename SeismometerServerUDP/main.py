import socket
import time
import threading

PACKET_SIZE = 1472

class UDPSpeedMonitor:
    def __init__(self, host='0.0.0.0', port=1234):
        self.host = host
        self.port = port
        self.packet_count = 0
        self.start_time = time.time()
        self.lock = threading.Lock()

    def start_server(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server_socket:
            server_socket.bind((self.host, self.port))
            print(f"Server listening on {self.host}:{self.port}")
            while True:
                data, _ = server_socket.recvfrom(PACKET_SIZE)
                self.process_packet(data)

    def process_packet(self, data):
        with self.lock:
            self.packet_count += 1

    def get_statistics(self):
        elapsed_time = time.time() - self.start_time
        packets_per_second = self.packet_count / elapsed_time
        incoming_speed = self.packet_count * PACKET_SIZE / elapsed_time
        return self.packet_count, packets_per_second, incoming_speed

    def display_statistics(self):
        while True:
            time.sleep(3)
            packet_count, packets_per_second, incoming_speed = self.get_statistics()
            print(f"Packets received: {packet_count}, "
                  f"Packets per second: {packets_per_second:.2f}, "
                  f"Incoming speed: {incoming_speed*8/1024/1024:.2f} mbit/sec")
            with self.lock:
                self.packet_count = 0
                self.start_time = time.time()

if __name__ == '__main__':
    monitor = UDPSpeedMonitor(host='0.0.0.0', port=1235)

    server_thread = threading.Thread(target=monitor.start_server, daemon=True)
    server_thread.start()

    stats_thread = threading.Thread(target=monitor.display_statistics, daemon=True)
    stats_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Server stopped.")
