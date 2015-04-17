#ifndef _LPE_H_
#define _LPE_H_

#include <sys/select.h>

#ifdef __cplusplus
extern "C" {
#endif

        int lpe_init(void *p_sensor_list, unsigned int *index);
        int lpe_add_fd(int maxfd, void *read_fds, int *hw_fds, int *hw_fds_num);
        int lpe_process_fd(int fd);
        int lpe_get_fd();
        error_t lpe_start_streaming(int data_rate, int buffer_delay);
        error_t lpe_stop_streaming();
#ifdef SUPPORT_ANDROID_SENSOR_HAL
        error_t lpe_flush_streaming();
#endif

#ifdef __cplusplus
}
#endif

#endif
