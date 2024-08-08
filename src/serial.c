#include "../include/serial.h"
#include <stdio.h>

void closeCOM(serial_com COM){
    #ifdef _WIN32
        CloseHandle(COM);
    #endif
    #ifdef __linux__
        close(COM);
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

        
       (void)list_flag; // temp measure to get my compiler to stop complaining about the unused list_flag arg

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
    
    return 0;
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