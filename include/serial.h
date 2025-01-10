#ifndef SERIAL_H
    #define SERIAL_H
    #define COMTIMEOUT 5000 // In milliseconds
    //#define BAUD_RATE 9600 // Win api wants unsigned long and linux terminos wants usigned int... but neither complain if I don't specify
    #include <stdint.h>
    #ifdef _WIN32
        #include <windows.h>
        #define NO_COM NULL // When a COM fails to init then it returns NULL on the windows code
        typedef HANDLE serial_com; 
        #define SERIAL_TIMEOUT COMTIMEOUT
    #else
        #include <fcntl.h>
        #include <errno.h>
        #include <termios.h>
        #include <unistd.h>
        #include <dirent.h>
        #include <string.h>
        typedef int serial_com;
        #define NO_COM 0 // When a COM fails to init then it returns 0 on the Linux code
        #define SERIAL_TIMEOUT COMTIMEOUT / 100
        uint32_t get_baud(int baud);
    #endif

    void closeCOM(serial_com COM);
    serial_com openCOM(char comPort[], int baudrate, int list_flag);
    void listCOMPorts(void);
#endif