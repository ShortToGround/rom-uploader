#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
    #include <ws2tcpip.h>
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #define MAX_FILE_NAME_LEN MAX_PATH
    #define NO_COM NULL // When a COM fails to init then it returns NULL on the windows code
    char defaultPort[] = "COM4";

    // API typedefs to avoid clutter
    typedef HANDLE serial_com;

#else
    #include <fcntl.h>
    #include <errno.h>
    #include <termios.h> // doesn't allow for custom baud rates
    //#include <sys/ioctl.h> // Used for TCGETS2/TCSETS2, which is required for custom baud rates
    #include <unistd.h>
    #include <dirent.h>
    #include <string.h>
    #include <time.h> // for nanosleep()
    #define MAX_FILE_NAME_LEN PATH_MAX
    #define NO_COM 0 // When a COM fails to init then it returns 0 on the Linux code
    char defaultPort[] = "ttyUSB0";

    // API typedefs to avoid clutter
    typedef int serial_com;
#endif

#define BAUD_RATE 250000 // Win api wants unsigned long and linux terminos wants usigned int... but neither complain if I don't specify

// TODO: Have the programming device send the buffer size it uses and make this dynamic on the PC end
#define BUFFER_SIZE 64 + 1 // +1 for null terminator on output
#define DATA_MAX BUFFER_SIZE-3 // -3 accounts for the header and footer added to the data chunk when sending
#define MAX_ROM_SIZE 32000
#define DATA_PACKET_FLAG_ON 1
#define DATA_PACKET_FLAG_OFF 0
#define PROGRESS_BAR_LEN 50 // Length of the progress bar during upload

// Global variables //
char *file_path = NULL;
char *port = NULL;
uint8_t printROM = 0;
uint16_t printROMSize = 0;

// main() arg flags
uint8_t fileFlag = 0;
uint8_t portFlag = 0;
uint8_t writeFlag = 0;
uint8_t compareFlag = 0;


// Cross-platform Sleep function - Thanks Bernardo Ramos on sack overflow!
void sleep_ms(int milliseconds){
    #ifdef WIN32
        Sleep(milliseconds);
    #elif _POSIX_C_SOURCE >= 199309L
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;
        nanosleep(&ts, NULL);
    #else
        if (milliseconds >= 1000)
        sleep(milliseconds / 1000);
        usleep((milliseconds % 1000) * 1000);
    #endif
}


// Attempts to open a serial port. If the list_flag is set then it will brute force and see which ports are active and then print the list
serial_com openCOM(char comPort[], int baudrate, int list_flag){
    #ifdef _WIN32
        int com_status;
        char comstr[] = "\\\\.\\";
        char port[14];

        sprintf(port, "%s%s", comstr, comPort);
        HANDLE COM;
        COM = CreateFileA(port,                        //port name
                      GENERIC_READ | GENERIC_WRITE,    //Read/Write
                      0,                               // No Sharing
                      NULL,                            // No Security
                      OPEN_EXISTING,                   // Open existing port only
                      0,                               // Non Overlapped I/O
                      NULL);                           // Null for Comm Devices

        // if the list flag is set then we don't want to set the baud rate and all that
        if (list_flag){
            return COM;
        }

        DCB comOptions = {0};
        comOptions.DCBlength = sizeof(DCB);

        // get the current settings on the port
        com_status = GetCommState(COM, &comOptions);

        if (com_status == 0){
            return NULL;
        }

        comOptions.BaudRate = baudrate;
        comOptions.ByteSize = 8;
        comOptions.Parity = NOPARITY;
        comOptions.StopBits = ONESTOPBIT;

        com_status = SetCommState(COM, &comOptions);
        if (!com_status){
            return NULL;
        }

        // now we set out timeout options
        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;

        if (SetCommTimeouts(COM, &timeouts) == 0){
            return NULL;
        }

        return COM;
    #endif

    #ifdef __linux__
        char comstr[] = "/dev/";
        char port[14];

        // Awful way to get my compiler to stop complaining about the unused list_flag arg
        if (list_flag){
            ;
        }
        // seriously I'm so sorry lol, I'll fix this later... said every dev ever

        sprintf(port, "%s%s", comstr, comPort);
        int serial_port = open(port, O_RDWR);

        if (serial_port < 0){
            printf("Error %i from open: %s\n", errno, strerror(errno));
        }

        struct termios tty;

        if (tcgetattr(serial_port, &tty) != 0){
            printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        }
        tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity
        tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication

        tty.c_cflag &= ~CSIZE; // Clear all the size bits
        tty.c_cflag |= CS8; // 8 bits per byte

        tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control

        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

        tty.c_lflag &= ~ICANON; // Disable canonical mode

        tty.c_lflag &= ~ECHO; // Disable echo
        tty.c_lflag &= ~ECHOE; // Disable erasure
        tty.c_lflag &= ~ECHONL; // Disable new-line echo

        tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP aka signal chars

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off software flow control

        // This will enable raw data, which we need
        tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

        tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
        tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

        // Set the recv mode and timeouts
        tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
        tty.c_cc[VMIN] = 0;

        // Set baud rate
        // tty.c_ispeed = 250000;
        // tty.c_ospeed = 250000;
        cfsetispeed(&tty, baudrate);
        cfsetospeed(&tty, baudrate);

        // Write terminal settings to file descriptor
        if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
            printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
            return NO_COM;
        }
        return serial_port;
    #endif

}

