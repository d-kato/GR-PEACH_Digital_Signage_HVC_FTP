
#if (MBED_CONF_APP_FTP_CONNECT == 1) || (MBED_CONF_APP_NTP_CONNECT == 1)
#include "mbed.h"
#include "DisplayDebugLog.h"
#include "network_proc.h"
#if (MBED_CONF_APP_FTP_CONNECT == 1)
#include "FTPClient.h"
#endif
#if (MBED_CONF_APP_NTP_CONNECT == 1)
#include "NTPClient.h"
#include "FileAnalysis.h"
#endif

#define ETHERNET        1
#define WIFI_BP3595     2

#if MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
  #include "EthernetInterface.h"
  EthernetInterface network;
#elif MBED_CONF_APP_NETWORK_INTERFACE == WIFI_BP3595
  #include "LWIPBP3595Interface.h"
  LWIPBP3595Interface network;
#else
  #error "No connectivity method chosen. Please add 'config.network-interfaces.value' to your mbed_app.json."
#endif

#ifdef MBED_CONF_APP_IP_ADDRESS
  #ifndef MBED_CONF_APP_SUBNET_MASK
    #error "Please add 'config.subnet-mask.value' to your mbed_app.json."
  #endif
  #ifndef MBED_CONF_APP_DEFAULT_GATEWAY
    #error "Please add 'config.default-gateway.value' to your mbed_app.json."
  #endif
#endif

bool network_connect(const char * mount_path, SdUsbConnect * p_storage) {
    DrawDebugLog("Network Setting up...\r\n");
#ifdef MBED_CONF_APP_IP_ADDRESS
    network.set_dhcp(false);
    if (network.set_network(MBED_CONF_APP_IP_ADDRESS, MBED_CONF_APP_SUBNET_MASK, MBED_CONF_APP_DEFAULT_GATEWAY) != 0) {
        DrawDebugLog("Network Set Network Error \r\n");
        return false;
    }
#endif
#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI_BP3595
    network.set_credentials(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, MBED_CONF_APP_WIFI_SECURITY);
#endif
    if (network.connect() != 0) {
        DrawDebugLog("Network Connect Error \r\n");
        network.disconnect();
        return false;
    }
    DrawDebugLog("MAC Address is %s\r\n", network.get_mac_address());
    DrawDebugLog("IP Address is %s\r\n", network.get_ip_address());
    DrawDebugLog("NetMask is %s\r\n", network.get_netmask());
    DrawDebugLog("Gateway Address is %s\r\n", network.get_gateway());
    DrawDebugLog("Network Setup OK\r\n");

#if (MBED_CONF_APP_NTP_CONNECT == 1)
    {
        NTPClient ntp(&network);

        ntp.set_server((char*)MBED_CONF_APP_NTP_ADDRESS, MBED_CONF_APP_NTP_PORT);
        time_t timestamp = ntp.get_timestamp();
        if (timestamp < 0) {
            DrawDebugLog("An error occurred when getting the time. %ld\r\n", timestamp);
        } else {
            set_time(timestamp);
            time_t seconds = time(NULL) + (60 * 60 * 9); // JST
            DrawDebugLog("Current time is %s\r\n", ctime(&seconds));
        }
    }
#endif
#if (MBED_CONF_APP_FTP_CONNECT == 1)
    {
        bool ret = false;
        char* token = NULL;
        char* savept = NULL;

        while (ret == false) {
            FTPClient myFTP(&network, mount_path);

            //Configure the display driver
            DrawDebugLog("Connecting to FTPServer...\r\n");
            if (myFTP.open(MBED_CONF_APP_FTP_IP_ADDRESS, MBED_CONF_APP_FTP_PORT,
                           MBED_CONF_APP_FTP_USER, MBED_CONF_APP_FTP_PASSWORD)) {
                char * filelist = new char[1024];
                DrawDebugLog("Connect Success\r\n");
                if (filelist == NULL) {
                    DrawDebugLog("Memory Allocation Error : %08x\n", 1024);
                } else {
                    DrawDebugLog("Formatting storage...\r\n");
                    p_storage->format();
                    DrawDebugLog("done\r\n");

                    memset(filelist, 0, 1024);
                    myFTP.cd((char*)MBED_CONF_APP_FTP_DIRECTORY);
                    myFTP.ls(filelist, 1024);
                    token = filelist;
                    savept = NULL;
                    while (token) {
                        token = strtok_r(token,"\r\n", &savept);
                        if (token != NULL) {
                            DrawDebugLog("Get File:%s\r\n",token);
                            myFTP.get(token);
                            token = savept;
                        }
                    }
                    myFTP.quit();
                    DrawDebugLog("Quit FTPServer\r\n");
                    delete[] filelist;
                    ret = true;
                }
            } else {
                DrawDebugLog("Connect error\r\n");
            }
        }
    }
#endif

    network.disconnect();

    return true;
}

