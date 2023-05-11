
#define BAUD 250000
#include <util/setbaud.h> // for Serial operations
#include <SPI.h>

#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13


#define MAX_ROM_SIZE 32767 // current max due to shift register hardware limit
#define MAX_INTERN_ROM_SIZE 1000
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

// the current programmer circuit I'm using is based on Ben Eater's circuit
// https://github.com/beneater/eeprom-programmer
// All of his connections are the same in my circuit, I'm just using the analog pins for CPU control, ROM isolation, and external RAM control

/*  This program is heavily based on Ben Eater's program in the link above 

    The code is a mess and I'm actively cleaning it up as I go. I'm also slowing porting it to plain C and plan to ditch the arduino IDE.
*/

int userInput;
int skipErase;
uint16_t address = 0;
// TODO: Add in external ram function
uint8_t internal_ram_buffer[MAX_INTERN_ROM_SIZE];
// Adjust write timing
int Device = 1; // 0 for ROM, 1 for RAM

/*
 * Output the address bits and outputEnable signal using shift registers.
 */
void setAddress(uint16_t addressVal, bool outputEnable) {
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (addressVal >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, addressVal);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}


/*
 * Read a byte from the EEPROM at the specified address.
 */
uint8_t readEEPROM(uint16_t addressVal){
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
    pinMode(pin, INPUT);
  }
  setAddress(addressVal, /*outputEnable*/ true);

  uint8_t data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; --pin) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}


/*
 * Write a byte to the EEPROM at the specified address.
 */
void writeEEPROM(int16_t addressVal, uint8_t data, uint8_t chiptype){
    uint8_t delay_time;
    if (chiptype == 0){
        delay_time = 1;
    }
    else{
        delay_time = 10;
    }
    setAddress(addressVal, /*outputEnable*/ false);
    for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin){
        pinMode(pin, OUTPUT);
    }

    for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin){
        digitalWrite(pin, data & 1);
        data = data >> 1;
    }
    digitalWrite(WRITE_EN, LOW);
    delayMicroseconds(1);
    digitalWrite(WRITE_EN, HIGH);
    delay(delay_time);
}


uint8_t timeoutCounter(uint8_t n){
    if (n == 3){
        return 0;
    }
    else{
        return n+1;
    }
}

/*
 * Read the contents of the EEPROM and print them to the serial monitor.
 */
void printContents(uint8_t memory_flag, uint16_t rom_size){
    if (memory_flag == INTERNAL_RAM){
        //char buf[80];
        uint8_t i;

        for (uint16_t base = 0; base <= rom_size-1; base += 16){
            uint8_t data[16] = {0};

            for (uint8_t offset = 0; offset <= 15; offset += 1){
                data[offset] = readEEPROM(base + offset);
            }

            for (i = 0; i < 16; ++i){
                send_byte(data[i]);
            }
        }
    }
}

void send_uint16(uint16_t data){
    uint8_t msb, lsb;

    msb = data >> 8;
    lsb = data & 0xFF;

    send_byte(msb);
    send_byte(lsb);
}

void runMode(void){
    pinMode(13, INPUT);
    // digitalWrite(A0, LOW);
    // digitalWrite(A1, HIGH);
    // digitalWrite(A2, HIGH);
    // digitalWrite(A3, LOW);
    // digitalWrite(A4, HIGH);
    // digitalWrite(A5, HIGH);

    // does the same as above but faster and with less memory
    PORTC |= 0b00110110;
    
    z80Reset();
}

void programMode(void){
    pinMode(13, OUTPUT);
    // digitalWrite(A0, HIGH);
    // digitalWrite(A1, LOW);
    // digitalWrite(A2, LOW);
    // digitalWrite(A3, HIGH);
    // digitalWrite(A4, LOW);
    // digitalWrite(A5, LOW);

    // does the same as above but faster and with less memory
    PORTC &= ~(0b00110110);

    //Serial.println("Board set to program mode.");
}

void z80Reset(void){
  digitalWrite(A5, LOW);
  delay(1);
  digitalWrite(A5, HIGH);
}

