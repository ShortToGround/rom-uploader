#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include "../include/serial.h"
#include "../include/uploader.h"

// TODO: adjust protocol to fit more standard implementations (like using proper ack bytes, SOT and EOT bytes, and etc)
// Also get rid of the hard coded baud rates and buffer sizes.

// TODO: Re-think the timeout system so that slower hardware can have time to calculate CRC and write memory without stopping the transfer

#define DEBUG
// Global variables //
serial_com COM;
char *file_path = NULL;
char *port = NULL;
char *target = NULL;
//uint8_t printROM = 0;
uint16_t printROMSize = 0;

// Default values before config
int baudrate = DEFAULT_BAUD_RATE;
uint8_t bufferSize = DEFAULT_BUFFER_SIZE;
uint8_t dataMax = DEFAULT_BUFFER_SIZE - DATA_HEADER_SIZE - DATA_FOOTER_SIZE;


// Cross-platform Sleep function - Thanks Bernardo Ramos on stack overflow!
void sleep_ms(int milliseconds){
    #ifdef _WIN32
        Sleep(milliseconds);
    #elif _POSIX_C_SOURCE >= 199309L
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;
        nanosleep(&ts, NULL);
    #else
        if (milliseconds >= 1000){
            sleep(milliseconds / 1000);
            usleep((milliseconds % 1000) * 1000);
        }
    #endif
}


// converts uint16_t object into 2 uint8_t objects
void u16tou8(uint8_t buf[], uint16_t n){
    // to get most significant byte
    // bit shift n to the right 8 times
    // we will store this into the first element of the array as the most significant byte
    buf[0] = n >> 8;

    // now we AND n and 0xFF to clear the most significant byte and store it into the array
    buf[1] = n & 0xFF;
}

