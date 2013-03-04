#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <string.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <pthread.h>


#include "../include/socket.h"
#include "../include/utils.h"
#include "../include/message.h"

#define MAX_STRING_SIZE 256

#define MAX_DEV_NODE_NUM 8

static int node_number = 2;

#define WAKE_NODE "/sys/power/wait_for_fb_wake"
#define SLEEP_NODE "/sys/power/wait_for_fb_sleep"

/* The Unix socket file descriptor */
static int sockfd = -1, ctlfd = -1, datafd = -1, datasizefd = -1, wakefd = -1, sleepfd = -1, notifyfd = -1;

static int screen_state;
static int enable_debug_data_rate = 0;

#define COMPASS_CALIBRATION_FILE "/data/compass.conf"
#define GYRO_CALIBRATION_FILE "/data/gyro.conf"

/* definition and struct for calibration used by psh_fw */
#define CMD_CALIBRATION_SET	(0x1)
#define CMD_CALIBRATION_GET	(0x2)
#define CMD_CALIBRATION_START	(0x3)
#define CMD_CALIBRATION_STOP	(0x4)
struct cmd_compasscal_param {
	unsigned char sub_cmd;
	struct compasscal_info info;
} __attribute__ ((packed));

struct cmd_gyrocal_param {
	unsigned char sub_cmd;
	struct gyrocal_info info;
} __attribute__ ((packed));

#define CALIB_RESULT_NONE	(0)
#define CALIB_RESULT_USER 	(1)
#define CALIB_RESULT_AUTO 	(2)
struct resp_compasscal {
	unsigned char calib_result;
	struct compasscal_info info;
} __attribute__ ((packed));

struct resp_gyrocal {
	unsigned char calib_result;
	struct gyrocal_info info;
} __attribute__ ((packed));

/* Calibration status */
#define	CALIBRATION_INIT	(1 << 1)	// Init status
#define	CALIBRATION_RESET	(1 << 2)	// Reset status
#define	CALIBRATION_IN_PROGRESS (1 << 3)	// Calibration request has been sent but get no reply
#define	CALIBRATION_DONE	(1 << 4)	// Get CALIBRATION_GET response
#define	CALIBRATION_NEED_STORE	(1 << 31)	// Additional sepcial status for parameters stored to file

typedef enum {
	INACTIVE = 0,
	ACTIVE,
	ALWAYS_ON,
	NEED_RESUME,
} state_t;

/* data structure to keep per-session state */
typedef struct session_state_t {
	state_t state;
	char flag;		// To differentiate between no_stop (0) and no_stop_no_report (1)
	int get_single;		// 1: pending; 0: not pending
	int get_calibration;
	int datafd;
	char datafd_invalid;
	int ctlfd;
	session_id_t session_id;
	union {
	int data_rate;
	unsigned char trans_id;
	};
	union {
	int buffer_delay;
	unsigned char event_id;
	};
	int tail;
	struct session_state_t *next;
} session_state_t;

/* data structure to keep state for a particular type of sensor */
typedef struct {
	psh_sensor_t sensor_type;
	short freq_max;
	int data_rate;
	int buffer_delay;
	int calibration_status; // only for compass_cal and gyro_cal
	session_state_t *list;
} sensor_state_t;

static sensor_state_t sensor_list[SENSOR_MAX];

/* Daemonize the sensorhubd */
static void daemonize()
{
	pid_t sid, pid = fork();

	if (pid < 0) {
		log_message(CRITICAL, "error in fork daemon. \n");
		exit(EXIT_FAILURE);
	} else if (pid > 0)
		exit(EXIT_SUCCESS);

	/* new SID for daemon process */
	sid = setsid();
	if (sid < 0) {
		log_message(CRITICAL, "error in setsid(). \n");
		exit(EXIT_FAILURE);
	}

	/* close STD fds */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	struct passwd *pw;

	pw = getpwnam("system");
	if (pw == NULL) {
		log_message(CRITICAL, "daemonize(): getpwnam return NULL \n");
		return;
	}
	setuid(pw->pw_uid);

	log_message(DEBUG, "sensorhubd is started in daemon mode. \n");
}

static session_state_t* get_session_state_with_session_id(
					session_id_t session_id)
{
	int i = 0;

	log_message(DEBUG, "get_session_state_with_session_id(): "
				"session_id %d\n", session_id);

	while (i < SENSOR_MAX) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->session_id == session_id)
				return p_session_state;

			p_session_state = p_session_state->next;
		}

		i++;
	}

	log_message(WARNING, "get_session_state_with_session_id(): "
				"NULL is returned\n");

	return NULL;
}

#define MAX_UNSIGNED_INT 0xffffffff

static session_id_t allocate_session_id()
{
        /* rewind session ID */
	static session_id_t session_id = 0;
	static int rewind = 0;

	session_id++;

	if (rewind) {
		while (get_session_state_with_session_id(session_id) != NULL)
			session_id++;
	}

	if (session_id == MAX_UNSIGNED_INT)
		rewind = 1;

	return session_id;
}

/* return 0 on success; -1 on fail */
static int get_sensor_type_session_state_with_fd(int ctlfd,
					psh_sensor_t *sensor_type,
					session_state_t **pp_session_state)
{
	int i = 0;

	while (i < SENSOR_MAX) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->ctlfd == ctlfd) {
				*sensor_type = i;
				*pp_session_state = p_session_state;

				return 0;
			}

			p_session_state = p_session_state->next;
		}

		i++;
	}

	return -1;
}

