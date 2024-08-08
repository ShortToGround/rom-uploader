#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13
#define CPU_BUS_REQ A4
#define CPU_RESET A5

#define Z80_INTERRUPT A2 // This does NOT work on an Arduino Nano, A6 can only be an analog input

#define MAX_ROM_SIZE 32767 // current max due to shift register hardware limit
// TODO: Have the AVR chip send the buffer size it uses and make this dynamic on the PC end
#define SER_BUFF_SIZE 64
#define MAX_DATA_SIZE SER_BUFF_SIZE-3
#define FALSE 0
#define TRUE 1
#define RAM 0
#define ROM 1
#define RAM_WR_DELAY 1
#define ROM_WR_DELAY 10
#define INTERNAL_RAM 0
#define EXTERNAL_RAM 1