void programMachine(uint8_t memory_flag, uint8_t chiptype, uint8_t skipErase){

    if (memory_flag == INTERNAL_RAM){
        // Uses global variable "address"
        uint16_t i;
        // First I'd like to round the address up to the nearest 16 byte range so that the erase cycle will cover enough bytes
        uint16_t eraseBytes;
        for (i = address; i < MAX_ROM_SIZE; ++i){
            if ((i % 16) == 0){
                break;
            }
        }
        eraseBytes = i;

        //Serial.println("Programming Z80.");
        if (!skipErase){
            // Erase entire EEPROM
            //Serial.print("Erasing EEPROM");
            for (i = 0; i <= eraseBytes; i += 1) {
                writeEEPROM(i, 0xff, chiptype);
            }
        }

        // Program data bytes
        //Serial.print("Programming EEPROM");
        for (i = 0; i <= address; ++i){
            writeEEPROM(i, internal_ram_buffer[i], chiptype);
        }
    }
}



void clearScreen(void){
    send_byte(27);          // ESC command
    Serial.print("[2J");    // clear screen command
    send_byte(27);          // ESC command
    Serial.print("[H");     // cursor to home command
}

uint8_t get_byte(void){

    loop_until_bit_is_set(UCSR0A, RXC0);
    return UDR0;
}

// will send a raw byte through serial
void send_byte(uint8_t n){

    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = n;
}

// sends a string, treated as null-terminated ASCII
void send_string(char s[]){
    for (uint16_t i = 0; s[i] != '\0'; ++i){
        send_byte(s[i]);
    }
}

uint16_t u8tou16(uint8_t msb, uint8_t lsb){
    uint16_t num = 0;
    // first we shift in the most significant byte
    num = msb << 8;
    // then we can simply add the least significant byte
    num = num | lsb;

    return num;
}

uint16_t crc16_update(uint16_t crc, uint8_t n){
    int i;
    crc ^= n;
    for (i = 0; i < 8; ++i){
        if (crc & 1){
            crc = (crc >> 1) ^ 0xA001;
        }
        else{
            crc = (crc >> 1);
        }
    }
    return crc;
}

void writeBytesToIntMemory(uint8_t sbuf[], uint8_t length){
    // uses global variables "address", "internal_ram_buffer", and "external_ram_buffer"
    int i = 0;
    for (i = 0; i <= length; ++i){
        internal_ram_buffer[address] = sbuf[i];
        ++address;
    }
}

void writeBytesToExtMemory(uint8_t sbuf[], uint8_t length){
    // uses global variables "address", "internal_ram_buffer", and "external_ram_buffer"
    int i = 0;
    for (i = 0; i <= length; ++i){
        internal_ram_buffer[address] = sbuf[i];
        ++address;
    }
}

void initSerial(void){
    UBRR0L = UBRRL_VALUE;
    UBRR0H = UBRRH_VALUE;


#if USE_2X
    UCSR0A |= _BV(U2X0);
#else
    UCSR0A &= ~(_BV(U2X0));
#endif

    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // 8-bit data
    UCSR0B = _BV(RXEN0)  | _BV(TXEN0);  // Enable RX and TX
}


void setup(void) {
    pinMode(SHIFT_DATA, OUTPUT);
    pinMode(SHIFT_CLK, OUTPUT);
    pinMode(SHIFT_LATCH, OUTPUT);

    // TODO: See if I need this here with my current setup
    digitalWrite(WRITE_EN, HIGH);

    pinMode(WRITE_EN, OUTPUT);


    //Serial.begin(BAUD);
    initSerial();

    pinMode(A0, OUTPUT);
    pinMode(A1, OUTPUT);
    pinMode(A2, OUTPUT);
    pinMode(A3, OUTPUT);
    pinMode(A4, OUTPUT);
    pinMode(A5, OUTPUT);

    // does the same as the pinMode lines above but faster and uses less memory
    //DDRC = DDRC | 0b00111111;

    runMode();
}