/* flag: 1 means start_streaming, 0 means stop_streaming */
static int data_rate_arbiter(psh_sensor_t sensor_type,
				int data_rate,
				session_state_t *p_session_state,
				char flag)
{
	session_state_t *p_session_state_tmp;
	int data1, data2;

	if (flag == 1) {
		data1 = data_rate;

		if (sensor_list[sensor_type].data_rate == 0)
			return data_rate;
		else if (data_rate == 0)
			return sensor_list[sensor_type].data_rate;

/*		p_session_state_tmp = sensor_list[sensor_type].list;

		while (p_session_state_tmp != NULL) {
			if ((p_session_state_tmp != p_session_state)
				&& ((p_session_state_tmp->state == ACTIVE)
				|| (p_session_state_tmp->state == ALWAYS_ON))) {
				data2 = p_session_state_tmp->data_rate;
				data1 = least_common_multiple(data1, data2);
			}

			p_session_state_tmp = p_session_state_tmp->next;
		}
*/
//		return data1;
/*
		r = least_common_multiple(sensor_list[sensor_type].data_rate,
								data_rate);

		return r;
*/
	} else {	/* flag == 0 */
		data1 = 1;
		data2 = 0;

/*
		if (data2 == 0)
			return 0;
		else
			return data1;
*/
	}

	p_session_state_tmp = sensor_list[sensor_type].list;

	while (p_session_state_tmp != NULL) {
		if ((p_session_state_tmp != p_session_state)
			&& ((p_session_state_tmp->state == ACTIVE)
			|| (p_session_state_tmp->state == ALWAYS_ON))) {
			data2 = p_session_state_tmp->data_rate;
			data1 = least_common_multiple(data1, data2);
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	if (flag == 1)
		return data1;
	else {		/* flag == 0 */
		if (data2 == 0)
			return 0;
		else
			return data1;
	}

}

/* flag: 1 means add a new buffer_delay, 0 means remove a buffer_delay */
static int buffer_delay_arbiter(psh_sensor_t sensor_type,
				int buffer_delay,
				session_state_t *p_session_state,
				char flag)
{
	if (flag == 1) {
		int r;
		if (buffer_delay == 0)
			return 0;

		/* a new buffer_delay and not the first one */
		if (sensor_list[sensor_type].buffer_delay == 0) {
			session_state_t *tmp = sensor_list[sensor_type].list;
			while (tmp != NULL) {
				if ((tmp != p_session_state)
					&& ((tmp->state == ACTIVE)
					|| (tmp->state == ALWAYS_ON)))
					return 0;
				tmp = tmp->next;
			}
		}
		r = max_common_factor(sensor_list[sensor_type].buffer_delay,
								buffer_delay);
		return r;
	}
	/* flag == 0 */
	int data1 = 0, data2;
	session_state_t *p_session_state_tmp = sensor_list[sensor_type].list;

	if ((sensor_list[sensor_type].buffer_delay == 0) && (buffer_delay != 0))
		return 0;

	while (p_session_state_tmp != NULL) {
		if ((p_session_state_tmp != p_session_state)
			&& ((p_session_state_tmp->state == ACTIVE)
			|| (p_session_state_tmp->state == ALWAYS_ON))) {
			if (p_session_state_tmp->buffer_delay == 0)
				return 0;

			data2 = p_session_state_tmp->buffer_delay;
			data1 = max_common_factor(data1, data2);
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	return data1;
}

static int sensor_type_to_sensor_id[SENSOR_MAX] = { 0 };
enum resp_type {
	RESP_CMD_ACK,
	RESP_GET_TIME,
	RESP_GET_SINGLE,
	RESP_STREAMING,
	RESP_DEBUG_MSG,
	RESP_DEBUG_GET_MASK = 5,
	RESP_GYRO_CAL_RESULT,
	RESP_BIST_RESULT,
	RESP_ADD_EVENT,
	RESP_CLEAR_EVENT,
	RESP_EVENT = 10,
	RESP_GET_STATUS,
	RESP_COMP_CAL_RESULT,
};

static int cmd_type_to_cmd_id[CMD_MAX] = {2, 3, 4, 5, 6, 9, 9, 12};

static int send_control_cmd(int tran_id, int cmd_id, int sensor_id, unsigned short data_rate, unsigned short buffer_delay);

static void error_handling(int error_no)
{

	/* 0 issue RESET and wait for psh fw to be ready */

	/* 1 re-issue start_streaming */
	int i, ret;

restart:
	for (i = SENSOR_ACCELEROMETER; i < SENSOR_MAX; i++) {
		/* no start_streaming with below ones */
		if ((i == SENSOR_CALIBRATION_COMP)
			|| (i == SENSOR_CALIBRATION_GYRO)
			|| (i == SENSOR_EVENT))
			continue;

		/* no pending active request */
		if (sensor_list[i].data_rate == 0)
			continue;

		ret = send_control_cmd(0, cmd_type_to_cmd_id[CMD_START_STREAMING], sensor_type_to_sensor_id[i],
				sensor_list[i].data_rate, sensor_list[i].buffer_delay);
		if (ret < 0)
			goto restart;
	}

	/* 2 re-issue get_single */

	/* 3 re-issue set_property */

	/* 4 re-issue calibration */
}

/* 0 on success; -1 on fail */
static int send_control_cmd(int tran_id, int cmd_id, int sensor_id,
				unsigned short data_rate, unsigned short buffer_delay)
{
	char cmd_string[MAX_STRING_SIZE];
	int size;
	ssize_t ret;
	unsigned char *data_rate_byte = (unsigned char *)&data_rate;
	unsigned char *buffer_delay_byte = (unsigned char *)&buffer_delay;

	size = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d %d %d %d %d",
				tran_id, cmd_id, sensor_id, data_rate_byte[0],
				data_rate_byte[1], buffer_delay_byte[0],
				buffer_delay_byte[1]);
	log_message(DEBUG, "cmd to sysfs is: %s\n", cmd_string);
	/* TODO: error handling if (size <= 0 || size > MAX_STRING_SIZE) */
	ret = write(ctlfd, cmd_string, size);
	log_message(DEBUG, "cmd return value is %d\n", ret);
	if (ret < 0)
		return -1;

	return 0;
}

static int send_set_property(psh_sensor_t sensor_type, property_type prop_type, int value)
{
	char cmd_string[MAX_STRING_SIZE];
	int size, ret;
	unsigned char *prop_type_byte = (unsigned char *)&prop_type;
	unsigned char *value_byte = (unsigned char *)&value;

	size = snprintf(cmd_string, MAX_STRING_SIZE, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", 0,
			cmd_type_to_cmd_id[CMD_SET_PROPERTY], sensor_type_to_sensor_id[sensor_type], 8,
			prop_type_byte[0], prop_type_byte[1], prop_type_byte[2], prop_type_byte[3], value_byte[0], value_byte[1], value_byte[2], value_byte[3]
			);

	log_message(DEBUG, "cmd to sysfs is: %s\n", cmd_string);
	ret = write(ctlfd, cmd_string, size);

	log_message(DEBUG, "cmd return value is %d\n", ret);
	if (ret < 0)
		return -1;

	return 0;
}

/* flag: 2 means no_stop_no_report; 1 means no_stop when screen off; 0 means stop when screen off */
static void start_streaming(psh_sensor_t sensor_type,
				session_state_t *p_session_state,
				int data_rate, int buffer_delay, int flag)
{
	int data_rate_arbitered, buffer_delay_arbitered;


	if (data_rate != -1)
		data_rate_arbitered = data_rate_arbiter(sensor_type, data_rate,
						p_session_state, 1);
	else
		data_rate_arbitered = -1;

	buffer_delay_arbitered = buffer_delay_arbiter(sensor_type, buffer_delay,
						p_session_state, 1);

	log_message(DEBUG, "CMD_START_STREAMING, data_rate_arbitered "
				"is %d, buffer_delay_arbitered is %d \n",
				data_rate_arbitered, buffer_delay_arbitered);

	sensor_list[sensor_type].data_rate = data_rate_arbitered;
	sensor_list[sensor_type].buffer_delay = buffer_delay_arbitered;

	/* send CMD_START_STREAMING to psh_fw no matter data rate changes or not,
	   this is requested by Dong for terminal context sensor on 2012/07/17 */
//	if ((sensor_list[sensor_type].data_rate != data_rate_arbitered)
//	||(sensor_list[sensor_type].buffer_delay != buffer_delay_arbitered)) {
		/* send cmd to sysfs control node to change data_rate.
		   NO NEED to send CMD_STOP_STREAMING
		   before CMD_START_STREAMING according to Dong's enhancement
		   on 2011/12/29 */
//		send_control_cmd(0, cmd_type_to_cmd_id[CMD_STOP_STREAMING],
//				sensor_type_to_sensor_id[sensor_type], 0, 0);

		send_control_cmd(0, cmd_type_to_cmd_id[CMD_START_STREAMING],
				sensor_type_to_sensor_id[sensor_type],
				data_rate_arbitered, buffer_delay_arbitered);
//	}

	if (data_rate != 0) {
		p_session_state->data_rate = data_rate;
		p_session_state->buffer_delay = buffer_delay;
		p_session_state->tail = 0;
		if (flag == 0)
			p_session_state->state = ACTIVE;
		else if (flag == 1)
			p_session_state->state = ALWAYS_ON;
		else if (flag == 2) {
			p_session_state->state = ALWAYS_ON;
			p_session_state->flag = 1;
		}
	}
}

static void stop_streaming(psh_sensor_t sensor_type,
				session_state_t *p_session_state)
{
	int data_rate_arbitered, buffer_delay_arbitered;

	if ((p_session_state->state == INACTIVE) || (p_session_state->state == NEED_RESUME))
		return;

	data_rate_arbitered = data_rate_arbiter(sensor_type,
						p_session_state->data_rate,
						p_session_state, 0);
	buffer_delay_arbitered = buffer_delay_arbiter(sensor_type,
						p_session_state->buffer_delay,
						p_session_state, 0);

	p_session_state->state = INACTIVE;

	if ((sensor_list[sensor_type].data_rate == data_rate_arbitered)
	&& (sensor_list[sensor_type].buffer_delay == buffer_delay_arbitered))
		return;

	sensor_list[sensor_type].data_rate = data_rate_arbitered;
	sensor_list[sensor_type].buffer_delay = buffer_delay_arbitered;

	if (data_rate_arbitered != 0) {
		/* send CMD_START_STREAMING to sysfs control node to
		   re-config the data rate */
		send_control_cmd(0, cmd_type_to_cmd_id[CMD_START_STREAMING],
				sensor_type_to_sensor_id[sensor_type],
				data_rate_arbitered, buffer_delay_arbitered);
	} else {
		/* send CMD_STOP_STREAMING cmd to sysfs control node */
		send_control_cmd(0, cmd_type_to_cmd_id[CMD_STOP_STREAMING],
			sensor_type_to_sensor_id[sensor_type], 0, 0);
	}

//	sensor_list[sensor_type].count = 0;

	log_message(DEBUG, "CMD_STOP_STREAMING, data_rate_arbitered is %d, "
			"buffer_delay_arbitered is %d \n", data_rate_arbitered,
			buffer_delay_arbitered);
}

static void get_single(psh_sensor_t sensor_type,
				session_state_t *p_session_state)
{
	log_message(DEBUG, "get_single is called with sensor_type %d \n",
							sensor_type);

	/* send CMD_GET_SINGLE to sysfs control node */
	p_session_state->get_single = 1;
	send_control_cmd(0, cmd_type_to_cmd_id[CMD_GET_SINGLE],
			sensor_type_to_sensor_id[sensor_type], 0, 0);
}

static void get_calibration(psh_sensor_t sensor_type,
				session_state_t *p_session_state)
{
	char cmdstring[MAX_STRING_SIZE];
	int ret, len;

	if (sensor_type != SENSOR_CALIBRATION_COMP &&
		sensor_type != SENSOR_CALIBRATION_GYRO) {
		log_message(DEBUG, "Get calibration with confused parameter,"
					"sensor type is not for calibration, is %d\n",
					sensor_type);
		return;
	}

	len = snprintf (cmdstring, MAX_STRING_SIZE, "%d %d %d %d",
			0,			// tran_id
			cmd_type_to_cmd_id[CMD_SET_CALIBRATION],	// cmd_id
			sensor_type_to_sensor_id[sensor_type],	// sensor_id
			SUBCMD_CALIBRATION_GET);		// sub_cmd

	log_message(DEBUG, "Send to control node :%s\n", cmdstring);
	ret = write(ctlfd, cmdstring, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);

	if (p_session_state)
		p_session_state->get_calibration = 1;
}

static void set_calibration(psh_sensor_t sensor_type,
				struct cmd_calibration_param *param)
{
	char cmdstring[MAX_STRING_SIZE];
	int ret, len;

	if (sensor_type != SENSOR_CALIBRATION_COMP &&
		sensor_type != SENSOR_CALIBRATION_GYRO) {
		log_message(DEBUG, "Set calibration with confused parameter,"
					"sensor type is not for calibration, is %d\n",
					sensor_type);
		return;
	}

	if (param->sensor_type != sensor_type) {
		log_message(DEBUG, "Set calibration with confused parameter,"
					"sensor_type is %d, but parameter is %d\n",
					sensor_type, param->sensor_type);
		return;
	}

	/* Too bad we cannot use send_control_cmd, send cmd by hand. */
	// echo trans cmd sensor [sensor subcmd calibration [struct parameter info]]

	log_message(DEBUG, "Begin to form command string.\n");
	len = snprintf (cmdstring, MAX_STRING_SIZE, "%d %d %d %d ",
			0,			// tran_id
			cmd_type_to_cmd_id[CMD_SET_CALIBRATION],	// cmd_id
			sensor_type_to_sensor_id[sensor_type],	// sensor_id
			param->sub_cmd);		// sub_cmd

	if (param->sub_cmd == SUBCMD_CALIBRATION_SET) {
		// 30 parameters, horrible Orz...
		unsigned char* p;
		log_message(DEBUG, "Set calibration, sensor_type %d\n", sensor_type);

		if (param->sub_cmd != SUBCMD_CALIBRATION_SET) {
			log_message(DEBUG, "Set calibration with confused parameter,"
						"sub_cmd is %d\n",param->sub_cmd);

			return;
		}

		if (param->calibrated != SUBCMD_CALIBRATION_TRUE) {
			log_message(DEBUG, "Set calibration with confused parameter,"
						"calibrated is %d\n",param->calibrated);

			return;
		}

		if (sensor_type == SENSOR_CALIBRATION_COMP) {
			/* For compass_cal, the parameter is:
			 * minx, maxx, miny, maxy, minz, maxz
			 */
			p = (unsigned char*)&param->cal_param.compass.minx;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
				p[0], p[1], p[2], p[3]); // Min x

			p = (unsigned char*)&param->cal_param.compass.maxx;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
				p[0], p[1], p[2], p[3]); // Max x

			p = (unsigned char*)&param->cal_param.compass.miny;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
				p[0], p[1], p[2], p[3]); // Min y

			p = (unsigned char*)&param->cal_param.compass.maxy;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
				p[0], p[1], p[2], p[3]); // Max y

			p = (unsigned char*)&param->cal_param.compass.minz;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d ",
				p[0], p[1], p[2], p[3]); // Min z

			p = (unsigned char*)&param->cal_param.compass.maxz;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d %d %d",
				p[0], p[1], p[2], p[3]); // Max z
		} else if (sensor_type == SENSOR_CALIBRATION_GYRO) {
			/* For gyro_cal, the parameter is:
			 * x, y, z
			 */
			p = (unsigned char*)&param->cal_param.gyro.x;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d ",
				p[0], p[1]); // x

			p = (unsigned char*)&param->cal_param.gyro.y;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d ",
				p[0], p[1]); // y

			p = (unsigned char*)&param->cal_param.gyro.z;
			len += snprintf (cmdstring + len, MAX_STRING_SIZE - len, "%d %d",
				p[0], p[1]); // z

		}
	} else if (param->sub_cmd == SUBCMD_CALIBRATION_START) {
		log_message(DEBUG, "Calibration START, sensor_type %d\n", sensor_type);

	} else if (param->sub_cmd == SUBCMD_CALIBRATION_STOP) {
		log_message(DEBUG, "Calibration STOP, sensor_type %d\n", sensor_type);

	}

	log_message(DEBUG, "Send to control node :%s\n", cmdstring);
	ret = write(ctlfd, cmdstring, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);
}