void closeCOM(serial_com COM){
    #ifdef _WIN32
        CloseHandle(COM);
    #endif
    #ifdef __linux__
        close(COM);
    #endif
}

void listCOMPorts(void){
    //TODO: Add linux support.. just not sure how to tackle this yet
    #ifdef _WIN32
        char port[7];
        HANDLE COM;
        int i;
        for (i = 1; i <= 256; ++i){
            sprintf(port, "%s%d", "COM", i);
            COM = openCOM(port, 9600, 1);
            if (COM != INVALID_HANDLE_VALUE && COM != NO_COM){
                printf("COM%d is open\n", i);
            }
            else{
            }
            CloseHandle(COM);
        }
    #endif
    #ifdef __linux__
        printf("Sorry, the com list function doesn't work for *nix devices just yet.\n");
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

// generates a 16-bit crc checksum, iterated by an uint8_t input per loop
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
    The data packet flag will control wether or not we are sending the length byte header and crc footer.
    If the function is called with DATA_PACKET_FLAG_OFF then only the bytes in the output buffer will be sent, otherwise it will add the footer and header before sending
*/
int sendData(serial_com COM, uint8_t s[], uint8_t num_bytes, uint8_t data_packet_flag){
    //TODO: maybe pull the crc code blocks out into a function since they are copy and pasted for windows and linux ifdefs
    int output, i, j;

    uint8_t *outputBuf; // pointer to the output buffer, this is modified based on the data_packet_flag arg
    // if the DATA_PACKET_FLAG_OFF arg is used then we don't need an extra buffer and we can just send the bytes from the supplied s array  
    
    // if a data packet flag is set then we need to pad the data with the length byte and the crc bytes
        if (data_packet_flag){
            // this is where the data is buffered before sending
            // the length byte is stored first, then the data, then the crc bytes at the end
            uint8_t sbuf[BUFFER_SIZE];
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
    // if a data packet flag is set then we need to pad the data with the length byte and the crc bytes
        if (data_packet_flag){
            // this is where the data is buffered before sending
            // the length byte is stored first, then the data, then the crc bytes at the end
            uint8_t sbuf[BUFFER_SIZE];
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

        output = write(COM, outputBuf, num_bytes);

        if (output == -1){
            return 0;
        }
        return 1;
    #endif
}

/*  
    Grabs data from COM buffer and places it into a static buffer
    if numOfRecvBytes is set to 0 then it will read the same amount as BUFFER_SIZE
*/
unsigned long recvData(uint8_t s[BUFFER_SIZE], serial_com COM, unsigned int numOfRecvBytes){
    unsigned long recv, i;
    uint8_t sbuf[BUFFER_SIZE];

    // if (numOfRecvBytes == 0){
    //     numOfRecvBytes = (sizeof(sbuf) / sizeof(uint8_t)) - 1;
    // }

    // I think this will do the same as above without using cycles on math?
    if (numOfRecvBytes == 0){
        numOfRecvBytes = BUFFER_SIZE - 1;
    }

    #ifdef _WIN32
        recv = SetCommMask(COM, EV_RXCHAR);
        DWORD inBytes;

        if (recv == 0){
            printf("recBytes: Could not set CommMask. Aborting...");
            exit(1);
        }

        // - 1 on byte size to read to account for the null-terminator slapped on the end of the s buffer for reading later
        recv = ReadFile(COM, sbuf, numOfRecvBytes, &inBytes, NULL);
        if (!recv || !inBytes){
            return 0;
        }

        // I'm not sure why I'm not just passing s[] directly instead of doing this with sbuf? Gotta re-think this part I think
        for (i = 0; i < inBytes; ++i){
            s[i] = sbuf[i];
        }
        
        return inBytes;
    #endif

    #ifdef __linux__
        recv = read(COM, sbuf, numOfRecvBytes);
        if (!recv){
            return 0;
        }
        // I'm not sure why I'm not just passing s[] directly instead of doing this with sbuf? Gotta re-think this part I think
        for (i = 0; i < recv; ++i){
            s[i] = sbuf[i];
        }
        return recv;
    #endif
}



/*  
    Everytime this is called it will write DATA_MAX number of bytes into the buffer (or the last of the remaining bytes)
    it will return 0 when all bytes have been read
*/
int getBytesFromFile(uint8_t buf[DATA_MAX], FILE *f){
    int c = 0;
    int i;
    for (i = 0; i <= DATA_MAX && c != EOF; ++i){
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
void printProgress(long file_size, long processed_data, unsigned int divisor){
    // makes the cursor invisible
    printf("\33[?25l");

    if (processed_data != 0){
        double out = ((double) processed_data) / ((double) file_size / (double) divisor);
        putchar(13);
        // prints the '#' at the appropriate spot
        for (int i = 0; i < (int) out; ++i){
            //printf("writing to char %d\n", 9 + i);
            printf("\x1b[%dC#", 9 + i);
            putchar(13);
        }
        
       
        // updates the completion %
        printf("\x1b[%dC%.1f%%", divisor + 11, (((double) processed_data) / ((double) file_size)) * 100);
    }
}


void printHelp(){
    printf("Usage: comm [OPTIONS] [--file [INFILE]] [--port [PORTNAME]] [--read-contents [num_of_bytes]] --write\n");
    printf("Options\n");
    //TODO:
    printf("\t-c, --compare\tCompares bin/hex file with the current program on the machine.\n");
    //TODO:
    printf("\t-ch, --chiptype\tspecifiy if this is a ROM or RAM chip to adjust write timings\n");
    printf("\t-f, --file\tinput hex file to send to the programmer\n");
    printf("\t-h, --help\tprint this help information and then exit\n");
    printf("\t-l, --list\tlist all available serial ports on the system and then exits\n");
    printf("\t-p, --port\tselect the programmer's serial port\n");
    printf("\t-r, --read-contents\tprint the contents of the ROM. If number of bytes isn't supplied (or if 0 is entered) then it will dump the max rom size worth of bytes.\n");
    printf("\t-w, --write\tWrites bin/hex file to the programmer. Requires a COM port, an input file.\n");
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


// loads the args into various variables/flags for use
int parseArgs(int argc, char *argv[]){
    /* 
        TODO: There isn't really any checking going on to see if I'm accessing argv[] out of bounds. 
        for instance if I run the program "./uploader --port ttyUSB0 -r", when checking for a number arg after -r I will go out of bounds and do UB.. oops
    */

    // I believe the limit for both windows and linux is 256 serial com ports at once
    int comList[256] = {0};
    int j = 0;
    for (int i = 0; i < argc; ++i){
        char *arg = argv[i];
        if ((strcmp("-f", arg) == 0) || ((strcmp("--file", arg) == 0))){
            file_path = argv[i + 1];
            fileFlag = 1;
        }
        else if ((strcmp("-h", arg) == 0) || ((strcmp("--help", arg) == 0))){
            printHelp();
            exit(0);
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
            portFlag = 1;
        }
        else if ((strcmp("-r", arg) == 0) || ((strcmp("--read-contents", arg) == 0))){
            if (writeFlag){
                printf("ERROR: Selected read and write operations at the same time. Please do these separately.\n");
                exit(1);
            }
            // TODO: see why this doesn't work when -r is just used without specifying a number
            // Probably has something to do with the fact that I have no bounds checking on argv atm
            if (isStrNum(argv[i + 1])){
                printROMSize = atoi(argv[i + 1]);
                ++i;
                printf("Reading %d bytes of ROM\n", printROMSize);
            }
            else{
                printROMSize = 0;
            }
            printf("Dumping ROM...\n");
            printROM = 1;
        }
        else if ((strcmp("-w", arg) == 0) || ((strcmp("--write", arg) == 0))){
            if (printROM){
                printf("ERROR: Selected read and write operations at the same time. Please do these separately.\n");
                exit(1);
            }
            writeFlag = 1;
        }
    }
    
    return 0;

}

// Handles receiving the ROM dump from the device and formats the output to make it easier to read
// TODO: Maybe add a compare flag and save the ROM into a buffer for comparison later for my comparsion main() arg? 
void getROMFromMachine(uint8_t s[], serial_com COM){
    unsigned long i, j = 0, k;
    unsigned long byteCounter = 0;
    uint8_t rombuf[MAX_ROM_SIZE] = {0};
    uint8_t timeout = 0;

    while (timeout < 3){
        while ((i = recvData(s, COM, 0)) && (j < MAX_ROM_SIZE)){
            for (k = 0; k < i; ++k, ++j){
                rombuf[j] = s[k];
            }
        }
        sleep_ms(10);
        ++timeout;
    }
    printf("ROM Size:%lu\n", j);
    printf("ROM Data:\n");
    for (i = 0; i < j; ++i){
        printf("%02x ", rombuf[i]);
        ++byteCounter;
        if ((byteCounter > 0) && ((byteCounter % 16) == 0)){
            putchar('\n');
        }
        else if ((byteCounter > 0) && ((byteCounter % 8) == 0)){
            putchar(' ');
            putchar(' ');
        }
    }
}

int main(int argc, char *argv[]){
    uint8_t s[BUFFER_SIZE] = {0};
    uint8_t chunk[sizeof(s)];
    uint8_t timeoutct = 0;
    unsigned long i = 1;
    long file_size, data_processed = 0;

    parseArgs(argc, argv);

    // if the port flag wasn't set then we will just set the port to some defined default values
    if (!portFlag){
        printf("no port selected, defaulting to %s\n", defaultPort);
        port = defaultPort; // defined in #ifdefs
    }

    // if file_path and port aren't empty then let's try to upload some data!
    if (fileFlag && writeFlag){
        serial_com COM = openCOM(port, BAUD_RATE, 0);
        if (COM != NO_COM){
            // My target device is an arduino board and this gives it time to reboot
            sleep_ms(2000);

            FILE *fp;
            fp = fopen(file_path, "rb");

            // Let's see how many bytes are in this bin/hex file
            fseek(fp, 0L, SEEK_END);
            file_size = ftell(fp);
            // Now let's set the file pointer back to 0
            fseek(fp, 0L, SEEK_SET);
            s[0] = file_size >> 8;
            s[1] = file_size & 0xFF;

            s[0] = '#';
            sendData(COM, s, 1, DATA_PACKET_FLAG_OFF);
            recvData(s, COM, 1);

            if (s[0] == '#'){
                // Break the 16-bit file_size into 2 bytes and stick then in the s[] buffer
                u16tou8(s, file_size);
                // call sendData and tell it to only send the first 2 bytes (that contain our file_size) and only send this data without adding the header and footer
                sendData(COM, s, 2, DATA_PACKET_FLAG_OFF);
                recvData(s, COM, 2);
                uint16_t tmp = 0;
                tmp = s[0] << 8;
                tmp = tmp | s[1];
                
                //let's setup the progress bar
                char progress_bar[PROGRESS_BAR_LEN]; // the printProgress function will change its bar length depending on the size of this array
                
                // Set all of the elements to a space char
                for (i = 0; i < sizeof(progress_bar); ++i){
                    progress_bar[i] = ' ';
                }
                
                if (file_size == tmp){
                    //printf("ROM size acknowledged\n");

                    // print the empty 00.0% bar to the screen
                    clearScreen();
                    printf("Upload: [");
                    printf("%s", progress_bar);
                    putchar(']');
                    printf(" 00.0%%");
                    
                    // Start gathering bytes from the ROM file and attempt to upload them to the device
                    while ((i = getBytesFromFile(chunk, fp))){
                        // seriously crude timeout control
                        while (timeoutct < 3){
                            sendData(COM, chunk, i, DATA_PACKET_FLAG_ON);
                            recvData(s, COM, 1);
                            // ACK of 1 means successful reception and to send the next chunk
                            if (s[0] == 1){
                                data_processed += i;
                                printProgress(file_size, data_processed, sizeof(progress_bar));
                                timeoutct = 0;
                                break;
                            }
                            else{
                                printProgress(file_size, data_processed, sizeof(progress_bar));
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
                    sendData(COM, s, 1, DATA_PACKET_FLAG_OFF);
                    putchar('\n');
                    
                    // Now we will get the output back from the chip
                    getROMFromMachine(s, COM);

                    // make the cursor visible again
                    printf("\33[?25h");
                }
                else{
                    printf("data is too big to write to ROM!\n");
                }
            }

            fclose(fp);
            closeCOM(COM);
            return 0;
        }
    }
    else if (printROM == 1){
        printf("PrintROM:%d\nport:%s\n", printROM, port);
        serial_com COM = openCOM(port, BAUD_RATE, 0);
        if (COM != NO_COM){
            sleep_ms(2000);
            s[0] = '?';
            printf("sending print rom command\n");
            sendData(COM, s, 1, DATA_PACKET_FLAG_OFF);
            printf("waiting for response\n");
            recvData(s, COM, 1);
            if (s[0] == '?'){
                u16tou8(s, printROMSize);
                sendData(COM, s, 2, DATA_PACKET_FLAG_OFF);
                getROMFromMachine(s, COM);
            }

            printf("\ndone\n");
        }
        closeCOM(COM);
    }
    else if (compareFlag && fileFlag){
        // TODO: ADD THIS IN
    }
    else{
        printf("Not enough arguments\n");
        exit(1);
    }
    return 0;
}
