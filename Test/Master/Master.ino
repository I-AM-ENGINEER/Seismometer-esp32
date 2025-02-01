#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>
#include <stdlib.h>
#include <string.h>

#define UART_BAUD 115200
#define F_CPU 16000000UL
#define UBRR_VALUE 16  // Correct UBRR value for double-speed mode at 115200 baud with 16 MHz clock
#define BUFFER_SIZE 64


volatile char uartBuffer[BUFFER_SIZE];
volatile uint8_t uartIndex = 0;
bool commandReady = false;

typedef enum {
  CFGTABLE_LAMP_STATE,                  // 0: RW - состояние лампы (0 - выключена / 1 - включена)
  CFGTABLE_DRIVER_VERSION,              // 1: R  - версия драйвера
  CFGTABLE_DRIVER_RUNTIME,              // 2: R  - время работы драйвера
  CFGTABLE_DRIVER_HOURS,                // 3: R  - время наработки драйвера
  CFGTABLE_ERROR_CODE,                  // 4: R  - код ошибки
  CFGTABLE_IGNITION_COUNT,              // 5: R  - кол-во разжиганий лампы
  CFGTABLE_LAMP_RUNTIME,                // 6: RW - время работы лампы
  CFGTABLE_LAMP_OFF_TIME,               // 7: RW - время отключения лампы
  CFGTABLE_DRIVER_TEMPERATURE,          // 8: R  - температура драйвера
  CFGTABLE_DRIVER_TEMP_LOWER_LIMIT,     // 9: RW - нижний предел температуры драйвера
  CFGTABLE_DRIVER_TEMP_UPPER_LIMIT,     // 10: RW - верхний предел температуры драйвера
  CFGTABLE_AC_VOLTAGE,                  // 11: R  - напряжение переменного тока
  CFGTABLE_AC_VOLTAGE_LOWER_LIMIT,      // 12: RW - нижний предел напряжения переменного тока
  CFGTABLE_AC_VOLTAGE_UPPER_LIMIT,      // 13: RW - верхний предел напряжения переменного тока
  CFGTABLE_LAMP_CURRENT,                // 14: R  - ток лампы
  CFGTABLE_LAMP_CURRENT_LOWER_LIMIT,    // 15: RW - нижний предел тока лампы
  CFGTABLE_LAMP_CURRENT_UPPER_LIMIT,    // 16: RW - верхний предел тока лампы
  CFGTABLE_LAMP_VOLTAGE,                // 17: R  - напряжение лампы
  CFGTABLE_LAMP_VOLTAGE_LOWER_LIMIT,    // 18: RW - нижний предел напряжения лампы
  CFGTABLE_LAMP_VOLTAGE_UPPER_LIMIT,    // 19: RW - верхний предел напряжения лампы
  CFGTABLE_LAMP_IGNITION_TIME,          // 20: RW - время зажигания лампы
  CFGTABLE_LAMP_MAX_POWER,              // 21: RW - максимальная мощность зажигания лампы
  CFGTABLE_LAMP_NOMINAL_POWER,           // 22: RW - номинальная мощность лампы
  CFGTABLE_PARAM_COUNT
} LampParamIndex;

int writable_params[] = {
  CFGTABLE_LAMP_STATE,
  CFGTABLE_LAMP_RUNTIME,
  CFGTABLE_LAMP_OFF_TIME,
  CFGTABLE_DRIVER_TEMP_LOWER_LIMIT,
  CFGTABLE_DRIVER_TEMP_UPPER_LIMIT,
  CFGTABLE_AC_VOLTAGE_LOWER_LIMIT,
  CFGTABLE_AC_VOLTAGE_UPPER_LIMIT,
  CFGTABLE_LAMP_CURRENT_LOWER_LIMIT,
  CFGTABLE_LAMP_CURRENT_UPPER_LIMIT,
  CFGTABLE_LAMP_VOLTAGE_LOWER_LIMIT,
  CFGTABLE_LAMP_VOLTAGE_UPPER_LIMIT,
  CFGTABLE_LAMP_IGNITION_TIME,
  CFGTABLE_LAMP_MAX_POWER,
  CFGTABLE_LAMP_NOMINAL_POWER
};