static void load_calibration_from_file(psh_sensor_t sensor_type,
					struct cmd_calibration_param *param)
{
	FILE *conf;
	const char *file_name;

	memset(param, 0, sizeof(struct cmd_calibration_param));

	if (sensor_type == SENSOR_CALIBRATION_COMP)
		file_name = COMPASS_CALIBRATION_FILE;
	else if (sensor_type == SENSOR_CALIBRATION_GYRO)
		file_name = GYRO_CALIBRATION_FILE;
	else
		return;

	conf = fopen(file_name, "r");

	if (!conf) {
		/* calibration file doesn't exist, creat a new file */
		log_message(DEBUG, "Open file %s failed, create a new one!\n", file_name);
		conf = fopen(file_name, "w");
		if (conf == NULL) {
			log_message(CRITICAL, "load_calibration_from_file(): failed to open file \n");
			return;
		}
		fwrite(param, sizeof(struct cmd_calibration_param), 1, conf);
		fclose(conf);

		return;
	}

	fread(param, sizeof(struct cmd_calibration_param), 1, conf);
	fclose(conf);
}

static void store_calibration_to_file(psh_sensor_t sensor_type,
					struct cmd_calibration_param *param)
{
	FILE *conf;
	const char *file_name;

	if (sensor_type == SENSOR_CALIBRATION_COMP)
		file_name = COMPASS_CALIBRATION_FILE;
	else if (sensor_type == SENSOR_CALIBRATION_GYRO)
		file_name = GYRO_CALIBRATION_FILE;
	else
		return;

	conf = fopen(file_name, "w");

	if (!conf) {
		/* calibration file doesn't exist, creat a new file */
		log_message(DEBUG, "Can not Open file %s for writing\n", file_name);
		return;
	}

	fwrite(param, sizeof(struct cmd_calibration_param), 1, conf);
	fclose(conf);
}

static inline int check_calibration_status(psh_sensor_t sensor_type, int status)
{
	if (sensor_list[sensor_type].calibration_status & status)
		return 1;
	else
		return 0;
}

static int set_calibration_status(psh_sensor_t sensor_type, int status,
					struct cmd_calibration_param *param)
{
	if (sensor_type != SENSOR_CALIBRATION_COMP &&
		sensor_type != SENSOR_CALIBRATION_GYRO)
		return -1;

	if (status & CALIBRATION_NEED_STORE)
		sensor_list[sensor_type].calibration_status |=
							CALIBRATION_NEED_STORE;

	if (status & CALIBRATION_INIT) {
		struct cmd_calibration_param param_temp;

		/* If current status is calibrated, calibration init isn't needed*/
		if (sensor_list[sensor_type].calibration_status &
				CALIBRATION_DONE)
			return sensor_list[sensor_type].calibration_status;

		load_calibration_from_file(sensor_type, &param_temp);

		if (param_temp.sensor_type == sensor_type &&
			param_temp.calibrated == SUBCMD_CALIBRATION_TRUE) {
			/* Set calibration parameter to psh_fw */
			param_temp.sub_cmd = SUBCMD_CALIBRATION_SET;
			set_calibration(sensor_type, &param_temp);

			/* Clear 0~30bit, 31bit is for CALIBRATION_NEED_STORE,
			 * 31bit can coexist with other bits */
			sensor_list[sensor_type].calibration_status &=
							CALIBRATION_NEED_STORE;
			/* Then set the status bit */
			sensor_list[sensor_type].calibration_status |=
							CALIBRATION_DONE;
		} else {
			memset(&param_temp, 0, sizeof(struct cmd_calibration_param));
			/* clear the parameter file */
			store_calibration_to_file(sensor_type, &param_temp);
			/* If no calibration parameter, just set status */
			sensor_list[sensor_type].calibration_status &=
							CALIBRATION_NEED_STORE;
			sensor_list[sensor_type].calibration_status |=
							CALIBRATION_IN_PROGRESS;
		}
	} else if (status & CALIBRATION_RESET) {
		/* Start the calibration */
		if (param && param->sub_cmd == SUBCMD_CALIBRATION_START) {
			set_calibration(sensor_type, param);

			memset(param, 0, sizeof(struct cmd_calibration_param));
			/* clear the parameter file */
			store_calibration_to_file(sensor_type, param);

			sensor_list[sensor_type].calibration_status &=
							CALIBRATION_NEED_STORE;
			sensor_list[sensor_type].calibration_status |=
							CALIBRATION_IN_PROGRESS;
		}
	} else if (status & CALIBRATION_IN_PROGRESS) {
		/* CALIBRATION_IN_PROGRESS just a self-changed middle status,
		 * This status can not be set
		 * In this status, can only send SUBCMD_CALIBRATION_STOP
		 */
		if (param && param->sub_cmd == SUBCMD_CALIBRATION_STOP)
			set_calibration(sensor_type, param);
	} else if (status & CALIBRATION_DONE) {
		if (param && param->sensor_type == sensor_type &&
			param->calibrated == SUBCMD_CALIBRATION_TRUE &&
			param->sub_cmd == SUBCMD_CALIBRATION_SET) {
			/* Set calibration parameter to psh_fw */
			set_calibration(sensor_type, param);

			sensor_list[sensor_type].calibration_status &=
							CALIBRATION_NEED_STORE;
			sensor_list[sensor_type].calibration_status |=
							CALIBRATION_DONE;

			/* There is a chance to check if needs store param to file
			 * If so, then do it, and clear the CALIBRATION_NEED_STORE bit
			 */
			if (sensor_list[sensor_type].calibration_status &
				CALIBRATION_NEED_STORE) {
				store_calibration_to_file(sensor_type, param);

				sensor_list[sensor_type].calibration_status &=
							(~CALIBRATION_NEED_STORE);
			}
		}
	}

	return sensor_list[sensor_type].calibration_status;
}

static session_state_t* get_session_state_with_trans_id(
					unsigned char trans_id)
{
	session_state_t *p_session_state = sensor_list[SENSOR_EVENT].list;

	while (p_session_state != NULL) {
		if (p_session_state->trans_id == trans_id)
			return p_session_state;

		p_session_state = p_session_state->next;
	}

	return NULL;
}

#define MAX_UNSIGNED_CHAR 0xff
/* trans_id starts from 1 */
static unsigned char allocate_trans_id()
{
	/* rewind transaction ID */
	static unsigned char trans_id = 0;
	static int rewind = 0;

	trans_id++;

	if (rewind) {
		while ((get_session_state_with_trans_id(trans_id) != NULL) || (trans_id == 0))
		trans_id++;
	}

	if (trans_id == MAX_UNSIGNED_CHAR)
		rewind = 1;

	return trans_id;
}

static void handle_add_event(session_state_t *p_session_state, cmd_event* p_cmd)
{
	unsigned char trans_id;
	char cmd_string[MAX_STRING_SIZE];
	int len, ret;

	log_message(DEBUG, "[%s] Ready to handle command %d\n", __func__, p_cmd->cmd);

	/* allocate a tran_id, send cmd to PSH */
	struct cmd_event_param *evt_param = (struct cmd_event_param *)p_cmd->buf;
	int i, num = evt_param->num;

	trans_id = allocate_trans_id();

	p_session_state->trans_id = trans_id;

	/* struct ia_cmd { u8 tran_id; u8 cmd_id; u8 sensor_id; char param[] }; */
	len = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d ", trans_id, cmd_type_to_cmd_id[CMD_ADD_EVENT], 0);
	/* struct cmd_event_param {u8 num; u8 op; struct sub_event evts[] }; */
	len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d ", num, evt_param->relation);
	/* struct sub_event {u8 sensor_id; u8 chan_id; opt_id; u32 param1; u32 param2; }; */
	for (i = 0; i < num; i++) {
		struct sub_event *sub_event = &(evt_param->evts[i]);
		unsigned char* p;

		len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d %d ", sensor_type_to_sensor_id[sub_event->sensor_id], sub_event->chan_id, sub_event->opt_id);
		p = (unsigned char*)&sub_event->param1;
		len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d %d %d ", p[0], p[1], p[2], p[3]);
		p = (unsigned char*)&sub_event->param2;
		len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d %d %d %d ", p[0], p[1], p[2], p[3]);
	}

	ret = write(ctlfd, cmd_string, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);

	return;
}

static void handle_clear_event(session_state_t *p_session_state)
{
	char cmd_string[MAX_STRING_SIZE];
	int len, ret;

	if (p_session_state->event_id == 0)
		return;

	/* struct ia_cmd { u8 tran_id; u8 cmd_id; u8 sensor_id; char param[] }; */
	len = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d ", 0, cmd_type_to_cmd_id[CMD_CLEAR_EVENT], 0);
	/* struct clear_evt_param {u8 evt_id}; */
	len += snprintf(cmd_string + len, MAX_STRING_SIZE - len, "%d ", p_session_state->event_id);

	p_session_state->event_id = 0;
	p_session_state->trans_id = 0;

	ret = write(ctlfd, cmd_string, len);
	if (ret != len)
		log_message(DEBUG, "[%s]Error in sending cmd:%d\n", __func__, ret);

	return;
}

static int recalculate_data_rate(psh_sensor_t sensor_type, int data_rate)
{
	short freq_max = sensor_list[sensor_type].freq_max;
	short i;

	if (freq_max == -1)
		return -1;

	if (freq_max == 0)
		return 0;

	if ((freq_max % data_rate) == 0)
		return data_rate;

	for (i = data_rate; i > 0; i--) {
		if ((freq_max % i) == 0)
			break;
	}

	return i;
}

/* 1 arbiter
   2 send cmd to control node under sysfs.
     cmd format is <TRANID><CMDID><SENSORID><RATE><BUFFER DELAY>
     CMDID refer to psh_fw/include/cmd_engine.h;
     SENSORID refer to psh_fw/sensors/include/sensor_def.h */
