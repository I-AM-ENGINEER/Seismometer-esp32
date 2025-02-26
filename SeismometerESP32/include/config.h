#ifndef CONFIG_H__
#define CONFIG_H__

#define I2S_RAW_QUEUE_LENGTH                2
//#define UDP_PACKAGE_LENGTH                  3840
//#define UDP_PACKAGE_QUEUE_LENGTH            1

#define WIFI_TSF_SAMPLE_PERIOD_MS           (5000)
#define WIFI_TSF_SAMPLERATE                 (50)
#define TCP_PACKAGE_LENGTH                  1460
#define TCP_PACKAGE_QUEUE_LENGTH            1
#define TCP_RECONNECT_DELAY_MS              100
#define TCP_MAX_RETRY_DELAY_MS              60000
// Valid: 96000, 64000, 48000, 32000, 16000, 8000
#define I2S_SAMPLERATE                      32000

// Multiply to 4*2 for get bytes size
#define I2S_DMA_FRAMES                      120
#define I2S_DMA_FRAME_SIZE                  8
#define I2S_SAMPLES_MONO_FRAME              (I2S_DMA_FRAMES * I2S_DMA_FRAME_SIZE / 2 / 3)
#define I2S_SAMPLES_STEREO_FRAME            (I2S_SAMPLES_MONO_FRAME * 2)
#define I2S_DMA_FRAMES_BUFFER               4

#define IP_TARGET                           "192.168.1.17"
#define IP_TARGET_PORT                      1883
#define MQTT_USERNAME                       "demo"
#define MQTT_PASSWORD                       "demo"

#define GPIO_BTN_PIN                        GPIO_NUM_6
#define GPIO_LED_PIN                        GPIO_NUM_5

#define WIFI_SSID                           "5G_COVID_BASE_STATION"
#define WIFI_PWD                            "1SimplePass9"

#define GPIO_GAIN_1_PIN                     GPIO_NUM_3
#define GPIO_GAIN_10_PIN                    GPIO_NUM_2
#define GPIO_GAIN_100_PIN                   GPIO_NUM_1
#define GPIO_GAIN_1000_PIN                  GPIO_NUM_0

#define TFS_SYNC_TIME_PERIOD_S      (5)
#define TFS_SYNC_TFS_PERIOD_MS      (100)
#define TFS_SYNC_ARRAY_COUNT        ((1000/TFS_SYNC_TFS_PERIOD_MS)*TFS_SYNC_TIME_PERIOD_S)
#define TFS_EXPECTED_GAIN           (40)




#define I2S_DMA_BUFFER_SIZE                 I2S_DMA_FRAMES * I2S_DMA_FRAME_SIZE

#endif // !CONFIG_H__
