
#ifndef MBED_CLIENT_PROC_H
#define MBED_CLIENT_PROC_H

extern void mbed_client_task(void);
extern void set_hvc_result(char * str);
extern void set_total_expression(char * str);
extern bool check_new_playlist(uint32_t * p_total_file_num);

#endif
