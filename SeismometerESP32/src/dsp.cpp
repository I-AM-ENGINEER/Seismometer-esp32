#include "DSP_Task.hpp"
#include "System.hpp"

void DSP_Task::Run(void) {
    I2S_to_DSP_package_t i2s_package;

    NetPackageHeader_t pkg_header;
    pkg_header.channel_id   = 0;
    pkg_header.frames_count = 1;
    pkg_header.bits_per_sample = 16;

    while (1) {
        SampleHeader_t frame;

        xQueueReceive(System::i2s_to_dsp_queue.getHandle(), &i2s_package, portMAX_DELAY);
        frame.timespamp = i2s_package.timestamp;
        frame.gain      = 9.0909f;
        pkg_header.samples_per_frame = (uint16_t)i2s_package.samples_count;

        const size_t data_size = (size_t)pkg_header.samples_per_frame * (size_t)i2s_package.bits_per_single_sample / 8;
        const size_t package_size =
                sizeof(NetPackageHeader_t) + 
                (sizeof(SampleHeader_t) + data_size) * (size_t)pkg_header.frames_count;

        uint8_t* package_data = (uint8_t*)pvPortMalloc(package_size);
        if (package_data == nullptr) {
            vPortFree(i2s_package.data);
            continue;
        }

        memcpy(package_data, &pkg_header, sizeof(NetPackageHeader_t));
        memcpy(package_data + sizeof(NetPackageHeader_t), &frame, sizeof(SampleHeader_t));
        memcpy(package_data + sizeof(NetPackageHeader_t) + sizeof(SampleHeader_t), i2s_package.data, data_size);
        vPortFree(i2s_package.data);
        
        NetQueueElement_t mqtt_package;
        mqtt_package.package_size = package_size;
        mqtt_package.data = package_data;

        xQueueSend(System::dsp_to_mqtt_queue.getHandle(), &mqtt_package, portMAX_DELAY);
    }
}