static ret_t handle_cmd(int fd, cmd_event* p_cmd, int parameter, int parameter1,
						int parameter2,	int *reply_now)
{
	psh_sensor_t sensor_type;
	session_state_t *p_session_state;
	int data_rate_calculated, size;
	int ret = get_sensor_type_session_state_with_fd(fd, &sensor_type,
							&p_session_state);
	cmd_t cmd = p_cmd->cmd;

	*reply_now = 1;

	if (ret != 0)
		return ERR_SESSION_NOT_EXIST;

	log_message(DEBUG, "[%s] Ready to handle command %d\n", __func__, cmd);

	if (cmd == CMD_START_STREAMING) {
		data_rate_calculated = recalculate_data_rate(sensor_type, parameter);
		start_streaming(sensor_type, p_session_state,
					data_rate_calculated, parameter1, parameter2);
	} else if (cmd == CMD_STOP_STREAMING) {
		stop_streaming(sensor_type, p_session_state);
	} else if (cmd == CMD_GET_SINGLE) {
		get_single(sensor_type, p_session_state);
		*reply_now = 0;
	} else if (cmd == CMD_GET_CALIBRATION) {
		get_calibration(sensor_type, p_session_state);
		*reply_now = 0;
	} else if (cmd == CMD_SET_CALIBRATION) {
		char *p = (char*)p_cmd;
		struct cmd_calibration_param *cal_param;

		if (!parameter) {
			log_message(DEBUG, "ERR: This CMD need take parameter\n");
			return ERR_CMD_NOT_SUPPORT;
		}

		cal_param = (struct cmd_calibration_param*)(p + sizeof(cmd_event));

		if (cal_param->sub_cmd == SUBCMD_CALIBRATION_SET)
			set_calibration_status(sensor_type,
						CALIBRATION_DONE | CALIBRATION_NEED_STORE,
						cal_param);
		else if (cal_param->sub_cmd == SUBCMD_CALIBRATION_START)
			set_calibration_status(sensor_type,
						CALIBRATION_RESET | CALIBRATION_NEED_STORE,
						cal_param);
		else if (cal_param->sub_cmd == SUBCMD_CALIBRATION_STOP)
			set_calibration_status(sensor_type,
						CALIBRATION_IN_PROGRESS,
						cal_param);
	} else if (cmd == CMD_ADD_EVENT) {
		handle_add_event(p_session_state, p_cmd);
		*reply_now = 0;
	} else if (cmd == CMD_CLEAR_EVENT) {
		if (p_session_state->event_id == p_cmd->parameter)
			handle_clear_event(p_session_state);
		else
			return ERR_SESSION_NOT_EXIST;
	} else if (cmd == CMD_SET_PROPERTY) {
		send_set_property(sensor_type, p_cmd->parameter, p_cmd->parameter1);
	}

	return SUCCESS;
}

static void handle_message(int fd, char *message)
{
	int event_type = *((int *) message);

	log_message(DEBUG, "handle_message(): fd is %d, event_type is %d\n",
							fd, event_type);

	if (event_type == EVENT_HELLO_WITH_SENSOR_TYPE) {
		hello_with_sensor_type_ack_event hello_with_sensor_type_ack;
		session_state_t *p_session_state, **next;
		hello_with_sensor_type_event *p_hello_with_sensor_type =
					(hello_with_sensor_type_event *)message;
		psh_sensor_t sensor_type =
					p_hello_with_sensor_type->sensor_type;
		session_id_t session_id = allocate_session_id();

		/* allocate session ID and return it to client;
		   allocate session_state and add it to sensor_list */
		hello_with_sensor_type_ack.event_type =
					EVENT_HELLO_WITH_SENSOR_TYPE_ACK;
		hello_with_sensor_type_ack.session_id = session_id;
		send(fd, &hello_with_sensor_type_ack,
			sizeof(hello_with_sensor_type_ack), 0);

		p_session_state = malloc(sizeof(session_state_t));
		if (p_session_state == NULL) {
			log_message(CRITICAL, "handle_message(): malloc failed \n");
			return;
		}
		memset(p_session_state, 0, sizeof(session_state_t));
		p_session_state->datafd = fd;
		p_session_state->session_id = session_id;

		if ((sensor_type == SENSOR_COMP || sensor_type == SENSOR_GYRO) &&
			sensor_list[sensor_type].list == NULL) {
			/* In this case, this sensor is opened firstly,
			 * So calibration init is needed
			 */
			psh_sensor_t cal_sensor = (sensor_type == SENSOR_COMP) ?
							SENSOR_CALIBRATION_COMP :
							SENSOR_CALIBRATION_GYRO;

			set_calibration_status(cal_sensor, CALIBRATION_INIT, NULL);
		}

		p_session_state->next = sensor_list[sensor_type].list;
		sensor_list[sensor_type].list = p_session_state;

	} else if (event_type == EVENT_HELLO_WITH_SESSION_ID) {
		hello_with_session_id_ack_event hello_with_session_id_ack;
		hello_with_session_id_event *p_hello_with_session_id =
					(hello_with_session_id_event *)message;
		session_id_t session_id = p_hello_with_session_id->session_id;
		session_state_t *p_session_state =
				get_session_state_with_session_id(session_id);

		p_session_state->ctlfd = fd;

		hello_with_session_id_ack.event_type =
					EVENT_HELLO_WITH_SESSION_ID_ACK;
		hello_with_session_id_ack.ret = SUCCESS;
		send(fd, &hello_with_session_id_ack,
				sizeof(hello_with_session_id_ack), 0);

	} else if (event_type == EVENT_CMD) {
		ret_t ret;
		int reply_now;
		cmd_event *p_cmd = (cmd_event *)message;
		cmd_ack_event cmd_ack;

		ret = handle_cmd(fd, p_cmd, p_cmd->parameter,
					p_cmd->parameter1, p_cmd->parameter2, &reply_now);

		if (reply_now == 0)
			return;

		cmd_ack.event_type = EVENT_CMD_ACK;
		cmd_ack.ret = ret;
		send(fd, &cmd_ack, sizeof(cmd_ack), 0);
	} else {
		/* TODO: unknown message and drop it */
	}
}

/* 1 (re)open control node and send 'reset' cmd
   2 (re)open data node
   3 (re)open Unix socket */
static void reset_sensorhub()
{
	struct sockaddr_un serv_addr;
	char node_path[MAX_STRING_SIZE];

	if (ctlfd != -1) {
		close(ctlfd);
		ctlfd = -1;
	}

	if (datafd != -1) {
		close(datafd);
		datafd = -1;
	}

	if (sockfd != -1) {
		close(sockfd);
		sockfd = -1;
	}

	if (datasizefd != -1) {
		close(datasizefd);
		datasizefd = -1;
	}

	if (wakefd != -1) {
		close(wakefd);
		wakefd = -1;
	}

	if (sleepfd != -1) {
		close(sleepfd);
		sleepfd = -1;
	}

	/* detect the device node */
	for (node_number = 0; node_number < MAX_DEV_NODE_NUM; node_number++) {
		struct stat f_stat;
		int fd;
		char magic_string[32];

		snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/hwmon%d/device/modalias", node_number);
		if (stat(node_path, &f_stat) != 0)
			continue;

		fd = open(node_path, O_RDONLY);
		if (fd == -1)
			continue;

		read(fd, magic_string, 32);
		magic_string[31] = '\0';
		close(fd);

		log_message(DEBUG, "magic_string is %s \n", magic_string);

		if ((strstr(magic_string, "11A4") != NULL) || (strstr(magic_string, "psh") != NULL))
			break;
	}

	log_message(DEBUG, "node_number is %d \n", node_number);

	if (node_number == MAX_DEV_NODE_NUM) {
		log_message(CRITICAL, "can't find device node \n");
		exit(EXIT_FAILURE);
	}

	/* open control node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/hwmon%d/device/control", node_number);
	ctlfd = open(node_path, O_WRONLY);
	if (ctlfd == -1) {
		log_message(CRITICAL, "open %s failed, errno is %d\n",
				node_path, errno);
		exit(EXIT_FAILURE);
	}

	/* TODO: send 'reset' cmd */

	/* open data node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/hwmon%d/device/data", node_number);
	datafd = open(node_path, O_RDONLY);
	if (datafd == -1) {
		log_message(CRITICAL, "open %s failed, errno is %d\n",
				node_path, errno);
		exit(EXIT_FAILURE);
	}

	/* open data_size node */
	snprintf(node_path, MAX_STRING_SIZE, "/sys/class/hwmon/hwmon%d/device/data_size", node_number);
	datasizefd = open(node_path, O_RDONLY);
	if (datasizefd == -1) {
		log_message(CRITICAL, "open %s failed, errno is %d\n",
				node_path, errno);
		exit(EXIT_FAILURE);
	}

	/* create sensorhubd Unix socket */
	sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_LOCAL;
	snprintf(serv_addr.sun_path, UNIX_PATH_MAX, UNIX_SOCKET_PATH);

	unlink(UNIX_SOCKET_PATH);
	int ret = bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr));
	if (ret != 0) {
		log_message(CRITICAL, "bind failed, ret is %d\n", ret);
		exit(EXIT_FAILURE);
	}

	listen(sockfd, MAX_Q_LENGTH);

	/* open wakeup and sleep sysfs node */
	wakefd = open(WAKE_NODE, O_RDONLY, 0);
	if (wakefd == -1) {
		log_message(CRITICAL, "open %s failed, errno is %d\n",
				WAKE_NODE, errno);
		exit(EXIT_FAILURE);
	}

	sleepfd = open(SLEEP_NODE, O_RDONLY, 0);
	if (sleepfd == -1) {
		log_message(CRITICAL, "open %s failed, errno is %d\n",
			SLEEP_NODE, errno);
		exit(EXIT_FAILURE);
	}
}

/* 1 release resources in sensor_list
   2 clear sensor_list to 0 */
static void reset_client_sessions()
{
	/* TODO: release resources in sensor_list */

	memset(sensor_list, 0, sizeof(sensor_list));
}

/* 1 read data from data node
   2 analyze the message format and extract the sensor data
   3 send to clients according to arbiter result */

struct cmd_resp {
	unsigned char tran_id;
	unsigned char cmd_type;
	unsigned char sensor_id;
	unsigned short data_len;
	char buf[0];
} __attribute__ ((packed));

static void write_data(session_state_t *p_session_state, void *data, int size)
{
	char buf[1];
	int ret;

	ret = recv(p_session_state->datafd, buf, 1, MSG_DONTWAIT);
	if (ret == 0)
		p_session_state->datafd_invalid = 1;

	if (p_session_state->datafd_invalid == 1)
		return;

	ret = write(p_session_state->datafd, data, size);
	if (ret < 0)
		p_session_state->datafd_invalid = 1;
}

