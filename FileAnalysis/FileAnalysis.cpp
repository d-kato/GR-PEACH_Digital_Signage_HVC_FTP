#include "mbed.h"

#include "FATFileSystem.h"
#include <string>
#include <map>
#include "DisplayDebugLog.h"

static std::map<int, std::string> m_lpJpegfiles;
static char mount_path_cpy[32];

/****** File Access ******/
size_t GetFileData(uint8_t * p_buf, size_t size, const char * file_name) {
    size_t read_size = 0;
    FILE * fp;
    char file_path[64];

    sprintf(file_path, "%s/%s", mount_path_cpy, file_name);
    fp = fopen(file_path, "r");
    if (fp != NULL) {
        setvbuf(fp, NULL, _IONBF, 0); // unbuffered
        read_size = fread(p_buf, sizeof(char), size, fp);
        fclose(fp);
    }

    return read_size;
}

const char * get_file_name(uint32_t file_id) {
    return m_lpJpegfiles[file_id].c_str();
}

void SetMountPath(const char * mount_path) {
    if (strlen(mount_path) > sizeof(mount_path_cpy)) {
        DrawDebugLog("mount_path length error\r\n");
        return;
    }
    strcpy(mount_path_cpy, mount_path);
}

uint32_t MemoryAnalysis(char * src_buf, const char *delim) {
    uint32_t file_index = 0;
    FILE* fp;
    char file_path[64];
    char* token = src_buf;
    char* savept = NULL;

    m_lpJpegfiles.clear();

    while (token) {
        token = strtok_r(token, delim, &savept);
        if (token != NULL) {
            if (strstr(token,".jpg")) {
                sprintf(file_path, "%s/%s", mount_path_cpy, token);
                fp = fopen(file_path, "r");
                if (fp != NULL) {
                    fclose(fp);
                    m_lpJpegfiles[file_index] = token;
                    file_index++;
                }
            }
            token = savept;
        }
    }

    return file_index;
}

uint32_t FileAnalysis(const char * index_file) {
    uint32_t file_index = 0;
    FILE* fp;
    char file_path[64];

    sprintf(file_path, "%s/%s", mount_path_cpy, index_file);
    fp = fopen(file_path, "r");
    if (fp != NULL) {
        char* token = NULL;
        char* savept = NULL;
        char * work_buff = new char[1024];

        if (work_buff == NULL) {
            DrawDebugLog("Memory error FileAnalysis\r\n");
            return 0;
        }

        DrawDebugLog("\"%s\" is being read...\r\n", index_file);
        memset(work_buff, 0, 1024);
        fread(work_buff, sizeof(char), 1024, fp);
        fclose(fp);

        file_index = MemoryAnalysis(work_buff, "\r\n");
        DrawDebugLog("done\r\n");
        delete[] work_buff;
    } else {
        DIR  * d;
        struct dirent * p;

        m_lpJpegfiles.clear();
        DrawDebugLog("\"%s\" was not found\r\n", index_file);
        sprintf(file_path, "%s/", mount_path_cpy);
        d = opendir(file_path);
        if (d != NULL) {
            while ((p = readdir(d)) != NULL) {
                if (strstr(p->d_name,".jpg")) {
                    m_lpJpegfiles[file_index] = p->d_name;
                    file_index++;
                }
            }
            closedir(d);
        }
    }

    return file_index;
}

