#include "../include/serial.h"

void closeCOM(serial_com COM){
    #ifdef _WIN32
        CloseHandle(COM);
    #endif
    #ifdef __linux__
        #include <unistd.h> // for close()
        close(COM);
    #endif
}
