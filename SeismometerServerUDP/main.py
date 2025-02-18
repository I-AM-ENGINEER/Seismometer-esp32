import os
import wave
import time
import struct
from datetime import datetime
import asyncio
import queue
import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import numpy as np
from sklearn.linear_model import RANSACRegressor, TheilSenRegressor, HuberRegressor
from TimestampFilter import TimestampFilter


# Define a queue to hold MQTT message payloads
mqtt_queue = queue.Queue()

def parse_net_package_header(data):
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
    sample_header_format = "<Qf"
    sample_header_size = struct.calcsize(sample_header_format)
    timestamp, gain = struct.unpack(sample_header_format, data[:sample_header_size])

    # Оставшиеся данные интерпретируем как массив int16_t
    sample_format = "<" + str((len(data) - sample_header_size) // 2) + "h"
    sample_data = struct.unpack(sample_format, data[sample_header_size:])
    return {
        "timestamp": timestamp,
        "gain": gain,
        "sample_data": sample_data
    }

def process_packet(data):
    """Обрабатывает сетевой пакет и возвращает список фреймов"""
    header = parse_net_package_header(data)
    byte_sample = header["bits_per_sample"] // 8
    sample_count = header["samples_per_frame"]
    frame_size = sample_count * byte_sample + 12  # 12 байт — размер заголовка

    frames = [
        parse_sample_header(header["remaining_data"][i * frame_size: (i + 1) * frame_size])
        for i in range(header["frames_count"])
    ]
    return frames

filter = TimestampFilter(max_buffer_size=600)

sample_count = 0
previous_ts = 0


def process_frame(frame):
    global sample_count, previous_ts

    real_ts = frame["timestamp"]

    #previous_ts = filter.get_approx_ts_from_sample_number(sample_count)
    #expected_dts = expected_ts - filter.get_approx_ts_from_sample_number(sample_count)
    expected_dts = 5000000
    real_dts = real_ts - previous_ts

    previous_ts = real_ts

    #if(expected_dts == 0):
    #    expected_dts =
    #if(real_dts < (expected_dts * 1000)):
    #    samples_delta = round((abs(real_dts)/expected_dts))
    #else:
    #    samples_delta = 0
    #if(samples_delta > 1.5):
    #    print("Samples skip: ", (abs(real_dts)/expected_dts))


    sample_count += 1
    filter.add_to_ring_buffer(sample_count, real_ts)

    expected_ts = filter.get_approx_ts_from_sample_number(sample_count + 1)
    #print(real_dts, expected_ts - filter.get_approx_ts_from_sample_number(sample_count))
    print(real_ts - filter.get_approx_ts_from_sample_number(sample_count))
    # Update for next iteration
    #previous_ts = current_ts


async def consume_queue():
    while True:
        payload = await asyncio.to_thread(mqtt_queue.get)
        if payload is None:  # None will be used to exit the loop
            break

        frames = process_packet(payload)
        for frame in frames:
            process_frame(frame)

        #print(f"Received payload: {payload}")  # Process the payload as needed

async def recalculate_filter_periodically():
    while True:
        await asyncio.sleep(1)  # Wait for 1 second
        filter.recalc_ransac_offset_and_slope()

def on_message(client, userdata, message):
    payload = message.payload
    mqtt_queue.put(payload)
    #frames = process_packet(message.payload)
    #for frame in frames:
    #    process_frame(frame)
    #filter.recalc_ransac_offset_and_slope()
        #print(frame["timestamp"])




BROKER = "192.168.1.17"
TOPIC = "xui"

async def main():
    client = mqtt.Client()
    client.on_message = on_message
    client.username_pw_set("demo", "demo")
    client.connect(BROKER, 1883, 60)
    client.subscribe(TOPIC)

    # Start the MQTT loop in a separate thread (non-blocking)
    client.loop_start()

    # Start async tasks
    asyncio.create_task(recalculate_filter_periodically())
    asyncio.create_task(consume_queue())

    # Keep the event loop running
    await asyncio.Event().wait()

if __name__ == "__main__":
    asyncio.run(main())  # Run the asyncio event loop properly