void loop(void) {
    uint8_t i, c, length, rom_size_msb, rom_size_lsb, crc8_1, crc8_2;
    uint16_t rom_size, crc16_1 = 0, crc16_2 = 0;
    uint8_t sbuf[SER_BUFF_SIZE];
    //uint8_t internal_mem_flag = 0;
    // TODO: linked to TODO about external ram buffer
    void (*writeBytesToMemory)(uint8_t *, uint8_t);
    writeBytesToMemory = &writeBytesToIntMemory;
    // # - Upload ROM
    // ? - Read ROM
    while(1){
        //c = Serial.read();
        c = get_byte();
        if (c == '#'){ 
            // Now we know the programmer wants to send data, let's send an ACK back first
            send_byte('#');
            //send_byte('#'); // Our ACK, 1 for success
            
            // isolates the ROM/RAM chip so it can be programmed without interference from the CPU
            programMode();

            // The next 2 bytes will be the size of our ROM so know if we can process it or not
            // most significant byte will come first
            rom_size_msb = get_byte();
            // then our least significant byte
            rom_size_lsb = get_byte();

            // Now we need to convert these back into a true uint16_t
            rom_size = u8tou16(rom_size_msb, rom_size_lsb);

            if ((rom_size > 0) && (rom_size <= MAX_ROM_SIZE)){
                send_uint16(rom_size); // ACK 1 for success
            }
            else{
                // if the ROM is too big we will tell the programmer and then break
                send_uint16(0);
                break;
            }

            // If the ROM is greater than the amount we can fit in internal RAM, then we will
            // use the external RAM instead
            // This will use a function pointer to swap between the functions that will handle internal or external memory routines
            if (rom_size > MAX_INTERN_ROM_SIZE){
                writeBytesToMemory = &writeBytesToExtMemory;
            }

            // Now that we have the write command, and know that the ROM is small enough to be written, let's do the dang thing

            while (1){
                // the next byte should be our data chunk length
                length = get_byte();
                //send_byte(c);
                if (length == 0){
                    // We have uploaded the entire ROM
                    
                    
                    if (rom_size > MAX_INTERN_ROM_SIZE){
                        programMachine(EXTERNAL_RAM, RAM, TRUE);
                        printContents(EXTERNAL_RAM, rom_size);
                    }
                    else{
                        programMachine(INTERNAL_RAM, RAM, TRUE);
                        printContents(INTERNAL_RAM, rom_size);
                    }
                    runMode();
                    break;
                }
                // Now we grab the next n bytes, where n is equal to the length we just received
                for (i = 0; i < length; ++i){
                    sbuf[i] = get_byte();
                }

                // now we grab the next 2 bytes which will be the crc hash from the PC
                crc8_1 = get_byte();
                crc8_2 = get_byte();
                crc16_1 = u8tou16(crc8_1, crc8_2);

                // then we calculate the CRC ourselves
                for (i = 0; i < length; ++i){
                    crc16_2 = crc16_update(crc16_2, sbuf[i]);
                }

                // Lastly, we see if they match
                if (crc16_1 == crc16_2){
                    send_byte(1); // Success ACK
                    crc16_2 = 0; // set the calculated checksum to zero for the next chunk
                    // if they match then we will save the data to RAM

                    // this calls the function pointer writeBytesToMemory which will be adjusted based on the ROM size
                    // if the ROM size is too big for internal RAM to hold then it will point to a function that handles writing to the SRAM via SPI
                    (*writeBytesToMemory)(sbuf, length);
                }
                else{
                    send_uint16(crc16_2); // Fail ACK
                    crc16_2 = 0; // set the calculated checksum to zero for the next chunk
                    // If it doesn't pass the checksum check then it won't save the chunk to memory and will loop back around
                    // it's up to the PC program to send the chunk again
                }

            }

        }
        else if (c == '?'){
            send_byte('?'); // ACK
            rom_size_msb = get_byte(); // How many bytes do we want to read?
            rom_size_lsb = get_byte();
            rom_size = u8tou16(rom_size_msb, rom_size_lsb);
            // rom_size arg of 0 will mean it will print from 0 to MAX_ROM_SIZE
            programMode();
            if (rom_size > MAX_INTERN_ROM_SIZE){
                printContents(EXTERNAL_RAM, rom_size);
            }
            else{
                printContents(INTERNAL_RAM, rom_size);
            }
            runMode();
            break;
        }
    }
}
