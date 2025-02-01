/*
 * Zephyr project template 
 * Marcelo Aleksandravicius - Jul 31 2024
 *
 * 
*/
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/socket.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4_server.h>

#define UDP_SERVER_IP "192.168.1.16"  // Destination IP for UDP packets
#define UDP_SERVER_PORT 1234         // Destination port for UDP packets
#define WIFI_THREAD_PRIORITY 10
#define UDP_THREAD_PRIORITY 5

#define WIFI_THREAD_STACK_SIZE 1024
#define UDP_THREAD_STACK_SIZE 1024

//LOG_MODULE_REGISTER(wifi_connect);

// UDP socket
static int udp_sock = -1;
static struct sockaddr_in server_addr;

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                        \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)

/* STA Mode Configuration */
#define WIFI_SSID "5G_COVID_BASE_STATION"     /* Replace `SSID` with WiFi ssid. */
#define WIFI_PSK  "1SimplePass9" /* Replace `PASSWORD` with Router password. */

//static K_SEM_DEFINE(wifi_connected, 0, 1);
//static K_SEM_DEFINE(ipv4_address_obtained, 0, 1);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static struct net_mgmt_event_callback cb;

static struct wifi_connect_req_params ap_config;
static struct net_if *sta_iface;
static struct wifi_connect_req_params sta_config;

K_SEM_DEFINE(wifi_connected_sem, 0, 1);

LOG_MODULE_REGISTER(main);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(btn0), gpios);


static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
        struct wifi_status* status = (struct wifi_status*)cb->info;
        if (status->status == 0) {
            LOG_INF("Wi-Fi connected to %s successfully.", WIFI_SSID);
            k_sem_give(&wifi_connected_sem);
        } else {
            LOG_ERR("Wi-Fi connection failed (status: %d)", status->status);
            k_sem_reset(&wifi_connected_sem);
        }
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		LOG_INF("Disconnected from %s", WIFI_SSID);
        k_sem_reset(&wifi_connected_sem); 
		break;
	}
	default:
        LOG_ERR("Unknown event");
        k_sem_reset(&wifi_connected_sem);
		break;
	}
}

static int connect_to_wifi( k_timeout_t timeout ){
	if (!sta_iface) {
		LOG_INF("STA: interface no initialized");
		return -EIO;
	}
    
	sta_config.ssid = (const uint8_t *)WIFI_SSID;
	sta_config.ssid_length = strlen(WIFI_SSID);
	sta_config.psk = (const uint8_t *)WIFI_PSK;
	sta_config.psk_length = strlen(WIFI_PSK);
	sta_config.security = WIFI_SECURITY_TYPE_PSK;
	sta_config.channel = WIFI_CHANNEL_ANY;
	sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;
    sta_config.timeout = SYS_FOREVER_MS;
	LOG_INF("Connecting to SSID: %s\n", sta_config.ssid);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("Unable to initiate connection to (%s)", WIFI_SSID);
        return ret;
	}

    // Wait for connection event or timeout
    if (k_sem_take(&wifi_connected_sem, timeout) == 0) {
        LOG_INF("Wi-Fi connection established.");
        ret = 0; // Success
    } else {
        LOG_ERR("Wi-Fi connection timed out.");
        ret = -ETIMEDOUT;
    }

	return ret;
}

static int init_udp_socket(void){
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        LOG_ERR("Failed to create UDP socket: %d", errno);
        return -errno;
    }

    // Set up the destination address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(UDP_SERVER_IP);
    
    return 0;
}

void udp_sending_thread(void) {
    struct sockaddr_in server_addr;
    int udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (udp_socket < 0) {
        LOG_ERR("Failed to create UDP socket");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_SERVER_PORT);
    net_addr_pton(AF_INET, UDP_SERVER_IP, &server_addr.sin_addr);

    while (1) {
        // Wait for Wi-Fi connection
        //k_sem_take(&wifi_connected_sem, K_FOREVER);

        //LOG_INF("Starting to send UDP packets...");

        //while (k_sem_count_get(&wifi_connected_sem) > 0) { // Check if still connected
            int led_state = gpio_pin_get_dt(&btn);
            gpio_pin_set_dt(&led, led_state);
            char buffer[1472];
            int len = sizeof(buffer);
            if (sendto(udp_sock, buffer, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                LOG_ERR("Failed to send UDP packet");
                k_msleep(1000);
            } else {
                //LOG_INF("UDP packet sent: %d", led_state);
            }
       // }

        k_msleep(1);
        //LOG_WRN("Stopping UDP packet sending due to disconnection.");
    }

    close(udp_socket);
}

void wifi_connection_thread(void) {
    while (1) {
        LOG_INF("Attempting to connect to Wi-Fi...");
        int ret = connect_to_wifi(K_SECONDS(30));
        if (ret == 0) {
            LOG_INF("Wi-Fi connected successfully.");
            if (k_sem_take(&wifi_connected_sem, K_FOREVER) == 0) {
                LOG_INF("Wi-Fi connection was broken.");
            }
        } else {
            LOG_ERR("Wi-Fi connection failed. Error: %d", ret);
        }
        k_msleep(5000);
    }
}

static struct k_thread thread_a_data;
static struct k_thread thread_b_data;

K_THREAD_STACK_DEFINE(thread_a_stack_area, WIFI_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_b_stack_area, UDP_THREAD_STACK_SIZE);

int main(void) {
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&btn, GPIO_INPUT | GPIO_PULL_DOWN | GPIO_INT_EDGE_RISING);

    k_sem_init(&wifi_connected_sem, 0, 1);
    
    net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
	net_mgmt_add_event_callback(&cb);
    
    sta_iface = net_if_get_wifi_sta();
    
/*
    k_thread_create(
        &(struct k_thread){}, k_malloc(WIFI_THREAD_STACK_SIZE),
        WIFI_THREAD_STACK_SIZE, wifi_connection_thread, NULL, NULL, NULL,
        WIFI_THREAD_PRIORITY, 0, K_NO_WAIT);
*/
    //net_mgmt_init_event_callback(&cb, wifi_scan_result_handler, NET_EVENT_WIFI_SCAN_DONE);
    //net_mgmt_add_event_callback(&cb);


    LOG_ERR("Trying wifi");
    

    /*
    /// connect_to_wifi();
    connect_to_wifi(K_SECONDS(10));
    
    init_udp_socket();
    while (1) {
        //int led_state = gpio_pin_get_dt(&btn);
        //gpio_pin_set_dt(&led, led_state);
        send_udp_packet(0);
        k_msleep(1000);
    }
*/
    LOG_ERR("Thread wifi starting");
    k_thread_create(
        &thread_b_data, thread_b_stack_area,
        WIFI_THREAD_STACK_SIZE, wifi_connection_thread, NULL, NULL, NULL,
        WIFI_THREAD_PRIORITY, 0, K_FOREVER);


    LOG_ERR("Thread udp starting");
    k_thread_create(
        &thread_a_data, thread_a_stack_area,
        UDP_THREAD_STACK_SIZE, udp_sending_thread, NULL, NULL, NULL,
        UDP_THREAD_PRIORITY, 0, K_FOREVER);
    //k_thread_name_set(&thread_a_data, "thread_a");
    k_thread_start(&thread_b_data);
    k_thread_start(&thread_a_data);
    LOG_ERR("Exiting main");
    return 0;
}