// generates a 16-bit crc checksum, iterated by an uint8_t input per call
uint16_t crc16_update(uint16_t crc, uint8_t n){
    uint8_t i;
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


// Serial related functions

/*
    Sends data out to the serial port. Must supply the output buffer, the number of bytes you are sending, and the data packet flag.
    The data packet flag will control whether or not we are sending the length byte header and crc footer.
    If the function is called with DATA_PACKET_FLAG_OFF then only the bytes in the output buffer will be sent, otherwise it will add the footer and header before sending
*/
int sendData(uint8_t s[], uint8_t num_bytes, uint8_t data_packet_flag){
    //TODO: maybe pull the crc code blocks out into a function since they are copy and pasted for windows and linuxifdefs
    int output, i, j;
    uint8_t *sbuf;
    sbuf = (uint8_t *) malloc(bufferSize * sizeof(uint8_t));



    uint8_t *outputBuf; // pointer to the output buffer, this is modified based on the data_packet_flag arg
    // if the DATA_PACKET_FLAG_OFF arg is used then we don't need an extra buffer and we can just send the bytes from the supplied s array  
    
    // if a data packet flag is set then we need to pad the data with the length byte and the crc bytes
        if (data_packet_flag){
            // this is where the data is buffered before sending
            // the length byte is stored first, then the data, then the crc bytes at the end
            
            outputBuf = sbuf;
            // First byte in the data chunk will be our data byte count
            sbuf[0] = num_bytes;

            // Populate the actual chunk of data
            for (i = 1, j = 0; j < num_bytes; ++i, ++j){
                sbuf[i] = s[j];
            }

            // next we calculate the crc hash
            uint16_t crc = 0;
            uint8_t crc_most_sig, crc_least_sig;

            for (i = 0; i < num_bytes; ++i){
                crc = crc16_update(crc, s[i]);
            }
            // to get most significant byte
            // bit shift n to the right 8 times
            crc_most_sig = crc >> 8;

            // To get the least sig byte we & crc and 0xFF to clear the most significant byte
            crc_least_sig = crc & 0xFF;

            // Add the crc bytes to the end of the data chunk
            sbuf[++i] = crc_most_sig;
            sbuf[++i] = crc_least_sig;

            // Because we've added a len byte and 2 crc bytes we need to make sure to change num_bytes to match
            // num_bytes is used to tell the WriteFile how many bytes to send
            num_bytes += 3;
    }
    else{
        // set the outputBuf pointer to s
        outputBuf = s;
    }

    #ifdef _WIN32
        // Windows WriteFile() doesn't return the bytes sent directly, it will set this variable to the total instead, though it is unused for now
        unsigned long bytesSent = 0;
        output = WriteFile(COM,         // Handle to the Serialport
                        outputBuf,           // Data to be written to the port
                        num_bytes,      // No of bytes to write into the port
                        &bytesSent,     // No of bytes written to the port
                        NULL);
        if (output == 0){
            return 0;
        }
        return 1;
    #endif

    #ifdef __linux__
        output = write(COM, outputBuf, num_bytes);

        if (output == -1){
            return 0;
        }
        return 1;
    #endif

    return 0;
}

/*  
    Grabs data from COM buffer and places it into a static buffer
    if numOfRecvBytes is set to 0 then it will read the same amount as BUFFER_SIZE
*/
unsigned long recvData(uint8_t s[], unsigned int numOfRecvBytes){
    unsigned long recv;

    if (numOfRecvBytes == 0){
        numOfRecvBytes = bufferSize - 1;
    }

    #ifdef _WIN32
        recv = SetCommMask(COM, EV_RXCHAR);
        DWORD inBytes;

        if (recv == 0){
            printf("recBytes: Could not set CommMask. Aborting...");
            exit(1);
        }

        // - 1 on byte size to read to account for the null-terminator slapped on the end of the s buffer for reading later
        recv = ReadFile(COM, s, numOfRecvBytes, &inBytes, NULL);
        if (!recv || !inBytes){
            return 0;
        }

        return inBytes;
    #endif

    #ifdef __linux__
        recv = read(COM, s, numOfRecvBytes);
        if (!recv){
            return 0;
        }

        return recv;
    #endif

    return 0;
}



/*  
    Everytime this is called it will write DATA_MAX number of bytes into the buffer (or the last of the remaining bytes)
    it will return 0 when all bytes have been read
*/
int getBytesFromFile(uint8_t buf[], FILE *f){
    int c = 0;
    int i;
    for (i = 0; i < dataMax && c != EOF; ++i){
        c = fgetc(f);
        buf[i] = c;
        if (c == EOF){
            buf[i+1] = EOF;
            return i;
        }
    }

    return i;
}

// Clears the terminal screen
void clearScreen(void){
    #ifdef _WIN32
        system("cls");
    #endif
    
    #ifdef __linux__
        system("clear");
    #endif
}

// This prints the progress bar to the terminal based on how many bytes have been sent vs the total number of bytes
void printProgress(long file_size, long processed_data){
    // makes the cursor invisible
    printf("\33[?25l");

    if (processed_data != 0){
        double out = ((double) processed_data) / ((double) file_size / (double) PROGRESS_BAR_LEN);
        putchar(13);
        // prints the '#' at the appropriate spot
        for (int i = 0; i < (int)(out + 0.5); ++i){
            printf("\x1b[%dC#", 9 + i);
            putchar(13);
        }
        
       
        // updates the completion %
        printf("\x1b[%dC]", PROGRESS_BAR_LEN + 8); // Puts the 
        putchar(13);
        printf("\x1b[%dC%.1f%%", PROGRESS_BAR_LEN + 11, (((double) processed_data) / ((double) file_size)) * 100);
    }
}


void printHelp(){
    printf("Usage: comm [OPTIONS] [--file [INFILE]] [--port [PORTNAME]] [--read-contents [num_of_bytes]] --write\n");
    printf("Options\n");
    printf("\t-b, --baud\tOverrides default baud rate.\n");
    //TODO:
    printf("\t-c, --compare\tCompares bin/hex file with the current program on the machine.\n");
    //TODO:
    printf("\t-ch, --chiptype\tspecifiy if this is a ROM or RAM chip to adjust write timings\n");
    printf("\t-f, --file\tinput hex file to send to the programmer\n");
    printf("\t-h, --help\tprint this help information and then exit\n");
    printf("\t-l, --list\tlist all available serial ports on the system and then exits\n");
    printf("\t-p, --port\tselect the programmer's serial port\n");
    printf("\t-r, --read-contents\tprint the contents of the ROM. If number of bytes isn't supplied (or if 0 is entered) then it will dump the max rom size worth of bytes.\n");
    printf("\t-t, --target\tselect the target (arduino or z80)\n");
    printf("\t-w, --write\tWrites bin/hex file to the programmer. Requires a COM port, an input file.\n");
    // TODO: Add option to adjust timeout
    printf("\n");
}

/*
    Sees if a string of chars are all numbers, thus the entire string is equal to some number n if converted
*/
uint8_t isStrNum(char s[]){
    for (long unsigned int i = 0; i < strlen(s); ++i){
        if (isdigit(s[i]) == 0){
            return 0;
        }
    }
    return 1;
}

// Used to make sure we aren't accessing an arg outside of bounds of argv
// Used when an arg requires the next arg for config (for example, "-b 50")
// 0 if within bounds
int argBoundsCheck(int argc, int i){
    if (i < argc){
        return 0; // if within bounds we return 0
    }
    else{
        return 1;
    }
}

// loads the args into various variables/flags for use
int parseArgs(int argc, char *argv[], struct bitflags *flags){
    // I believe the limit for both windows and linux is 256 serial com ports at once
    int comList[256] = {0};
    int j = 0;
    for (int i = 1; i < argc; ++i){
        char *arg = argv[i];
        if ((strcmp("-f", arg) == 0) || ((strcmp("--file", arg) == 0))){
            file_path = argv[i + 1];
            flags->fileFlag = 1;
        }
        else if ((strcmp("-h", arg) == 0) || ((strcmp("--help", arg) == 0))){
            printHelp();
            exit(0);
        }
        else if ((strcmp("-b", arg) == 0) || ((strcmp("--baud", arg) == 0))){
            if(argBoundsCheck(argc, i)){
                printf("No baud rate specified. Using default rate.\n");
            }
            else{
                if (isStrNum(argv[i + 1])){
                    baudrate = atoi(argv[i + 1]);
                    ++i;
                    printf("baudrate set to %d\n", baudrate);
                }
                else{
                    printf("Invalid baud rate selected. Using default rate (%d).\n", BAUD_RATE);
                }
            }
        }
        else if ((strcmp("-l", arg) == 0) || ((strcmp("--list", arg) == 0))){
            listCOMPorts();
            while (comList[j] != 0){
                printf("COM%d is open\n", j++);
            }
            exit(0);
        }
        else if ((strcmp("-p", arg) == 0) || ((strcmp("--port", arg) == 0))){

            port = argv[i + 1];
            flags->portFlag = 1;
        }
        else if ((strcmp("-r", arg) == 0) || ((strcmp("--read-contents", arg) == 0))){
            if (flags->writeFlag){
                printf("ERROR: Selected read and write operations at the same time. Please do these separately.\n");
                exit(1);
            }
    
            if(argBoundsCheck(argc, i)){
                if (isStrNum(argv[i + 1])){
                    printROMSize = atoi(argv[i + 1]);
                    ++i;
                    printf("Reading %d bytes of ROM\n", printROMSize);
                }
                else{
                    printROMSize = 0;
                }
            }
            else{
                printROMSize = 0;
            }

            printf("Dumping ROM...\n");
            flags->printROM = 1;
        }
        else if ((strcmp("-t", arg) == 0) || ((strcmp("--target", arg) == 0))){
            target = argv[i + 1];
            if ((strcmp("z80", target) == 0)){
                if (flags->arduinoFlag){
                    printf("ERROR: Selected more than 1 target machine type.\n");
                    exit(1);
                }
                else{
                    flags->z80Flag = 1;
                    baudrate = 115200;
                    printf("z80 targeted, seeing baudrate to 115200\n");
                }
            }
        }
        else if ((strcmp("-w", arg) == 0) || ((strcmp("--write", arg) == 0))){
            if (flags->printROM){
                printf("ERROR: Selected read and write operations at the same time. Please do these separately.\n");
                exit(1);
            }
            flags->writeFlag = 1;
        }
    }
    
    return 0;

}

// Handles receiving the ROM dump from the device and formats the output to make it easier to read
// TODO: Maybe add a compare flag and save the ROM into a buffer for comparison later for my comparsion main() arg? 
unsigned long getROMFromMachine(uint8_t s[], uint8_t rombuf[]){
    unsigned long i, j = 0, k;
    //unsigned long byteCounter = 0;
    //uint8_t rombuf[MAX_ROM_SIZE] = {0};
    uint8_t timeout = 0;

    while (timeout < 3){
        while ((i = recvData(s, 0)) && (j < MAX_ROM_SIZE)){
            for (k = 0; k < i; ++k, ++j){
                rombuf[j] = s[k];
            }
        }
        sleep_ms(10);
        ++timeout;
    }
    return j;
}

void printRomData(uint8_t rombuf[], unsigned long byteCount){
    uint16_t i;

    printf("ROM Size:%lu\n", byteCount);
    printf("ROM Data:\n");
    for (i = 0; i <= byteCount - 1; ++i){
        printf("%02x ", rombuf[i]);
        if (((i + 1) > 0) && (((i + 1) % 16) == 0)){
            putchar('\n');
        }
        else if (((i + 1) > 0) && (((i + 1) % 8) == 0)){
            putchar(' ');
            putchar(' ');
        }
    } 
}

void clearInputBuf(serial_com COM){
    #ifdef _WIN32
        PurgeComm(COM, PURGE_RXCLEAR);
    #else
        tcflush(COM,TCIOFLUSH);
    #endif
}

int main(int argc, char *argv[]){
    
    // main() arg flags
    struct bitflags flags = {0}; // init all flags to 0

    parseArgs(argc, argv, &flags);
    
    uint8_t *s; 
    s = (uint8_t *) malloc(bufferSize * sizeof(uint8_t));
    memset(s, 0, sizeof(bufferSize * sizeof(uint8_t)));
    
    uint8_t *chunk;
    chunk = (uint8_t *) malloc(bufferSize * sizeof(uint8_t));

    uint8_t timeoutct = 0;
    unsigned long i = 1;
    long file_size, data_processed = 0;




    // if the port flag wasn't set then we will just set the port to some defined default values
    if (!flags.portFlag){
        printf("no port selected, defaulting to %s\n", defaultPort);
        port = defaultPort; // defined in #ifdefs
    }

    // if file_path and port aren't empty then let's try to upload some data!
    if (flags.fileFlag && flags.writeFlag){
        COM = openCOM(port, baudrate, 0);
        
        if (COM != NO_COM){

            if (flags.z80Flag){
                s[0] = '2';
                s[1] = 13; // Carriage return
                sendData(s, 2, DATA_PACKET_FLAG_OFF);
                clearInputBuf(COM);
            }
            // Then we are targeting an arduino
            else{ 
                // device is an arduino board and this gives it time to reboot
                sleep_ms(2000);
                // Arduinos also send some garbage data when they first boot so let's clear the COM input buffer
                clearInputBuf(COM);
            }


            FILE *fp;
            fp = fopen(file_path, "rb");
            if (fp == NULL){
                printf("Could not open file. Aborting...\n");
                exit(1);
            }
            // Let's see how many bytes are in this bin/hex file
            fseek(fp, 0L, SEEK_END);
            file_size = ftell(fp);
            // Now let's set the file pointer back to 0
            fseek(fp, 0L, SEEK_SET);
            //s[0] = file_size >> 8;
            //s[1] = file_size & 0xFF;



            // Now we send the write command
            s[0] = '#';
            sendData(s, 1, DATA_PACKET_FLAG_OFF);
            recvData(s, 1);
            if (s[0] == '#'){
                // Break the 16-bit file_size into 2 bytes and stick then in the s[] buffer
                u16tou8(s, file_size);
                // call sendData and tell it to only send the first 2 bytes (that contain our file_size) and only send this data without adding the header and footer
                sendData(s, 2, DATA_PACKET_FLAG_OFF);
                // The programmer will ACK back with either the rom_size it received or a 0 if the rom_size is too big
                recvData(s, 2);
                uint16_t tmp = 0;
                tmp = s[0] << 8;
                tmp = tmp | s[1];
                
                //let's setup the progress bar
                char progress_bar[PROGRESS_BAR_LEN] = {' '}; // the printProgress function will change its bar length depending on the size of this array

                
                // let's make sure the rom_size the device thinks is coming is correct
                if (file_size == tmp){

                    // print the empty 00.0% bar to the screen
                    // clearScreen();
                    // printf("Upload: [");
                    // printf("%s", progress_bar);
                    // putchar(']');
                    // printf(" 00.0%%");
                    
                    // Start gathering bytes from the ROM file and attempt to upload them to the device
                    while ((i = getBytesFromFile(chunk, fp))){
                        // TODO: Add heartbeat section here for slower devices... like my Z80 board
                        // seriously crude timeout control
                        while (timeoutct < 3){
                            sendData(chunk, i, DATA_PACKET_FLAG_ON);
                            recvData(s, 1);
                            // ACK of 1 means successful reception and to send the next chunk
                            if (s[0] == 1){
                                // Now we listen for the heartbeats to stop and recv another ack of 1 to say it is ready for more data
                                data_processed += i;
                                printProgress(file_size, data_processed);
                                timeoutct = 0;

                                // Serial heartbeat while the programmer writes to the memory IC
                                // possible place where the program could hang, need to implement a timeout here too
                                s[0] = '.'; // Lazy way to start the while loop
                                while (s[0] == '.'){
                                    recvData(s, 1);
                                }
                                if (s[0] == 1){
                                    break;
                                }
                                else{
                                    // If we get here then the programmer didn't ACK after the write so this means something really strange happened
                                }
                                break;
                            }
                            else{
                                printProgress(file_size, data_processed);
                                ++timeoutct;
                            }
                            if (timeoutct > 2){
                                printf("\nError sending chunk, tried 3 times without success. Aborting.\n");
                                // make the cursor visible again
                                printf("\33[?25h");
                                exit(1);
                            }
                        }
                    }
                    // Now that the ROM has been sent, let's send a 0 length 0 byte "chunk" to let the programmer know we are done
                    s[0] = 0;
                    // Sends a chunk with a length byte of 0, so the programmer will assume there is no more data to send.
                    sendData(s, 1, DATA_PACKET_FLAG_OFF);
                    putchar('\n');
                    
                    // Now we will get the output back from the chip
                    uint8_t *rombuf = (uint8_t *) malloc(MAX_ROM_SIZE * sizeof(uint8_t));
                    unsigned long numOfBytes = getROMFromMachine(s, rombuf);
                    printRomData(rombuf, numOfBytes);
                    free(rombuf);

                    // make the cursor visible again
                    printf("\33[?25h");
                    putchar('\n'); // so when the program exits the shell prompt starts on the next line
                }
                else{
                    printf("data is too big to write to ROM!\n");
                }
            }
            //printf("Didn't recieve write command ack. Aborting...(%c)\n", s[0]);
            fclose(fp);
            closeCOM(COM);
            return 0;
        }
        else{
            printf("Failed to open COM. Aborting...\n");
            exit(1);
        }
    }
    else if (flags.printROM){
        printf("PrintROM:%d\nport:%s\n", flags.printROM, port);
        COM = openCOM(port, baudrate, 0);
        if (COM != NO_COM){
            sleep_ms(2000);
            s[0] = '?';
            printf("sending print rom command\n");
            sendData(s, 1, DATA_PACKET_FLAG_OFF);
            printf("waiting for response\n");
            recvData(s, 1);
            if (s[0] == '?'){
                u16tou8(s, printROMSize);
                sendData(s, 2, DATA_PACKET_FLAG_OFF);
                uint8_t *rombuf = (uint8_t *) malloc(MAX_ROM_SIZE * sizeof(uint8_t));
                unsigned long numOfBytes = getROMFromMachine(s, rombuf);
                printRomData(rombuf, numOfBytes);
                free(rombuf);
            }

            printf("\ndone\n");
        }
        closeCOM(COM);
    }
    else if (flags.compareFlag && flags.fileFlag){
        // TODO: ADD THIS IN
        uint8_t *rombuf = (uint8_t *) malloc(MAX_ROM_SIZE * sizeof(uint8_t));
        free(rombuf);
    }
    else{
        #ifdef DEBUG
            printf("%X, %X, %X, %X, %X, %X, %X", flags.fileFlag, flags.portFlag, flags.writeFlag, flags.compareFlag, flags.z80Flag, flags.arduinoFlag, flags.printROM);
        #endif
        printf("Not enough arguments\n");
        exit(1);
    }
    return 0;
}
