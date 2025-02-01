import socket
import wave
import struct

# Configuration
TCP_IP  = "0.0.0.0"  # Listen on all network interfaces
TCP_PORT = 1235      # UDP port to listen on
PACKET_SIZE = 1460   # Size of incoming UDP packets
SAMPLE_RATE = 8000  # I2S sample rate (adjust to match I2S_SAMPLERATE)
BITS_PER_SAMPLE = 24 # I2S bit depth
CHANNELS = 2         # Stereo audio

# WAV file parameters
WAV_FILENAME = "output_audio.wav"

def unpack_24bit_to_32bit(data):
    samples = []
    for i in range(0, len(data), 3):
        sample = struct.unpack('<i', data[i:i+3] + (b'\x00' if data[i+2] & 0x80 == 0 else b'\xFF'))[0]
        samples.append(sample)
    return samples


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((TCP_IP, TCP_PORT))
    sock.listen(1)
    print(f"Listening on TCP port {TCP_PORT}...")

    with wave.open(WAV_FILENAME, 'wb') as wav_file:
        wav_file.setnchannels(CHANNELS)
        wav_file.setsampwidth(2)  # 16-bit output
        wav_file.setframerate(SAMPLE_RATE)

        try:
            conn, addr = sock.accept()
            print(f"Connection established with {addr}")

            with conn:
                while True:
                    data = conn.recv(PACKET_SIZE)
                    if not data:
                        print("Client disconnected.")
                        break

                    if len(data) % (3 * CHANNELS) != 0:
                        print(f"Warning: Received packet with invalid length {len(data)}")
                        continue

                    samples = unpack_24bit_to_32bit(data)

                    left_channel = samples[0::2]
                    right_channel = samples[1::2]

                    left_channel_16bit = [sample >> 8 for sample in left_channel]
                    right_channel_16bit = [sample >> 8 for sample in right_channel]

                    interleaved = [val for pair in zip(left_channel_16bit, right_channel_16bit) for val in pair]

                    wav_file.writeframes(struct.pack('<' + 'h' * len(interleaved), *interleaved))

        except KeyboardInterrupt:
            print("\nStopping...")
        except Exception as e:
            print(f"Error: {e}")
        finally:
            sock.close()


if __name__ == "__main__":
    main()