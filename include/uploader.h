#ifdef _WIN32
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #define MAX_FILE_NAME_LEN MAX_PATH
    char defaultPort[] = "COM4";

#else
    #include <fcntl.h>
    #include <errno.h>
    #include <termios.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <string.h>
    #include <time.h> // for nanosleep()
    #define MAX_FILE_NAME_LEN PATH_MAX
    char defaultPort[] = "ttyUSB0";

#endif

// Why do I have this adjustable anyway?
#define PROGRESS_BAR_LEN 50 // Length of the progress bar during upload

// Flag struct used to select options on the main program.
// bitfield to save a bit of memory
struct bitflags {
    uint8_t fileFlag : 1;
    uint8_t portFlag : 1;
    uint8_t writeFlag : 1;
    uint8_t compareFlag : 1;
    uint8_t targetFlag : 1;
    uint8_t debugFlag : 1;
    uint8_t printROM : 1;
};

#define SOH 0x01 // Start of header
#define EOT 0x04 // End of Transmission
#define ACK 0x06 // Ack
#define NAK 0x15 // No Ack
#define ETB 0x17 // End of transmit block
#define CAN 0x18 // Cancel

#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_BUFFER_SIZE 64
#define DATA_MAX BUFFER_SIZE - 4 // - 4 accounts for the header and footer added to the data chunk when sending
#define MAX_ROM_SIZE 16383
#define DATA_HEADER_SIZE 2
#define DATA_FOOTER_SIZE 2

// if flag is on, data will be sent with header and footer, otherwise it will just send the data only
#define DATA_PACKET_FLAG_ON 1
#define DATA_PACKET_FLAG_OFF 0