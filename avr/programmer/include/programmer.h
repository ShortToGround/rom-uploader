// Shift Register Controls
#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12

#define WRITE_EN        A0
#define EEPROM_OE       A1
#define EEPROM_CS       A2
#define CPU_BUS_REQ     A3
#define CPU_BUS_ACK     A4
#define CPU_IO_REQ      A5
#define SHIFT_REG_OE    A6
#define CPU_RESET       A7


// Programmer Config
// TODO: Have the AVR chip send the buffer size it uses and make this dynamic on the PC end
#define MAX_ROM_SIZE 16383 // current max due to shift register hardware limit
#define SER_BUFF_SIZE 64
#define MAX_DATA_SIZE SER_BUFF_SIZE - 4

// Defines
#define FALSE 0
#define TRUE 1
#define RAM 0
#define ROM 1
#define RAM_WR_DELAY 1
#define ROM_WR_DELAY 10
#define INTERNAL_RAM 0
#define EXTERNAL_RAM 1

// Flow Control Chars
#define SOH 0x01
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define ETB 0x17
#define CAN 0x18