int32_t lamp_parameters[CFGTABLE_PARAM_COUNT]; // Array of int32_t to hold all parameters
const int num_writable_params = sizeof(writable_params) / sizeof(writable_params[0]);

// Initialize UART
void uart_init() {
  UBRR0H = (UBRR_VALUE >> 8);
  UBRR0L = UBRR_VALUE;
  UCSR0A = (1 << U2X0);                          // Enable double speed mode
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); // Enable RX, TX, and RX interrupt
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);               // 8 data bits, 1 stop bit
}

// Transmit a single character
void uart_transmit(char c) {
  while (!(UCSR0A & (1 << UDRE0))); // Wait until buffer is empty
  UDR0 = c;
}

// Transmit a string
void uart_transmit_string(const char *str) {
  while (*str) {
    uart_transmit(*str++);
  }
}

// CRC8-CCITT function
uint8_t calculateCRC8(const char *data, int length) {
    uint8_t crc = 0x00;  // Initial value
    uint8_t polynomial = 0x07;  // Polynomial used for CRC-8-CCITT

    for (int i = 0; i < length; i++) {
        crc ^= (uint8_t)data[i];  // XOR byte into the least significant byte of crc
        for (int j = 0; j < 8; j++) {  // Process each bit
            if (crc & 0x80) {  // If the most significant bit is set
                crc = (crc << 1) ^ polynomial;  // Shift left and XOR with polynomial
            } else {
                crc <<= 1;  // Just shift left
            }
            crc &= 0xFF;  // Ensure crc is 8 bits
        }
    }

    return crc;  // Return the calculated CRC
}

void handle_read_command( char package[] ) {
  int paramNum = atoi(&package[1]);
  if (paramNum >= CFGTABLE_PARAM_COUNT) {
    return;
  }

  int32_t value = lamp_parameters[paramNum];

  char response[32];
  int len = snprintf(response, sizeof(response), "R%hhu:%ld:", paramNum, value);

  uint8_t crc = calculateCRC8(response, len);
  snprintf(response + len, sizeof(response) - len, "%02X\n", crc);

  uart_transmit_string(response);
  uart_transmit_string("\r\n"); // Start with a newline
}

void handle_write_command(char package[]) {
    char *paramStr = strchr(package, ':');
    if (!paramStr) {
        return;
    }
    
    int paramNum = atoi(&package[1]);
    int writeValue = atoi(paramStr + 1);

    char *crcStr = strrchr(paramStr, ':');
    if (!crcStr) {
        return; 
    }

    uint8_t receivedCRC = (uint8_t)strtol(crcStr + 1, NULL, 16);  // Convert hex string to uint8_t

    if (paramNum < 0 || paramNum >= sizeof(lamp_parameters) / sizeof(lamp_parameters[0])) {
        return;
    }

    size_t command_length = crcStr - package + 1;
    uint8_t calculatedCRC = calculateCRC8((uint8_t *)package, command_length);
    if (receivedCRC != calculatedCRC) {
        char response[32];
        snprintf(response, sizeof(response), "%02X\n", calculatedCRC);
        uart_transmit_string(response);
        uart_transmit_string("ERR: CRC mismatch\n");
        return;
    }

    lamp_parameters[paramNum] = writeValue;
    uart_transmit_string("ACK\n");
}

void processCommand( char package[] ) {
  if (package[0] == 'R') {
    handle_read_command(uartBuffer);
  } else if (package[0] == 'W') {
    handle_write_command(uartBuffer);
  }
}

void cfgtable_receive_byte_ISR( uint8_t rcv ){
  if (commandReady) {
    return;
  }
  
  if ((receivedChar == '\n') && (uartIndex > 0)) {
    uartBuffer[uartIndex] = '\0';
    commandReady = true;
    uartIndex = 0;
  } else if (uartIndex < BUFFER_SIZE - 1) {
    uartBuffer[uartIndex++] = receivedChar;
  } else {
    uartIndex = 0;
  }
}

ISR(USART_RX_vect) {
  char receivedChar = UDR0;
  cfgtable_receive_byte_ISR(receivedChar);
}

void setup() {
  uart_init();
}

void loop() {
  if (commandReady) {
    processCommand(uartBuffer);
    commandReady = false;
  }
  delay(1);
}
