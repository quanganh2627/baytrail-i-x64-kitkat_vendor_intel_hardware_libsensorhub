#ifndef _PLATFORM_DEF_H
#define _PLATFORM_DEF_H

#include "libsensorhub.h"
#include "message.h"

/* include platfrom special head file */
#include "generic_sensors.h"
#include "lpe.h"

/* this function implemented in main.c and can be used by all platforms */
void dispatch_streaming(struct cmd_resp *p_cmd_resp);

typedef struct {
	char *platform_name;
	unsigned int index_start;
	unsigned int index_end;

	int (*init)(void *p_sensor_list, unsigned int *index);
	int (*send_cmd)(struct cmd_send *cmd);
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
	[1] = {
		.platform_name = "lpe sensor",
		.init = lpe_init,
		.send_cmd = NULL,
		.add_fds = lpe_add_fd,
		.process_fd = lpe_process_fd,
	},
};

#endif
