#ifndef SERIAL_H
    #define SERIAL_H

    #define BAUD_RATE 9600 // Win api wants unsigned long and linux terminos wants usigned int... but neither complain if I don't specify

    #ifdef _WIN32
        #include <windows.h>
        #define NO_COM NULL // When a COM fails to init then it returns NULL on the windows code
        typedef HANDLE serial_com;
    #else
        #include <fcntl.h>
        #include <errno.h>
        #include <termios.h>
        #include <unistd.h>
        #include <dirent.h>
        #include <string.h>
        typedef int serial_com;
        #define NO_COM 0 // When a COM fails to init then it returns 0 on the Linux code
    #endif

    void closeCOM(serial_com COM);
    serial_com openCOM(char comPort[], int baudrate, int list_flag);
    void listCOMPorts(void);
#endif