#ifndef CONFIG_H__
#define CONFIG_H__

#define I2S_RAW_QUEUE_LENGTH                2
//#define UDP_PACKAGE_LENGTH                  3840
//#define UDP_PACKAGE_QUEUE_LENGTH            1


#define TCP_PACKAGE_LENGTH                  1460
#define TCP_PACKAGE_QUEUE_LENGTH            1
#define TCP_RECONNECT_DELAY_MS              100
#define TCP_MAX_RETRY_DELAY_MS              60000
// Valid: 96000, 64000, 48000, 32000, 16000, 8000
#define I2S_SAMPLERATE                      8000

// Multiply to 4*2 for get bytes size
#define I2S_DMA_FRAMES                      240
#define I2S_DMA_FRAME_SIZE                  8
#define I2S_DMA_FRAMES_BUFFER               4

#define IP_TARGET                           "192.168.1.10"
#define IP_TARGET_PORT                      1235

#define GPIO_BTN_PIN                        GPIO_NUM_6
#define GPIO_LED_PIN                        GPIO_NUM_5

#define WIFI_SSID                           "5G_COVID_BASE_STATION"
#define WIFI_PWD                            "1SimplePass9"






#define I2S_DMA_BUFFER_SIZE                 I2S_DMA_FRAMES * I2S_DMA_FRAME_SIZE

#endif // !CONFIG_H__
