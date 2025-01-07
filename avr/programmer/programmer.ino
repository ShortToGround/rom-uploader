
#define BAUD 9600
#include <util/setbaud.h> // for Serial operations
#include "include/programmer.h"

/* 

The current programmer circuit I'm using is heavily based on Ben Eater's circuit
https://github.com/beneater/eeprom-programmer
All of his connections are the same in my circuit, I'm just using the analog pins for CPU control, ROM isolation, and external RAM control

I'm slowly re-writing this in pure C as time goes on and will eventually only have my code left I hope.

TODO: Add more robust error detecting during handshake and data transfer, has a possibility of hanging if something goes wrong during the handshake
Luckily, arduinos reset when you connect to them via their serial interface, so you can just re-send the data to try again

*/

int userInput;
int skipErase;
uint16_t address = 0;

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


// Read a byte from memory IC at the given address
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


// Read a byte to memory IC at the given address
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


// Read the contents of the memory IC and print to serial
void printContents(uint16_t rom_size){
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

void send_uint16(uint16_t data){
    uint8_t msb, lsb;

    msb = data >> 8;
    lsb = data & 0xFF;

    send_byte(msb);
    send_byte(lsb);
}

void z80Reset(void){
  digitalWrite(A5, LOW);
  delay(1);
  digitalWrite(A5, HIGH);
}


void eraseMemoryIC(uint8_t chiptype){
    // Uses global variable "address"
    uint16_t i, eraseBytes;
    // First I'd like to round the address up to the nearest 16 byte range so that the erase cycle will cover enough bytes
    for (i = address; i < MAX_ROM_SIZE; ++i){
        if ((i % 16) == 0){
            break;
        }
    }
    eraseBytes = i;
    // Erase entire memory IC
    for (i = 0; i <= eraseBytes; i += 1){
        writeEEPROM(i, 0xFF, chiptype);
        send_byte('.');
    }

    // ACK to signal to the computer app that we are done writing and ready for the next operation
    send_byte(1);
}



void clearScreen(void){
    send_byte(27);       // ESC command
    Serial.print("[2J");    // clear screen command
    send_byte(27);       // ESC command
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


void runMode(void){
    digitalWrite(SHIFT_REG_OE, HIGH);
    for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
        pinMode(pin, INPUT);
    }

    digitalWrite(WRITE_EN, HIGH);
    pinMode(WRITE_EN, INPUT);
    digitalWrite(EEPROM_CS, HIGH); // A2
    pinMode(EEPROM_CS, INPUT);
    
    digitalWrite(CPU_BUS_REQ, HIGH);
    z80Reset();
    pinMode(CPU_BUS_REQ, INPUT);
    pinMode(CPU_RESET, INPUT);

}

void programMode(void){
    pinMode(CPU_BUS_REQ, OUTPUT);
    digitalWrite(CPU_BUS_REQ, LOW);

    pinMode(WRITE_EN, OUTPUT);
    digitalWrite(WRITE_EN, HIGH);

    pinMode(EEPROM_CS, OUTPUT);
    digitalWrite(EEPROM_CS, LOW);

    digitalWrite(SHIFT_REG_OE, LOW);

    pinMode(CPU_RESET, OUTPUT);
    digitalWrite(CPU_RESET, HIGH);
}

void setup(void) {
    
    pinMode(SHIFT_REG_OE, OUTPUT);// This pin should also have a pullup resistor on it to keep it from going low until we want it to.
    digitalWrite(SHIFT_REG_OE, HIGH);
    pinMode(SHIFT_LATCH, OUTPUT); 
    pinMode(SHIFT_DATA, OUTPUT);
    pinMode(SHIFT_CLK, OUTPUT);
    
    // By default all CPU bus pins should be set to INPUT on boot
    // but we are going to make super sure they are
    // We should not set any outputs until the device is in program mode
    pinMode(WRITE_EN, INPUT);
    pinMode(EEPROM_CS, INPUT);
    pinMode(CPU_BUS_REQ, INPUT);
    pinMode(CPU_RESET, INPUT);
    // Set all data signals to INPUT/Float
    for (int pin = EEPROM_D0; pin <= EEPROM_D7; ++pin) {
        pinMode(pin, INPUT);
    }
    initSerial();
    runMode();
}

void loop(void) {
    uint8_t i, c, length, inverse_length, rom_size_msb, rom_size_lsb, crc8_1, crc8_2;
    uint16_t rom_size, crc16_1 = 0, crc16_2 = 0;
    uint8_t sbuf[SER_BUFF_SIZE];

    // # - Upload ROM
    // ? - Read ROM
    while(1){
        //c = Serial.read();
        c = get_byte();
        if (c == '#'){ 
            // Now we know the programmer wants to send data, let's send an ACK back first
            send_byte(ACK);
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
                send_uint16(rom_size); // ACK size of ROM for success
            }
            else{
                // if the ROM is too big we will tell the programmer and then break
                send_uint16(0);
                break;
            }


            // Now that we have the write command, and know that the ROM is small enough to be written, let's do the dang thing
            while (1){
                // the next byte should be our data chunk length
                length = get_byte();
                inverse_length = get_byte();
                inverse_length = ~inverse_length;
                if (length != inverse_length){
                    // Data was corrupted, we need to ask for it again
                    send_byte(NAK);
                }
                //send_byte(c);
                if (length == 0){
                    // We have uploaded the entire ROM

                    printContents(rom_size);

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
                    send_byte(ACK); // Success ACK
                    crc16_2 = 0; // set the calculated checksum to zero for the next chunk
                    // if they match then we will save the data to the memory IC

                    for (i = 0; i < length; ++i){
                        writeEEPROM(address, sbuf[i], RAM);
                        ++address;
                    }

                    // Now we will send our ACK saying we are done writing to the memory IC and want more data
                    send_byte(ACK);
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
            printContents(rom_size);
            runMode();
            break;
        }
    }
}
