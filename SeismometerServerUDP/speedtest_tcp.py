import socket
import time

def tcp_server(host='0.0.0.0', port=12345):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((host, port))
        server_socket.listen()
        print(f"Server listening on {host}:{port}")

        while True:
            conn, addr = server_socket.accept()
            with conn:
                print(f"Connection from {addr}")
                while True:
                    data = conn.recv(10240)
                    if not data:
                        break
                    arrival_time = time.time()
                    print(f"Received {len(data)} bytes at {arrival_time:.6f} sec")

if __name__ == "__main__":
    tcp_server(host='0.0.0.0', port=1235)
