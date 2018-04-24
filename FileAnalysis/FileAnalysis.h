
#ifndef FILE_ANALYSIS_H
#define FILE_ANALYSIS_H

extern void SetMountPath(const char * mount_path);
extern uint32_t MemoryAnalysis(char * src_buf, const char *delim);
extern uint32_t FileAnalysis(const char * index_file);
extern size_t GetFileData(uint8_t * p_buf, size_t size, const char * file_name);
extern const char * get_file_name(uint32_t file_id);

#endif
