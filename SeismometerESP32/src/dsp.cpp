#include "DSP_Task.hpp"
#include "System.hpp"


void DSP_Task::Run( void* arg ) {
    // How many sample frames to pack into one network package.
    constexpr uint8_t num_frames_per_package = 10; // Change as needed

    while (1) {
        // Initialize network package header.
        NetPackageHeader_t pkg_header;
        pkg_header.channel_id      = 0;
        pkg_header.frames_count    = num_frames_per_package;
        pkg_header.bits_per_sample = 16;
        
        // This vector will hold each sample’s header and its associated data pointer.
        std::vector<std::pair<SampleHeader_t, uint8_t*>> samples{};
        bool headerInitialized = false;

        // Collect the desired number of sample frames.
        for (uint8_t i = 0; i < num_frames_per_package; ++i) {
            I2S_to_DSP_package_t i2s_package;
            // Wait indefinitely for a package to arrive.
            xQueueReceive(System::i2s_to_dsp_queue.getHandle(), &i2s_package, portMAX_DELAY);

            // For the first sample, set the samples_per_frame field in the network header.
            if (!headerInitialized) {
                pkg_header.samples_per_frame = static_cast<uint16_t>(i2s_package.samples_count);
                headerInitialized = true;
            }

            // Create the sample header.
            SampleHeader_t sample;
            sample.timespamp = i2s_package.timestamp;
            sample.gain      = 9.0909f;
            
            uint8_t* sample_data = i2s_package.data;
            // Store the header and data pointer together.
            samples.emplace_back(sample, sample_data);
        }

        // Pack all collected samples into one network package.
        uint8_t* package_data = PackNetPackage(pkg_header, samples);
        if (!package_data) {
            // In case of allocation failure, free all sample data and continue.
            for (auto& samplePair : samples) {
                vPortFree(samplePair.second);
            }
            continue;
        }

        // After packing, free the original sample data.
        for (auto& samplePair : samples) {
            vPortFree(samplePair.second);
        }

        // Calculate the total package size.
        size_t data_size = pkg_header.samples_per_frame * pkg_header.bits_per_sample / 8;
        size_t package_size = sizeof(NetPackageHeader_t) +
                              samples.size() * (sizeof(SampleHeader_t) + data_size);

        // Prepare the MQTT package.
        NetQueueElement_t mqtt_package;
        mqtt_package.package_size = package_size;
        mqtt_package.data = package_data;

        // Send the packed network package to the DSP->MQTT queue.
        if(xQueueSend(System::dsp_to_mqtt_queue.getHandle(), &mqtt_package, 0) != pdTRUE){
            vPortFree(mqtt_package.data);
        }
    }
}

uint8_t* DSP_Task::PackNetPackage(NetPackageHeader_t& net_header, const std::vector<std::pair<SampleHeader_t, uint8_t*>>& samples) {
    size_t total_size = sizeof(NetPackageHeader_t);
    for (const auto& sample : samples) {
        total_size += sizeof(sample.first);  // SampleHeader_t
        total_size += net_header.samples_per_frame * net_header.frames_count * net_header.bits_per_sample / 8;  // Sample data
    }

    uint8_t* net_package = new uint8_t[total_size];
    uint8_t* buffer_ptr = net_package;

    if(net_package == nullptr){
        ESP_LOGE(TAG, "Allocation failed");
        return nullptr;
    }

    std::memcpy(buffer_ptr, &net_header, sizeof(net_header));
    buffer_ptr += sizeof(net_header);

    for (const auto& sample : samples) {
        std::memcpy(buffer_ptr, &sample.first, sizeof(sample.first));
        buffer_ptr += sizeof(sample.first);
        size_t sample_data_size = net_header.samples_per_frame * net_header.bits_per_sample / 8;
        std::memcpy(buffer_ptr, sample.second, sample_data_size);
        buffer_ptr += sample_data_size;
    }
    return static_cast<uint8_t*>(net_package);
}


