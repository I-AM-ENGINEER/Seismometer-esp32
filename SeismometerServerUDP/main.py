import os
import wave
import time
import struct
from datetime import datetime
import paho.mqtt.client as mqtt

audio_buffer = []

def save_audio(sample_rate=16000, channels=1, sample_width=2):
    """Сохраняет аудиопоток в WAV файл после накопления достаточного количества фреймов"""
    global audio_buffer
    if not audio_buffer:
        return

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    folder = "audio_captures"
    os.makedirs(folder, exist_ok=True)
    file_path = os.path.join(folder, f"audio_{timestamp}.wav")

    with wave.open(file_path, 'wb') as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(sample_width)
        wf.setframerate(sample_rate)
        wf.writeframes(struct.pack(f"<{len(audio_buffer)}h", *audio_buffer))

    print(f"Audio saved: {file_path}")
    audio_buffer = []  # Очищаем буфер после сохранения

def parse_net_package_header(data):
    """Парсит заголовок NetPackageHeader_t"""
    header_format = "<bB BH"
    header_size = struct.calcsize(header_format)

    channel_id, frames_count, bits_per_sample, samples_per_frame = struct.unpack(header_format, data[:header_size])
    return {
        "channel_id": channel_id,
        "frames_count": frames_count,
        "bits_per_sample": bits_per_sample,
        "samples_per_frame": samples_per_frame,
        "remaining_data": data[header_size:]  # Остаток данных для парсинга фреймов
    }

def parse_sample_header(data):
    """Парсит заголовок SampleHeader_t"""
    sample_header_format = "<Qf"
    sample_header_size = struct.calcsize(sample_header_format)
    timestamp, gain = struct.unpack(sample_header_format, data[:sample_header_size])
    sample_data = data[sample_header_size:]  # Бинарные данные после заголовка

    return {
        "timestamp": timestamp,
        "gain": gain,
        "sample_data": sample_data
    }

def parse_sample_data(data, len_, size):
    """Парсит заголовок SampleHeader_t"""
    data_s = []
    sample_header_format = "<h"
    for num in range(0, len_, size):
        data_s.append(struct.unpack(sample_header_format, data[num:num+size])[0])
    return data_s


def on_message(client, userdata, message):
    """Обработчик сообщений MQTT"""
    global audio_buffer
    data = message.payload  # Получаем бинарные данные
    header = parse_net_package_header(data)
    byte_sample = header["bits_per_sample"] // 8
    sample_count = header["samples_per_frame"]

    for num in range(header["frames_count"]):
        frames_data = header["remaining_data"][num * sample_count * byte_sample:(num + 1) * sample_count * byte_sample]
        frame = parse_sample_header(frames_data)
        frame["sample_data"] = parse_sample_data(frame["sample_data"], sample_count, byte_sample)
        audio_buffer.extend(frame["sample_data"])

    print("Captured frames count:", len(audio_buffer))

    # Проверяем, достаточно ли данных для одной секунды записи
    if len(audio_buffer) >= 16000:
        save_audio()


BROKER = "192.168.1.17"
TOPIC = "xui"

def main():
    client = mqtt.Client()
    client.on_message = on_message
    client.username_pw_set("demo", "demo")
    client.connect(BROKER, 1883, 60)
    client.subscribe(TOPIC)
    client.loop_forever()


if __name__ == "__main__":
    main()