static void send_data_to_clients(psh_sensor_t sensor_type, void *data,
						int size, int unit_size)
{
	session_state_t *p_session_state = sensor_list[sensor_type].list;
	int count = size / unit_size;
	char buf[MAX_MESSAGE_LENGTH];

	/* Use slide window mechanism to send data to target client,
	   buffer_delay is gauranteed, data count is not gauranteed.
	   calculate count of incoming data, send to clients by count  */
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		int step, index = 0, i = 0;

		if ((p_session_state->state != ACTIVE) && (p_session_state->state != ALWAYS_ON))
			continue;

		/* no arbiter needed for SENSOR_ALS,
			SENSOR_ACTIVITY, SENSOR_GS, GESTURE_FLICK */
		if ((sensor_type == SENSOR_ALS)
				|| (sensor_type == SENSOR_PROXIMITY)
				|| (sensor_type == SENSOR_ACTIVITY)
				|| (sensor_type == SENSOR_GS)
				|| (sensor_type == SENSOR_GESTURE_FLICK)
				|| (sensor_type == SENSOR_TC)
				|| (sensor_type == SENSOR_PEDOMETER)) {
//			write(p_session_state->datafd, data, size);
			send(p_session_state->datafd, data, size, MSG_NOSIGNAL);
			continue;
		}
		/* special case, so fast processing without data copy */
		if (sensor_list[sensor_type].data_rate
				== p_session_state->data_rate) {
			int ret;
			ret = send(p_session_state->datafd, data, size, MSG_NOSIGNAL|MSG_DONTWAIT);
//			write_data(p_session_state, data, size);
			continue;
		}

		step = sensor_list[sensor_type].data_rate
				/ p_session_state->data_rate;
		if (p_session_state->tail != 0)
			index = step - (p_session_state->tail % step);

		while (index < count) {
			memcpy(buf + i * unit_size,
				(char *)data + index * unit_size, unit_size);
			i++;
			index += step;
		}
//		log_message(DEBUG, "step is %d, i is %d, count is %d \n",
//							step, i, count);
		p_session_state->tail = (p_session_state->tail + count) % step;

//		write(p_session_state->datafd, buf, unit_size * i);
		send(p_session_state->datafd, buf, unit_size * i, MSG_NOSIGNAL);
	}
}

static void dispatch_get_single(struct cmd_resp *p_cmd_resp)
{
	int sensor_type = -1;
	session_state_t *p_session_state;
	cmd_ack_event *p_cmd_ack;

	if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ACCELEROMETER]) {
		sensor_type = SENSOR_ACCELEROMETER;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GYRO]) {
		sensor_type = SENSOR_GYRO;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_COMP]) {
		sensor_type = SENSOR_COMP;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_TC]) {
		sensor_type = SENSOR_TC;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_BARO]) {
		sensor_type = SENSOR_BARO;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ALS]) {
		sensor_type = SENSOR_ALS;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ACTIVITY]) {
		sensor_type = SENSOR_ACTIVITY;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GS]) {
		sensor_type = SENSOR_GS;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_PROXIMITY]) {
		sensor_type = SENSOR_PROXIMITY;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GESTURE_FLICK]) {
		sensor_type = SENSOR_GESTURE_FLICK;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ROTATION_VECTOR]) {
		sensor_type = SENSOR_ROTATION_VECTOR;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GRAVITY]) {
		sensor_type = SENSOR_GRAVITY;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_LINEAR_ACCEL]) {
		sensor_type = SENSOR_LINEAR_ACCEL;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ORIENTATION]) {
		sensor_type = SENSOR_ORIENTATION;
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_MAG_HEADING]) {
		sensor_type = SENSOR_MAG_HEADING;
	} else {
		log_message(CRITICAL, "unkown sensor id from psh fw %d \n", p_cmd_resp->sensor_id);
		return;
	}

	p_session_state = sensor_list[sensor_type].list;
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		if (p_session_state->get_single == 0)
			continue;
		// TODO Compass data breaks the uniform handle of get_single ...
		// FIXME hard copy , too bad ...
		if (sensor_type == SENSOR_COMP || sensor_type == SENSOR_GYRO) {
			psh_sensor_t cal_type = (sensor_type == SENSOR_COMP) ?
							SENSOR_CALIBRATION_COMP :
							SENSOR_CALIBRATION_GYRO;

			p_cmd_ack = malloc(sizeof(cmd_ack_event) + p_cmd_resp->data_len + 2);
			if (p_cmd_ack == NULL) {
				log_message(CRITICAL, "dispatch_get_single(): malloc failed \n");
				goto fail;
			}
			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = p_cmd_resp->data_len + 2;
			{
				short *p = (short*)(p_cmd_ack->buf + p_cmd_resp->data_len);
				*p = check_calibration_status(cal_type, CALIBRATION_DONE);
			}
			memcpy(p_cmd_ack->buf, p_cmd_resp->buf, p_cmd_resp->data_len + 2);

			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
						+ p_cmd_resp->data_len + 2, 0);
		} else {
			p_cmd_ack = malloc(sizeof(cmd_ack_event)
						+ p_cmd_resp->data_len);
			if (p_cmd_ack == NULL) {
				log_message(CRITICAL, "dispatch_get_single(): malloc failed \n");
				goto fail;
			}
			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = p_cmd_resp->data_len;
			memcpy(p_cmd_ack->buf, p_cmd_resp->buf, p_cmd_resp->data_len);

			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
						+ p_cmd_resp->data_len, 0);

		}
			free(p_cmd_ack);
fail:
			p_session_state->get_single = 0;
	}
}

