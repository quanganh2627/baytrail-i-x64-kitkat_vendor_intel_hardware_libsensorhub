#ifndef _PLATFORM_DEF_H
#define _PLATFORM_DEF_H

#include "libsensorhub.h"
#include "message.h"

/* include platfrom special head file */
#include "generic_sensors.h"

/* this function implemented in main.c and can be used by all platforms */
void dispatch_streaming(struct cmd_resp *p_cmd_resp);

typedef struct {
	char *platform_name;
	unsigned int index_start;
	unsigned int index_end;

	int (*init)(void *p_sensor_list, unsigned int *index);
	int (*send_cmd)(int tran_id, int cmd_id, ish_sensor_t sensor_type,
			unsigned short data_rate, unsigned short buffer_delay, unsigned short bit_cfg);
	int (*add_fds)(int maxfd, void *read_fds, int *hw_fds, int *hw_fds_num);
	int (*process_fd)(int fd);
} ish_platform_t;

/* database for all platforms */
ish_platform_t ish_platf[] = {
	[0] = {
		.platform_name = "generic sensor",
		.init = init_generic_sensors,
		.send_cmd = generic_sensor_send_cmd,
		.add_fds = add_generic_sensor_fds,
		.process_fd = process_generic_sensor_fd,
	},
};

#endif