#if (MBED_CONF_APP_NTP_CONNECT == 1)
typedef struct {
    int  time_min;
    char * file_name;
} time_tbl_t;

#define MAX_TIME_TBL_NUM  (16)
static time_tbl_t timetable[MAX_TIME_TBL_NUM];
static int timetable_num;
static int timetable_idx;
static int timetable_tm_mday;
static bool timetable_end;

bool init_timetable(uint32_t * p_total_file_num) {
    uint8_t * work_buff = new uint8_t[1024];
    size_t read_size;
    char wk_str[3] = {0};
    int file_name_size;
    int tbl_idx = 0;

    for (int i = 0; i < MAX_TIME_TBL_NUM; i++) {
        timetable[i].time_min = -1;
    }
    timetable_num = 0;
    timetable_idx = 0;
    timetable_end = false;

    if (work_buff == NULL) {
        return false;
    }

    memset(work_buff, 0, 1024);
    read_size = GetFileData(work_buff, 1024, "timetable.txt");
    if (read_size == 0) {
        delete[] work_buff;
        return false;
    }

    char* token = (char *)work_buff;
    char* savept = NULL;

    while (token && (tbl_idx < MAX_TIME_TBL_NUM)) {
        token = strtok_r(token,"\r\n", &savept);
        if (token != NULL) {
            int len;

            printf("%s\r\n", token);
            len = strlen(token);
            if (len >= 11) {
                wk_str[0] = token[0];
                wk_str[1] = token[1];
                timetable[tbl_idx].time_min = atoi(wk_str) * 60;
                wk_str[0] = token[3];
                wk_str[1] = token[4];
                timetable[tbl_idx].time_min += atoi(wk_str);

                file_name_size = strlen(&token[6]) + 1;
                timetable[tbl_idx].file_name = new char[file_name_size];
                memcpy(timetable[tbl_idx].file_name, &token[6], file_name_size);
                tbl_idx++;
            }
            token = savept;
        }
    }
    delete[] work_buff;
    timetable_num = tbl_idx;

    if (timetable_num <= 0) {
        return false;
    }

    time_t seconds = time(NULL) + (60 * 60 * 9); // JST
    struct tm *t = localtime(&seconds);
    int time_min_now = (t->tm_hour * 60) + t->tm_min;

    timetable_tm_mday = t->tm_mday;

    timetable_idx = timetable_num - 1;
    for (int i = 0; i < timetable_num; i++) {
        if (timetable[i].time_min > time_min_now) {
            break;
        }
        timetable_idx = i;
    }

    *p_total_file_num = FileAnalysis(timetable[timetable_idx].file_name);
    timetable_idx++;
    if (timetable_idx >= timetable_num) {
        timetable_idx = 0;
    }
    printf("The next timetable is at %02d:%02d. %s\r\n",
           timetable[timetable_idx].time_min / 60,
           timetable[timetable_idx].time_min % 60,
           timetable[timetable_idx].file_name);

    return true;
}

bool check_timetable(uint32_t * p_total_file_num) {
    if (timetable_num <= 1) {
        return false;
    }

    time_t seconds = time(NULL) + (60 * 60 * 9); // JST
    struct tm *t = localtime(&seconds);
    int time_min_now = (t->tm_hour * 60) + t->tm_min;

    if (timetable_end) {
        if (timetable_tm_mday != t->tm_mday) {
            timetable_end = false;
        } else {
            return false;
        }
    }

    if (time_min_now < timetable[timetable_idx].time_min) {
        return false;
    }

    printf("%04d/%02d/%02d %02d:%02d:%02d\r\n",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    *p_total_file_num = FileAnalysis(timetable[timetable_idx].file_name);
    timetable_idx++;
    if (timetable_idx >= timetable_num) {
        timetable_idx = 0;
        timetable_end = true;
        timetable_tm_mday = t->tm_mday;
    }
    printf("The next timetable is at %02d:%02d. %s\r\n",
           timetable[timetable_idx].time_min / 60,
           timetable[timetable_idx].time_min % 60,
           timetable[timetable_idx].file_name);

    return true;
}
#endif

#endif // (MBED_CONF_APP_FTP_CONNECT == 1) || (MBED_CONF_APP_NTP_CONNECT == 1)