static void dispatch_streaming(struct cmd_resp *p_cmd_resp)
{
	if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ACCELEROMETER]) {
		struct accel_data *p_accel_data
				= (struct accel_data *)p_cmd_resp->buf;
//                      log_message(DEBUG, "accel data, x, y, z is: "
//                                      "%d, %d, %d \n", p_accel_data->x,
//                                      p_accel_data->y, p_accel_data->z);

                        /* TODO: split data into MAX_MESSAGE_LENGTH */
		send_data_to_clients(SENSOR_ACCELEROMETER, p_accel_data,
					p_cmd_resp->data_len,
					sizeof(struct accel_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GYRO]) {
		struct gyro_raw_data *p_gyro_raw_data;
		struct gyro_raw_data *p_fake
			= (struct gyro_raw_data *)p_cmd_resp->buf;
		int i, len;
		len = p_cmd_resp->data_len / (sizeof(struct gyro_raw_data) - 2);
		p_gyro_raw_data = (struct gyro_raw_data *)malloc(len * sizeof(struct gyro_raw_data));
		if (p_gyro_raw_data == NULL)
			goto fail;
		for (i = 0; i < len; ++i) {
			p_gyro_raw_data[i].x = p_fake[i].x;
			p_gyro_raw_data[i].y = p_fake[i].y;
			p_gyro_raw_data[i].z = p_fake[i].z;
			p_gyro_raw_data[i].accuracy =
				check_calibration_status(SENSOR_CALIBRATION_GYRO,
							CALIBRATION_DONE);
		}
		send_data_to_clients(SENSOR_GYRO, p_gyro_raw_data,
					len*sizeof(struct gyro_raw_data),
					sizeof(struct gyro_raw_data));
		free(p_gyro_raw_data);
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_COMP]) {
		struct compass_raw_data *p_compass_raw_data;
		struct compass_raw_data *p_fake = (struct compass_raw_data *)p_cmd_resp->buf;
		int i, len;
		len = p_cmd_resp->data_len / (sizeof(struct compass_raw_data) - 2);
		p_compass_raw_data = malloc(len * sizeof(struct compass_raw_data));
		if(p_compass_raw_data == NULL)
			goto fail;
		for (i = 0; i < len; ++i){
			p_compass_raw_data[i].x = p_fake[i].x;
			p_compass_raw_data[i].y = p_fake[i].y;
			p_compass_raw_data[i].z = p_fake[i].z;
			p_compass_raw_data[i].accuracy =
				check_calibration_status(SENSOR_CALIBRATION_COMP,
							CALIBRATION_DONE);
		}
		//TODO What to do?
		send_data_to_clients(SENSOR_COMP, p_compass_raw_data,
					len*sizeof(struct compass_raw_data),
					sizeof(struct compass_raw_data)); // Unggg ...
		free(p_compass_raw_data);
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_TC]) {
		struct tc_data *p_tc_data = (struct tc_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_TC, p_tc_data, p_cmd_resp->data_len,
							sizeof(struct tc_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_BARO]) {
		struct baro_raw_data *p_baro_raw_data
			= (struct baro_raw_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_BARO, p_baro_raw_data,
						p_cmd_resp->data_len,
						sizeof(struct baro_raw_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ALS]) {
		struct als_raw_data *p_als_raw_data
			 = (struct als_raw_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_ALS, p_als_raw_data,
						p_cmd_resp->data_len,
						sizeof(struct als_raw_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ACTIVITY]) {
		struct phy_activity_data *p_phy_activity_data
			= (struct phy_activity_data *)p_cmd_resp->buf;
		int us = p_phy_activity_data->len *
			sizeof(p_phy_activity_data->values[0]) +
			sizeof(p_phy_activity_data->len);
		send_data_to_clients(SENSOR_ACTIVITY, p_phy_activity_data,
					p_cmd_resp->data_len,
					us);
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GS]) {
		struct gs_data *p_gs_data
			= (struct gs_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_GS, p_gs_data, p_cmd_resp->data_len,
					sizeof(struct gs_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_PROXIMITY]) {
		struct ps_phy_data *p_ps_phy_data
			= (struct ps_phy_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_PROXIMITY, p_ps_phy_data,
					p_cmd_resp->data_len,
					sizeof(struct ps_phy_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GESTURE_FLICK]) {
		struct gesture_flick_data *p_gesture_flick_data
			= (struct gesture_flick_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_GESTURE_FLICK, p_gesture_flick_data,
					p_cmd_resp->data_len,
					sizeof(struct gesture_flick_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ROTATION_VECTOR]) {
		struct rotation_vector_data *p_rotation_vector_data
			= (struct rotation_vector_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_ROTATION_VECTOR,
					p_rotation_vector_data,
					p_cmd_resp->data_len,
					sizeof(struct rotation_vector_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_GRAVITY]) {
		struct gravity_data *p_gravity_data
			= (struct gravity_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_GRAVITY, p_gravity_data,
					p_cmd_resp->data_len,
					sizeof(struct gravity_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_LINEAR_ACCEL]) {
		struct linear_accel_data *p_linear_accel_data
			= (struct linear_accel_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_LINEAR_ACCEL, p_linear_accel_data,
					p_cmd_resp->data_len,
					sizeof(struct linear_accel_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_ORIENTATION]) {
		struct orientation_data *p_orientation_data
			= (struct orientation_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_ORIENTATION, p_orientation_data,
					p_cmd_resp->data_len,
					sizeof(struct orientation_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_9DOF]) {
		struct ndof_data *p_ndof_data
			= (struct ndof_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_9DOF, p_ndof_data,
					p_cmd_resp->data_len,
					sizeof(struct ndof_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_PEDOMETER]) {
		struct pedometer_data *p_pedometer_data
			= (struct pedometer_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_PEDOMETER, p_pedometer_data,
					p_cmd_resp->data_len,
					sizeof(struct pedometer_data));
	} else if (p_cmd_resp->sensor_id
			== sensor_type_to_sensor_id[SENSOR_MAG_HEADING]) {
		struct mag_heading_data *p_mag_heading_data
			= (struct mag_heading_data *)p_cmd_resp->buf;
		send_data_to_clients(SENSOR_MAG_HEADING, p_mag_heading_data,
					p_cmd_resp->data_len,
					sizeof(struct mag_heading_data));
	}

	return;
fail:
	log_message(CRITICAL, "failed to allocate memory \n");
}

static void debug_data_rate(struct cmd_resp *p_cmd_resp)
{
	static int restart = 1;
	static int count[SENSOR_MAX];
	static struct timeval tv_start, tv_current;

	if (restart) {
		restart = 0;
		gettimeofday(&tv_start, NULL);
		memset(&count[0], 0, sizeof(count));
	}

	if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ACCELEROMETER]) {
		count[SENSOR_ACCELEROMETER] += p_cmd_resp->data_len / sizeof(struct accel_data);
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_GYRO]) {
		count[SENSOR_GYRO] += p_cmd_resp->data_len / (sizeof(struct gyro_raw_data) - 2);
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_COMP]) {
		count[SENSOR_COMP] += p_cmd_resp->data_len / (sizeof(struct compass_raw_data) - 2);
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_TC]) {
		count[SENSOR_TC] += p_cmd_resp->data_len / (sizeof(struct tc_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_BARO]) {
		count[SENSOR_BARO] += p_cmd_resp->data_len / (sizeof(struct baro_raw_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ALS]) {
		count[SENSOR_ALS] += p_cmd_resp->data_len / (sizeof(struct als_raw_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ACTIVITY]) {
		struct phy_activity_data *p_phy_activity_data
			= (struct phy_activity_data *)p_cmd_resp->buf;
		int us = p_phy_activity_data->len *
			sizeof(p_phy_activity_data->values[0]) +
			sizeof(p_phy_activity_data->len);

		count[SENSOR_ACTIVITY] += p_cmd_resp->data_len / us;
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_GS]) {
		count[SENSOR_GS] += p_cmd_resp->data_len / (sizeof(struct gs_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_PROXIMITY]) {
		count[SENSOR_PROXIMITY] += p_cmd_resp->data_len / (sizeof(struct ps_phy_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_GESTURE_FLICK]) {
		count[SENSOR_GESTURE_FLICK] += p_cmd_resp->data_len / (sizeof(struct gesture_flick_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ROTATION_VECTOR]) {
		count[SENSOR_ROTATION_VECTOR] += p_cmd_resp->data_len / (sizeof(struct rotation_vector_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_GRAVITY]) {
		count[SENSOR_GRAVITY] += p_cmd_resp->data_len / (sizeof(struct gravity_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_LINEAR_ACCEL]) {
		count[SENSOR_LINEAR_ACCEL] += p_cmd_resp->data_len / (sizeof(struct linear_accel_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_ORIENTATION]) {
		count[SENSOR_ORIENTATION] += p_cmd_resp->data_len / (sizeof(struct orientation_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_9DOF]) {
		count[SENSOR_9DOF] += p_cmd_resp->data_len / (sizeof(struct ndof_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_PEDOMETER]) {
		count[SENSOR_PEDOMETER] += p_cmd_resp->data_len / (sizeof(struct pedometer_data));
	} else if (p_cmd_resp->sensor_id
		== sensor_type_to_sensor_id[SENSOR_MAG_HEADING]) {
		count[SENSOR_MAG_HEADING] += p_cmd_resp->data_len / (sizeof(struct mag_heading_data));
	}

	gettimeofday(&tv_current, NULL);

	int interval = tv_current.tv_sec - tv_start.tv_sec;
	log_message(DEBUG, "interval is %d\n", interval);
	if ( interval >= 5) {
		// report data rate
		log_message(DEBUG, "ACCEL:    %d Hz \n"
				"GYRO:     %d Hz \n"
				"COMP:     %d Hz \n"
				"TC:       %d Hz \n"
				"BARO:     %d Hz \n"
				"ALS:      %d Hz \n"
				"ACTIVITY: %d Hz \n"
				"GS:       %d Hz \n"
				"PROXI:    %d Hz \n"
				"G_FLICK:  %d Hz \n"
				"R_VECT:   %d Hz \n"
				"GRAVITY:  %d Hz \n"
				"L_ACCEL:  %d Hz \n"
				"ORIENT:   %d Hz \n"
				"9DOF:     %d Hz \n"
				"PEDOMET:  %d Hz \n"
				"MAG_H:    %d Hz \n",
				count[SENSOR_ACCELEROMETER]/interval,
				count[SENSOR_GYRO]/interval,
				count[SENSOR_COMP]/interval,
				count[SENSOR_TC]/interval,
				count[SENSOR_BARO]/interval,
				count[SENSOR_ALS]/interval,
				count[SENSOR_ACTIVITY]/interval,
				count[SENSOR_GS]/interval,
				count[SENSOR_PROXIMITY]/interval,
				count[SENSOR_GESTURE_FLICK]/interval,
				count[SENSOR_ROTATION_VECTOR]/interval,
				count[SENSOR_GRAVITY]/interval,
				count[SENSOR_LINEAR_ACCEL]/interval,
				count[SENSOR_ORIENTATION]/interval,
				count[SENSOR_9DOF]/interval,
				count[SENSOR_PEDOMETER]/interval,
				count[SENSOR_MAG_HEADING]/interval);

		restart = 1;
	}
}

static void handle_calibration(struct cmd_calibration_param * param){
	session_state_t *p_session_state;
	cmd_ack_event *p_cmd_ack;
	psh_sensor_t sensor_type = param->sensor_type;

	p_session_state = sensor_list[sensor_type].list;

	log_message(DEBUG, "Subcmd:%d.  Calibrated? %d\n", param->sub_cmd, param->calibrated);

	for (; p_session_state != NULL;
			p_session_state = p_session_state->next){
		struct cmd_calibration_param * p_cal_info;

		if (p_session_state->get_calibration == 0)
			continue;

		p_cmd_ack = malloc(sizeof(cmd_ack_event) + sizeof(struct cmd_calibration_param));
		if (p_cmd_ack == NULL) {
			log_message(CRITICAL, "failed to allocate memory \n");
			goto fail;
		}
		p_cmd_ack->event_type = EVENT_CMD_ACK;
		p_cmd_ack->ret = SUCCESS;
		p_cmd_ack->buf_len = sizeof(struct cmd_calibration_param);

		p_cal_info = (struct cmd_calibration_param *)p_cmd_ack->buf;
		*p_cal_info = *param;

		send(p_session_state->ctlfd, p_cmd_ack,
				sizeof(cmd_ack_event) + sizeof(struct cmd_calibration_param), 0);

		free(p_cmd_ack);
fail:
		p_session_state->get_calibration = 0;
	}

	if (param->calibrated) {
		param->sub_cmd = SUBCMD_CALIBRATION_SET;
		set_calibration_status(sensor_type, CALIBRATION_DONE, param);
	} else {
		log_message(DEBUG, "Get calibration message, but calibrated not done.\n");
	}
}

/* use trans_id to match client */
static void handle_add_event_resp(struct cmd_resp *p_cmd_resp)
{
	unsigned char trans_id = p_cmd_resp->tran_id;
	session_state_t *p_session_state = sensor_list[SENSOR_EVENT].list;
	cmd_ack_event *p_cmd_ack;

	/* trans_id starts from 1, 0 means no trans_id */
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		if (trans_id == p_session_state->trans_id) {
			p_cmd_ack = malloc(sizeof(cmd_ack_event)
					+ p_cmd_resp->data_len);
			if (p_cmd_ack == NULL) {
				log_message(CRITICAL, "handle_add_event_resp(): malloc failed \n");
				return;
			}

			p_cmd_ack->event_type = EVENT_CMD_ACK;
			p_cmd_ack->ret = SUCCESS;
			p_cmd_ack->buf_len = p_cmd_resp->data_len;
			memcpy(p_cmd_ack->buf, p_cmd_resp->buf, p_cmd_resp->data_len);
			send(p_session_state->ctlfd, p_cmd_ack, sizeof(cmd_ack_event)
				+ p_cmd_resp->data_len, 0);

			log_message(DEBUG, "event id is %d \n", p_cmd_resp->buf[0]);
			free(p_cmd_ack);

			p_session_state->event_id = p_cmd_resp->buf[0];
//			p_session_state->trans_id = 0;
			break;
		}
	}
}

/* use event_id to match client */
static void handle_clear_event_resp(struct cmd_resp *p_cmd_resp)
{

}

/* use event_id to match client */
static void dispatch_event(struct cmd_resp *p_cmd_resp)
{
	unsigned char event_id;
	session_state_t *p_session_state = sensor_list[SENSOR_EVENT].list;

	if (p_cmd_resp->data_len != 0) {
		event_id = p_cmd_resp->buf[0];
	} else
		return;

	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		if (event_id == p_session_state->event_id) {
//			write(p_session_state->datafd, p_cmd_resp->buf, p_cmd_resp->data_len);
			send(p_session_state->datafd, p_cmd_resp->buf, p_cmd_resp->data_len, MSG_NOSIGNAL);
			break;
		}
	}
}

static void dispatch_data()
{
	static char *buf = NULL;
	char *p;
	int ret, data_size;
	struct cmd_resp *p_cmd_resp;
	struct timeval tv, tv1;

	if (buf == NULL)
		buf = (char *)malloc(128 * 1024);

	if (buf == NULL) {
		log_message(CRITICAL, "dispatch_data(): malloc failed \n");
		return;
	}

	gettimeofday(&tv, NULL);

	log_message(DEBUG, "Got a packet, timestamp is %d, %d \n",
					tv.tv_sec, tv.tv_usec);

	/* read data_size node */
	lseek(datasizefd, 0, SEEK_SET);
	ret = read(datasizefd, buf, 128 * 1024);
	if (ret <= 0)
		return;

//	sscanf(buf, "%d", &data_size);

	ret = read(datafd, buf, 128 * 1024);
	if (ret <= 0)
		return;

//	log_message(DEBUG, "data_size is: %d, read() return value is %d \n",
//							data_size, ret);

	p_cmd_resp = (struct cmd_resp *) buf;
	p = buf;
	while (ret > 0) {
		log_message(DEBUG, "tran_id: %d, cmd_type: %d, sensor_id: %d, "
				"data_len: %d \n", p_cmd_resp->tran_id,
				p_cmd_resp->cmd_type, p_cmd_resp->sensor_id,
				p_cmd_resp->data_len);

		if (p_cmd_resp->cmd_type == RESP_GET_SINGLE)
			dispatch_get_single(p_cmd_resp);
		else if (p_cmd_resp->cmd_type == RESP_STREAMING) {
			dispatch_streaming(p_cmd_resp);
			if (enable_debug_data_rate)
				debug_data_rate(p_cmd_resp);
			}
		else if (p_cmd_resp->cmd_type == RESP_COMP_CAL_RESULT){
			struct cmd_calibration_param param;
			struct resp_compasscal *p = (struct resp_compasscal*)p_cmd_resp->buf;

			if (p_cmd_resp->data_len != sizeof(struct resp_compasscal))
				log_message(DEBUG, "Get a calibration_get response with wrong data_len:%d\n",
											p_cmd_resp->data_len);

			param.sensor_type = SENSOR_CALIBRATION_COMP;
			param.sub_cmd = SUBCMD_CALIBRATION_GET;
			param.calibrated = (p->calib_result == CALIB_RESULT_NONE) ?
								SUBCMD_CALIBRATION_FALSE :
								SUBCMD_CALIBRATION_TRUE;
			param.cal_param.compass = p->info;
			handle_calibration(&param);
		} else if (p_cmd_resp->cmd_type == RESP_GYRO_CAL_RESULT){
			struct cmd_calibration_param param;
			struct resp_gyrocal *p = (struct resp_gyrocal*)p_cmd_resp->buf;

			if (p_cmd_resp->data_len != sizeof(struct resp_gyrocal))
				log_message(DEBUG, "Get a calibration_get response with wrong data_len:%d\n",
											p_cmd_resp->data_len);

			param.sensor_type = SENSOR_CALIBRATION_GYRO;
			param.sub_cmd = SUBCMD_CALIBRATION_GET;
			param.calibrated = (p->calib_result == CALIB_RESULT_NONE) ?
								SUBCMD_CALIBRATION_FALSE :
								SUBCMD_CALIBRATION_TRUE;
			param.cal_param.gyro = p->info;
			handle_calibration(&param);
		} else if (p_cmd_resp->cmd_type == RESP_ADD_EVENT) {
			/* TODO: record event id, match client with trans id, send return value to client */
			handle_add_event_resp(p_cmd_resp);
		} else if (p_cmd_resp->cmd_type == RESP_CLEAR_EVENT) {
			handle_clear_event_resp(p_cmd_resp);
		} else if (p_cmd_resp->cmd_type == RESP_EVENT) {
			/* dispatch event */
			dispatch_event(p_cmd_resp);
		} else
			log_message(DEBUG,
			    "%d: not support this response currently\n", p_cmd_resp->cmd_type);

		ret = ret - (sizeof(struct cmd_resp) + p_cmd_resp->data_len);
//		log_message(DEBUG, "remain data len is %d\n", ret);

		p = p + sizeof(struct cmd_resp) + p_cmd_resp->data_len;
		p_cmd_resp = (struct cmd_resp *)p;
	}

	gettimeofday(&tv1, NULL);
	log_message(DEBUG, "Done with dispatching a packet, latency is "
					"%d \n", tv1.tv_usec - tv.tv_usec);
}

static void remove_session_by_fd(int fd)
{
	int i = 0;

	while (i < SENSOR_MAX) {
		session_state_t *p, *p_session_state = sensor_list[i].list;

		p = p_session_state;

		for (; p_session_state != NULL;
			p_session_state = p_session_state->next) {
			if ((p_session_state->ctlfd != fd)
				&& (p_session_state->datafd != fd)) {
				p = p_session_state;
				continue;
			}
//			close(p_session_state->ctlfd);
//			close(p_session_state->datafd);

			if (i == SENSOR_EVENT) {
			/* special treatment for SENSOR_EVENT */
				handle_clear_event(p_session_state);
			} else if (i == SENSOR_CALIBRATION_COMP ||
					i == SENSOR_CALIBRATION_GYRO) {

			} else {

				stop_streaming(i, p_session_state);
			}
			if (p_session_state == sensor_list[i].list)
				sensor_list[i].list = p_session_state->next;
			else
				p->next = p_session_state->next;

			if (i == SENSOR_COMP || i == SENSOR_GYRO) {
				/* In this case, this session is closed,
				 * So calibration parameter is needed to store to file
				 */
				psh_sensor_t cal_sensor = (i == SENSOR_COMP) ?
							SENSOR_CALIBRATION_COMP :
							SENSOR_CALIBRATION_GYRO;

				/* Set the CALIBRATION_NEED_STORE bit,
				 * parameter will be stored when get_calibration() response arrives
				 */
				set_calibration_status(cal_sensor, CALIBRATION_NEED_STORE, NULL);
				get_calibration(cal_sensor, NULL);
			}

			log_message(DEBUG, "session with datafd %d, ctlfd %d "
				"is removed \n", p_session_state->datafd,
				p_session_state->ctlfd);
			free(p_session_state);
			return;
		}

		i++;
        }
}

static void *screen_state_thread(void *cookie)
{
	int fd, err;
	char buf;
	pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;

	while(1) {
		fd = open(SLEEP_NODE, O_RDONLY, 0);
		do {
			err = read(fd, &buf, 1);
			log_message(DEBUG,"wait for sleep err: %d, errno: %d\n", err, errno);
		} while (err < 0 && errno == EINTR);

		pthread_mutex_lock(&update_mutex);
		screen_state = 0;
		pthread_mutex_unlock(&update_mutex);

		write(notifyfd, "0", 1);

		close(fd);

		fd = open(WAKE_NODE, O_RDONLY, 0);
		do {
			err = read(fd, &buf, 1);
			log_message(DEBUG,"wait for wake err: %d, errno: %d\n", err, errno);
		} while (err < 0 && errno == EINTR);

		pthread_mutex_lock(&update_mutex);
		screen_state = 1;
		pthread_mutex_unlock(&update_mutex);

		write(notifyfd, "0", 1);

		close(fd);
	}
	return NULL;
}

void handle_screen_state_change(void)
{
	int i;
	session_state_t *p;

	if (screen_state == 0) {
		for (i = 0; i <= SENSOR_9DOF; i++) {
			/* calibration is not stopped */
			if (i == SENSOR_CALIBRATION_COMP ||
				i == SENSOR_CALIBRATION_GYRO)
				continue;

			p = sensor_list[i].list;

			while (p != NULL) {
				if ((p->state == ACTIVE)) {
					stop_streaming(i, p);
					p->state = NEED_RESUME;
				}
				p = p->next;
			}
		}
	} else if (screen_state == 1) {
		for (i = 0; i <= SENSOR_9DOF; i++) {
			/* calibration is not stopped */
			if (i == SENSOR_CALIBRATION_COMP ||
				i == SENSOR_CALIBRATION_GYRO)
				continue;

			p = sensor_list[i].list;

			while (p != NULL) {
				if (p->state == NEED_RESUME) {
					start_streaming(i, p, p->data_rate, p->buffer_delay, 0);
				}
				p = p->next;
			}
		}
	}
}

static void handle_no_stop_no_report()
{
	int i;
	session_state_t *p;

	for (i = 0; i < SENSOR_MAX; i++) {
		if (i == SENSOR_CALIBRATION_COMP ||
			i == SENSOR_CALIBRATION_GYRO)
			continue;

		p = sensor_list[i].list;

		while (p != NULL) {
			if ((p->state == ALWAYS_ON) && (p->flag == 1)) {
				//TODO: send set_property to psh fw
				if (i == SENSOR_PEDOMETER) {
					if (screen_state == 0) {
						send_set_property(i, PROP_STOP_REPORTING, 1);
					} else {
						send_set_property(i, PROP_STOP_REPORTING, 0);
					}
				}
			}
			p = p->next;
		}

	}
}

/* 1 create data thread
   2 wait and handle the request from client in main thread */
static void start_sensorhubd()
{
	fd_set listen_fds, read_fds;
	int maxfd;
	int notifyfds[2];
	int ret;
	pthread_t t;

	/* two fd_set for I/O multiplexing */
	FD_ZERO(&listen_fds);
	FD_ZERO(&read_fds);

	/* add sockfd to listen_fds */
//	listen(sockfd, MAX_Q_LENGTH);
	FD_SET(sockfd, &listen_fds);
	maxfd = sockfd;

	/* add notifyfds[0] to listen_fds */
	ret = pipe(notifyfds);
	if (ret < 0) {
		log_message(CRITICAL, "sensorhubd failed to create pipe \n");
		exit(EXIT_FAILURE);
	}

	notifyfd = notifyfds[1];

	FD_SET(notifyfds[0], &listen_fds);
	if (notifyfds[0] > maxfd)
		maxfd = notifyfds[0];

	pthread_create(&t, NULL, screen_state_thread, NULL);

	/* get data from data node and dispatch it to clients, begin */
	fd_set datasize_fds;
	FD_ZERO(&datasize_fds);

	if (datasizefd > maxfd)
		maxfd = datasizefd;
	/* get data from data node and dispatch it to clients, end */

	while (1) {
		read_fds = listen_fds;
		FD_SET(datasizefd, &datasize_fds);

		/* error handling of select() */
		if (select(maxfd + 1, &read_fds, NULL, &datasize_fds, NULL)
								== -1) {
			if (errno == EINTR)
				continue;
			else {
				log_message(CRITICAL, "sensorhubd socket "
						"select() failed. errno "
						"is %d\n", errno);
				exit(EXIT_FAILURE);
			}
		}

		/* handle new connection request */
		if (FD_ISSET(sockfd, &read_fds)) {
			struct sockaddr_un client_addr;
			socklen_t addrlen = sizeof(client_addr);
			int clientfd = accept(sockfd,
					(struct sockaddr *) &client_addr,
					&addrlen);
			if (clientfd == -1) {
				log_message(CRITICAL, "sensorhubd socket "
						"accept() failed. \n");
				exit(EXIT_FAILURE);
			} else {
				log_message(CRITICAL, "new connection from client \n");
				FD_SET(clientfd, &listen_fds);
				if (clientfd > maxfd)
					maxfd = clientfd;
			}

			FD_CLR(sockfd, &read_fds);
		}

		/* get data from data node and dispatch it to clients, begin */
		if (FD_ISSET(datasizefd, &datasize_fds)) {
			dispatch_data();
		}
		/* get data from data node and dispatch it to clients, end */

		/* handle wake/sleep notification, begin */
		if (FD_ISSET(notifyfds[0], &read_fds)) {
			char buf[20];
			int ret;
			ret = read(notifyfds[0], buf, 20);
			if (ret > 0)
				log_message(DEBUG, "Get notification,buf is %s, screen state is %d \n", buf, screen_state);

//			handle_screen_state_change();
			handle_no_stop_no_report();
			FD_CLR(notifyfds[0], &read_fds);
		}

		/* handle wake/sleep notification, end */

		/* handle request from clients */
		int i;
		for (i = maxfd; i >= 0; i--) {
			char message[MAX_MESSAGE_LENGTH];

			if (!FD_ISSET(i, &read_fds))
				continue;

			int length = recv(i, message, MAX_MESSAGE_LENGTH, 0);
			if (length <= 0) {
				/* release session resource if necessary */
				remove_session_by_fd(i);
				close(i);
				log_message(DEBUG, "fd %d:error reading message \n", i);
				FD_CLR(i, &listen_fds);
			} else {
				/* process message */
				handle_message(i, message);
			}
		}
	}
}

static void setup_psh()
{
	// setup DDR
	send_control_cmd(0, 1, 0, 0, 0);
	// reset
	send_control_cmd(0, 0, 0, 0, 0);

	return;
}


#define CMD_GET_STATUS		11
#define SNR_NAME_MAX_LEN	6

static void get_status()
{
#define LINK_AS_CLIENT		(0)
#define LINK_AS_MONITOR		(1)
#define LINK_AS_REPORTER	(2)
	struct link_info {
		unsigned char id;
		unsigned char ltype;
		unsigned short slide;
	} __attribute__ ((packed));

	struct sensor_info {
		unsigned char id;
		unsigned char status;
		unsigned short freq;
		unsigned short data_cnt;
		unsigned short slide;
		unsigned short priv;
		unsigned short attri;

		short freq_max;	// -1, means no fixed data rate
		char name[SNR_NAME_MAX_LEN];

		unsigned char health;
		unsigned char link_num;
		struct link_info linfo[0];
	} __attribute__ ((packed));

	char cmd_string[MAX_STRING_SIZE];
	int size, ret;
	struct cmd_resp resp;
	struct sensor_info *snr_info;
	char buf[512];

	struct timeval tv, tv1;
	gettimeofday(&tv, NULL);

	size = snprintf(cmd_string, MAX_STRING_SIZE, "%d %d %d %d %d %d %d",
			0, CMD_GET_STATUS, 0, 0xff, 0xff, 0xff, 0xff);
	log_message(DEBUG, "cmd to sysfs is: %s\n", cmd_string);

	ret = write(ctlfd, cmd_string, size);
	log_message(DEBUG, "cmd return value is %d\n", ret);
	if (ret < 0)
		exit(EXIT_FAILURE);

	while ((ret = read(datafd, &resp, sizeof(resp))) >= 0) {
		if (ret == 0) {
			usleep(100000);
			continue;
		}

		// it's safe to cast ret to unsigned int since ret > 0 at this point
		if ((unsigned int)ret < sizeof(resp))
			exit(EXIT_FAILURE);

		if (resp.cmd_type != CMD_GET_STATUS)
			exit(EXIT_FAILURE);

		if (resp.data_len == 0)
			break;

		ret = read(datafd, buf, resp.data_len);
		if (ret != resp.data_len)
			exit(EXIT_FAILURE);

		snr_info = (struct sensor_info *)buf;

		log_message(DEBUG, "sensor id is %d, name is %s, freq_max is %d \n", snr_info->id, snr_info->name, snr_info->freq_max);

		if (strncmp(snr_info->name, "GYROC", SNR_NAME_MAX_LEN - 1) == 0)
			continue;

		if (strncmp(snr_info->name, "ACCEL", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_ACCELEROMETER] = snr_info->id;
			sensor_list[SENSOR_ACCELEROMETER].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_ACCELEROMETER],
									sensor_list[SENSOR_ACCELEROMETER].freq_max);
		} else if (strncmp(snr_info->name, "GYRO", SNR_NAME_MAX_LEN - 2) == 0) {
			sensor_type_to_sensor_id[SENSOR_GYRO] = snr_info->id;
			sensor_list[SENSOR_GYRO].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_GYRO],
									sensor_list[SENSOR_GYRO].freq_max);
		} else if (strncmp(snr_info->name, "COMPS", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_COMP] = snr_info->id;
			sensor_list[SENSOR_COMP].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_COMP],
									sensor_list[SENSOR_COMP].freq_max);
		} else if (strncmp(snr_info->name, "BARO", SNR_NAME_MAX_LEN - 2) == 0) {
			sensor_type_to_sensor_id[SENSOR_BARO] = snr_info->id;
			sensor_list[SENSOR_BARO].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_BARO],
									sensor_list[SENSOR_BARO].freq_max);
		} else if (strncmp(snr_info->name, "ALS_P", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_ALS] = snr_info->id;
			sensor_list[SENSOR_ALS].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_ALS],
									sensor_list[SENSOR_ALS].freq_max);
		} else if (strncmp(snr_info->name, "PS_P", SNR_NAME_MAX_LEN - 2) == 0) {
			sensor_type_to_sensor_id[SENSOR_PROXIMITY] = snr_info->id;
			sensor_list[SENSOR_PROXIMITY].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_PROXIMITY],
									sensor_list[SENSOR_PROXIMITY].freq_max);
		} else if (strncmp(snr_info->name, "TERMC", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_TC] = snr_info->id;
			sensor_list[SENSOR_TC].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_TC],
									sensor_list[SENSOR_TC].freq_max);
		} else if (strncmp(snr_info->name, "GSSPT", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_GS] = snr_info->id;
			sensor_list[SENSOR_GS].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_GS],
									sensor_list[SENSOR_GS].freq_max);
		} else if (strncmp(snr_info->name, "PHYAC", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_ACTIVITY] = snr_info->id;
			sensor_list[SENSOR_ACTIVITY].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_ACTIVITY],
									sensor_list[SENSOR_ACTIVITY].freq_max);
		} else if (strncmp(snr_info->name, "9DOF", SNR_NAME_MAX_LEN - 2) == 0) {
			sensor_type_to_sensor_id[SENSOR_9DOF] = snr_info->id;
			sensor_list[SENSOR_9DOF].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_9DOF],
									sensor_list[SENSOR_9DOF].freq_max);
		} else if (strncmp(snr_info->name, "GSFLK", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_GESTURE_FLICK] = snr_info->id;
			sensor_list[SENSOR_GESTURE_FLICK].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_GESTURE_FLICK],
									sensor_list[SENSOR_GESTURE_FLICK].freq_max);
		} else if (strncmp(snr_info->name, "GRAVI", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_GRAVITY] = snr_info->id;
			sensor_list[SENSOR_GRAVITY].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_GRAVITY],
									sensor_list[SENSOR_GRAVITY].freq_max);
		} else if (strncmp(snr_info->name, "ORIEN", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_ORIENTATION] = snr_info->id;
			sensor_list[SENSOR_ORIENTATION].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_ORIENTATION],
									sensor_list[SENSOR_ORIENTATION].freq_max);
		} else if (strncmp(snr_info->name, "LACCL", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_LINEAR_ACCEL] = snr_info->id;
			sensor_list[SENSOR_LINEAR_ACCEL].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_LINEAR_ACCEL],
									sensor_list[SENSOR_LINEAR_ACCEL].freq_max);
		} else if (strncmp(snr_info->name, "RVECT", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_ROTATION_VECTOR] = snr_info->id;
			sensor_list[SENSOR_ROTATION_VECTOR].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_ROTATION_VECTOR],
									sensor_list[SENSOR_ROTATION_VECTOR].freq_max);
		} else if (strncmp(snr_info->name, "MAGHD", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_MAG_HEADING] = snr_info->id;
			sensor_list[SENSOR_MAG_HEADING].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_MAG_HEADING],
									sensor_list[SENSOR_MAG_HEADING].freq_max);
		} else if (strncmp(snr_info->name, "PEDOM", SNR_NAME_MAX_LEN - 1) == 0) {
			sensor_type_to_sensor_id[SENSOR_PEDOMETER] = snr_info->id;
			sensor_list[SENSOR_PEDOMETER].freq_max = snr_info->freq_max;
			log_message(DEBUG, "id is %d, freq_max is %d \n", sensor_type_to_sensor_id[SENSOR_PEDOMETER],
									sensor_list[SENSOR_PEDOMETER].freq_max);
		}
	}

	gettimeofday(&tv1, NULL);
	log_message(DEBUG, "latency of is get_status() is "
			"%d \n", tv1.tv_usec - tv.tv_usec);

	if (ret < 0)
		 exit(EXIT_FAILURE);
}

