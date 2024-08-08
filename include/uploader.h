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

// TODO: Have the programming device send the buffer size it uses and make this dynamic on the PC end
#define BUFFER_SIZE 64 + 1 // +1 for null terminator on output
#define DATA_MAX BUFFER_SIZE-3 // -3 accounts for the header and footer added to the data chunk when sending
#define MAX_ROM_SIZE 32767
#define DATA_PACKET_FLAG_ON 1
#define DATA_PACKET_FLAG_OFF 0