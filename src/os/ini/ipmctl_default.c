#if defined(__LINUX__) || defined(__ESX__)
#define TEMP_FILE_PATH "/var/log/ipmctl/"
#else
#define TEMP_FILE_PATH "%APPDATA%\\Intel\\ipmctl\\"
#endif
#include "ipmctl_default.h"