static void usage()
{
	printf("Usage: sensorhubd [OPTION...] \n");
	printf("  -d, --enable-data-rate-debug      1: enable; 0: disable (default) \n");
	printf("  -l, --log-level        0-2, 2 is most verbose level \n");
	printf("  -h, --help             show this help message \n");

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int log_level = CRITICAL;

	if (is_first_instance() != 1)
		exit(0);

	while (1) {
		static struct option opts[] = {
			{"enable-data-rate-debug", 1, NULL, 'd'},
			{"log-level", 2, NULL, 'l'},
			{"help", 0, NULL, 'h'},
			{0, 0, NULL, 0}
		};

		int index, o;

		o = getopt_long(argc, argv, "d:l::h", opts, &index);
		if (o == -1)
			break;

		switch (o) {
		case 'd':
			if (optarg != NULL)
				enable_debug_data_rate = strtod(optarg, NULL);
			log_message(DEBUG, "enable_debug_data_rate is %d \n", enable_debug_data_rate);
			break;
		case 'l':
			if (optarg != NULL)
				log_level = strtod(optarg, NULL);
			log_message(DEBUG, "log_level is %d \n", log_level);
			set_log_level(log_level);
			break;
		case 'h':
			usage();
			break;
		default:
			usage();
		}
	}

	daemonize();

	memset(sensor_list, 0, sizeof(sensor_list));
	while (1) {
		reset_sensorhub();
//		sleep(60);
		setup_psh();
		get_status();
//		reset_client_sessions();
		start_sensorhubd();
	}
	return 0;
}