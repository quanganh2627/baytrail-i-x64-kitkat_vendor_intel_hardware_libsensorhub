#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <hardware_legacy/power.h>
#include <cutils/sockets.h>

#include "socket.h"
#include "utils.h"
#include "message.h"
#include "platform_def.h"
#include "lpe.h"

#ifdef ENABLE_CONTEXT_ARBITOR
#include <awarelibs/libcontextarbitor.h>
#endif

#define MAX_STRING_SIZE 256
#define MAX_PROP_VALUE_SIZE 58

#define WAKE_NODE "/sys/power/wait_for_fb_wake"
#define SLEEP_NODE "/sys/power/wait_for_fb_sleep"

static const char *log_file = "/data/sensorhubd.log";

#define IA_BIT_CFG_AS_WAKEUP_SRC ((unsigned short)(0x1 << 0))

/* The Unix socket file descriptor */
static int sockfd = -1;    // main upper half socket  (sensorhubd)

static int enable_debug_data_rate = 0;

sensor_state_t sensor_list[MAX_SENSOR_INDEX];
unsigned int current_sensor_index = 0;	// means the current empty slot of sensor_list[]

static char flush_completion_frame[MAX_UNIT_SIZE];

sensor_state_t* get_sensor_state_with_type(ish_sensor_t sensor_type)
{
	unsigned int i;

	for (i = 0; i < current_sensor_index; i ++) {
		if (sensor_list[i].sensor_info->sensor_type == sensor_type) {
			return &sensor_list[i];
		}
	}

	return NULL;
}

sensor_state_t* get_sensor_state_with_name(const char *name)
{
        unsigned int i;

        for (i = 0; i < current_sensor_index; i ++) {
                if (strncmp(sensor_list[i].sensor_info->name, name, SNR_NAME_MAX_LEN) == 0) {
                        return &sensor_list[i];
                }
        }

        return NULL;
}

session_state_t* get_session_state_with_session_id(
					session_id_t session_id)
{
	unsigned int i;

	log_message(DEBUG, "get_session_state_with_session_id(): "
				"session_id %d\n", session_id);

	for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->session_id == session_id)
				return p_session_state;

			p_session_state = p_session_state->next;
		}
        }

	log_message(WARNING, "get_session_state_with_session_id(): "
				"NULL is returned\n");

	return NULL;
}

/* return 0 on success; -1 on fail */
int get_sensor_state_session_state_with_fd(int ctlfd,
					sensor_state_t **pp_sensor_state,
					session_state_t **pp_session_state)
{
	unsigned int i;

	 for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->ctlfd == ctlfd) {
				*pp_sensor_state = &sensor_list[i];
				*pp_session_state = p_session_state;

				return 0;
			}

			p_session_state = p_session_state->next;
		}

	}

	return -1;
}

static session_state_t* get_session_state_with_trans_id(
					unsigned char trans_id)
{
	sensor_state_t *p_sensor_state;
	session_state_t *p_session_state;
	unsigned int i;

	for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p_session_state = sensor_list[i].list;

		while (p_session_state != NULL) {
			if (p_session_state->trans_id == trans_id)
				return p_session_state;

			p_session_state = p_session_state->next;
		}
        }

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

/* flag: 1 means start_streaming, 0 means stop_streaming */
static int data_rate_arbiter(sensor_state_t *p_sensor_state,
				unsigned int data_rate,
				session_state_t *p_session_state,
				char flag)
{
	session_state_t *p_session_state_tmp;
	unsigned int max_data_rate = 0;

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return 0;
	}

	if (flag == 1) {
		if (p_sensor_state->data_rate == 0)
			return data_rate;
		else if (data_rate > p_sensor_state->data_rate)
			return data_rate;
		else
			max_data_rate = data_rate;

	}

	/* flag == 0 */
	p_session_state_tmp = p_sensor_state->list;

	while (p_session_state_tmp != NULL) {
		if ((p_session_state_tmp != p_session_state)
			&& ((p_session_state_tmp->state == ACTIVE)
			|| (p_session_state_tmp->state == ALWAYS_ON))) {
			if (max_data_rate < p_session_state_tmp->data_rate) {
				max_data_rate = p_session_state_tmp->data_rate;
			}
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	return max_data_rate;
}

/* flag: 1 means add a new buffer_delay, 0 means remove a buffer_delay */
static int buffer_delay_arbiter(sensor_state_t *p_sensor_state,
				int buffer_delay,
				session_state_t *p_session_state,
				char flag)
{
	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return 0;
	}

	if (flag == 1) {
		int r;
		if (buffer_delay == 0)
			return 0;

		/* a new buffer_delay and not the first one */
		if (p_sensor_state->buffer_delay == 0) {
			session_state_t *tmp = p_sensor_state->list;
			while (tmp != NULL) {
				if ((tmp != p_session_state)
					&& ((tmp->state == ACTIVE)
					|| (tmp->state == ALWAYS_ON)))
					return 0;
				tmp = tmp->next;
			}
		}
		r = max_common_factor(p_sensor_state->buffer_delay,
							buffer_delay);
		return r;
	}
	/* flag == 0 */
	int data1 = 0, data2;
	session_state_t *p_session_state_tmp = p_sensor_state->list;

	if ((p_sensor_state->buffer_delay == 0) && (buffer_delay != 0))
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

/* return as long as anyone is ALWAYS_ON */
static unsigned short bit_cfg_arbiter(sensor_state_t *p_sensor_state,
                                unsigned short bit_cfg,
                                session_state_t *p_session_state)
{
	session_state_t *p_session_state_tmp;

	if (bit_cfg != 0)
		return bit_cfg;

	p_session_state_tmp = p_sensor_state->list;
	while (p_session_state_tmp != NULL) {
		if (p_session_state_tmp != p_session_state) {
			if (p_session_state_tmp->state == ALWAYS_ON) {
				return IA_BIT_CFG_AS_WAKEUP_SRC;
			}
		}

		p_session_state_tmp = p_session_state_tmp->next;
	}

	return 0;
}

static int send_control_cmd(struct cmd_send *cmd)
{
	int i, j;
	sensor_state_t *state = get_sensor_state_with_type(cmd->sensor_type);
	unsigned int index;
	int ret = ERROR_NONE;

	if (state == NULL) {
		log_message(CRITICAL, "[%s]: Invalid sensor type!\n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	log_message(DEBUG, "[%s]: cmd_id %d, sensor_type %d\n", __func__, cmd->cmd_id, cmd->sensor_type);

	index = state->index;

	i = sizeof(ish_platf) / sizeof(ish_platform_t);
	for (j = 0; j < i; j++)
		if (index >= ish_platf[j].index_start && index <= ish_platf[j].index_end)
			if (ish_platf[j].send_cmd)
				ret = ish_platf[j].send_cmd(cmd);

	return ret;
}

static int send_set_property(sensor_state_t *p_sensor_state, session_state_t *p_session_state,
                                 property_type prop_type, int len, unsigned char *value)
{
	struct cmd_send cmd;
	int ret;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (p_session_state == NULL) {
		log_message(CRITICAL, "%s: p_session_state is NULL \n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (value == NULL) {
		log_message(CRITICAL, "%s: value buf is NULL\n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (len > MAX_PROP_VALUE_LEN) {
		log_message(CRITICAL, "%s: property length over the max lenght\n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	cmd.tran_id = 0;
	cmd.cmd_id = CMD_SET_PROPERTY;
	cmd.sensor_type = p_sensor_state->sensor_info->sensor_type;
	cmd.set_prop.prop_type = prop_type;
	cmd.set_prop.len = len;
	memcpy(cmd.set_prop.value, value, len);

	ret = send_control_cmd(&cmd);

	return ret;
}

static int process_bist_get_property(sensor_state_t *p_sensor_state, session_state_t *p_session_state,
									int len, unsigned char *value)
{
	cmd_ack_event *cmd_ack;
	ish_usecase_t usecase = (ish_usecase_t)*value;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (p_session_state == NULL) {
		log_message(CRITICAL, "%s: p_session_state is NULL \n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (len != sizeof(ish_usecase_t))
		return ERR_WRONG_PARAMETER;

	cmd_ack = malloc(sizeof(cmd_ack_event) + current_sensor_index * sizeof(sensor_info_t));
	if (cmd_ack) {
		unsigned int i, j;
		sensor_info_t *tmp_buf = (sensor_info_t *)cmd_ack->buf;

		for (i = 0, j = 0; i < current_sensor_index; i++) {
			if (sensor_list[i].sensor_info->use_case <= usecase) {
				memcpy((char *)(tmp_buf + j), (char *)sensor_list[i].sensor_info, sizeof(sensor_info_t));
				/* because client can not and no need to access platform data */
				(tmp_buf + j)->plat_data = NULL;
				j++;
			}
		}

		cmd_ack->event_type = EVENT_CMD_ACK;
		cmd_ack->buf_len = j * sizeof(sensor_info_t);
		cmd_ack->ret = SUCCESS;

		if (send(p_session_state->ctlfd, cmd_ack, sizeof(cmd_ack_event) + cmd_ack->buf_len, 0) < 0)
			log_message(CRITICAL, "%s line: %d: send message to client error: %s\n", __FUNCTION__, __LINE__, strerror(errno));

		free(cmd_ack);

		return SUCCESS;
	} else {
		cmd_ack_event cmd_ack;

		cmd_ack.event_type = EVENT_CMD_ACK;
		cmd_ack.ret = ERR_NO_MEMORY;

		if (send(p_session_state->ctlfd, &cmd_ack, sizeof(cmd_ack), 0) < 0)
			log_message(CRITICAL, "%s line: %d: send message to client error: %s\n", __FUNCTION__, __LINE__, strerror(errno));

		return ERR_NO_MEMORY;
	}
}

static int send_get_property(sensor_state_t *p_sensor_state, session_state_t *p_session_state,
								int len, unsigned char *value)
{
	log_message(DEBUG, "[%s] enter\n", __func__);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (p_session_state == NULL) {
		log_message(CRITICAL, "%s: p_session_state is NULL \n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (value == NULL || len == 0) {
		log_message(CRITICAL, "%s: value buf is NULL\n", __func__);
		return ERROR_WRONG_PARAMETER;
	}

	if (p_sensor_state->sensor_info->sensor_type == SENSOR_BIST)
		return process_bist_get_property(p_sensor_state, p_session_state, len, value);

	return SUCCESS;
}

/* flag: 2 means no_stop_no_report; 1 means no_stop when screen off; 0 means stop when screen off */
static void start_streaming(sensor_state_t *p_sensor_state,
				session_state_t *p_session_state,
				int data_rate, int buffer_delay, int flag)
{
	int data_rate_arbitered, buffer_delay_arbitered;
	unsigned short bit_cfg = 0;
	struct cmd_send cmd;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "p_sensor_state is NULL \n");
		return;
	}

        if (p_sensor_state->sensor_info->sensor_type == SENSOR_LPE) {
                lpe_start_streaming(data_rate, buffer_delay);
                p_session_state->state = ACTIVE;
                return;
        }

	if (data_rate != -1)
		data_rate_arbitered = data_rate_arbiter(p_sensor_state, data_rate,
						p_session_state, 1);
	else
		data_rate_arbitered = -1;

	buffer_delay_arbitered = buffer_delay_arbiter(p_sensor_state, buffer_delay,
						p_session_state, 1);

	log_message(DEBUG, "CMD_START_STREAMING, data_rate_arbitered "
				"is %d, buffer_delay_arbitered is %d \n",
				data_rate_arbitered, buffer_delay_arbitered);

	if(data_rate_arbitered <=0 || buffer_delay_arbitered < 0)
		return;

	p_sensor_state->data_rate = data_rate_arbitered;
	p_sensor_state->buffer_delay = buffer_delay_arbitered;

	if ((flag == 1) || (flag == 2))
		bit_cfg = IA_BIT_CFG_AS_WAKEUP_SRC;
	else
		bit_cfg = bit_cfg_arbiter(p_sensor_state, 0, p_session_state);

	cmd.tran_id = 0;
	cmd.cmd_id = CMD_START_STREAMING;
	cmd.sensor_type = p_sensor_state->sensor_info->sensor_type;
	cmd.start_stream.data_rate = data_rate_arbitered;
	cmd.start_stream.buffer_delay = buffer_delay_arbitered;
	cmd.start_stream.bit_cfg = bit_cfg;
	send_control_cmd(&cmd);

	if (data_rate != 0) {
		p_session_state->data_rate = data_rate;
		p_session_state->buffer_delay = buffer_delay;
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

static void stop_streaming(sensor_state_t *p_sensor_state,
				session_state_t *p_session_state)
{
	unsigned int data_rate_arbitered, buffer_delay_arbitered;
	unsigned short bit_cfg_arbitered;
	struct cmd_send cmd;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	if (p_session_state->state == INACTIVE)
		return;

	if (p_session_state->state == NEED_RESUME) {
		p_session_state->state = INACTIVE;
		return;
	}

        if (p_sensor_state->sensor_info->sensor_type == SENSOR_LPE) {
                lpe_stop_streaming();
                p_session_state->state = INACTIVE;
                return;
        }

	data_rate_arbitered = data_rate_arbiter(p_sensor_state,
						p_session_state->data_rate,
						p_session_state, 0);
	buffer_delay_arbitered = buffer_delay_arbiter(p_sensor_state,
						p_session_state->buffer_delay,
						p_session_state, 0);

	p_session_state->state = INACTIVE;

	if ((p_sensor_state->data_rate == data_rate_arbitered)
	&& (p_sensor_state->buffer_delay == buffer_delay_arbitered))
		return;

	p_sensor_state->data_rate = data_rate_arbitered;
	p_sensor_state->buffer_delay = buffer_delay_arbitered;

	if (data_rate_arbitered != 0) {
		/* send CMD_START_STREAMING to sysfs control node to
		   re-config the data rate */
		bit_cfg_arbitered = bit_cfg_arbiter(p_sensor_state, 0, p_session_state);

		cmd.tran_id = 0;
		cmd.cmd_id = CMD_START_STREAMING;
		cmd.sensor_type = p_sensor_state->sensor_info->sensor_type;
		cmd.start_stream.data_rate = data_rate_arbitered;
		cmd.start_stream.buffer_delay = buffer_delay_arbitered;
		cmd.start_stream.bit_cfg = bit_cfg_arbitered;
		send_control_cmd(&cmd);
	} else {
		/* send CMD_STOP_STREAMING cmd to sysfs control node */
		cmd.tran_id = 0;
		cmd.cmd_id = CMD_STOP_STREAMING;
		cmd.sensor_type = p_sensor_state->sensor_info->sensor_type;
		cmd.start_stream.data_rate = 0;
		cmd.start_stream.buffer_delay = 0;
		cmd.start_stream.bit_cfg = 0;
		send_control_cmd(&cmd);
	}

	log_message(DEBUG, "CMD_STOP_STREAMING, data_rate_arbitered is %d, "
			"buffer_delay_arbitered is %d \n", data_rate_arbitered,
			buffer_delay_arbitered);
}

static void flush_streaming(sensor_state_t *p_sensor_state, session_state_t *p_session_state, int data_unit_size)
{
	struct cmd_send cmd;

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "%s: p_sensor_state is NULL \n", __func__);
		return;
	}

	if (p_session_state->state == INACTIVE)
		return;

	p_session_state->flush_complete_event_size = data_unit_size;
	p_session_state->flush_count++;

	cmd.tran_id = 0;
	cmd.cmd_id = CMD_FLUSH_STREAMING;
	cmd.sensor_type = p_sensor_state->sensor_info->sensor_type;
	send_control_cmd(&cmd);
}

static int recalculate_data_rate(sensor_state_t *p_sensor_state, int data_rate)
{
	if (p_sensor_state == NULL) {
		log_message(DEBUG, "%s: p_sensor_state is NULL \n", __func__);
		return -1;
	}

	return data_rate;
}

/* 1 arbiter
   2 send cmd to control node under sysfs.
     cmd format is <TRANID><CMDID><SENSORID><RATE><BUFFER DELAY>
     CMDID refer to ish_fw/include/cmd_engine.h;
     SENSORID refer to ish_fw/sensors/include/sensor_def.h */
static ret_t handle_cmd(int fd, cmd_event* p_cmd, int parameter, int parameter1,
						int parameter2,	int *reply_now)
{
	session_state_t *p_session_state;
	sensor_state_t *p_sensor_state;
	int data_rate_calculated, size;
	int ret = get_sensor_state_session_state_with_fd(fd, &p_sensor_state,
							&p_session_state);
	cmd_t cmd = p_cmd->cmd;

	log_message(DEBUG, "[%s] Ready to handle command %d\n", __func__, cmd);

	*reply_now = 1;

	if (ret != 0)
		return ERR_SESSION_NOT_EXIST;

	if (cmd == CMD_START_STREAMING) {
		data_rate_calculated = recalculate_data_rate(p_sensor_state, parameter);
		start_streaming(p_sensor_state, p_session_state,
					data_rate_calculated, parameter1, parameter2);
	} else if (cmd == CMD_STOP_STREAMING) {
		stop_streaming(p_sensor_state, p_session_state);
	} else if (cmd == CMD_FLUSH_STREAMING) {
		flush_streaming(p_sensor_state, p_session_state, parameter);
	} else if (cmd == CMD_SET_PROPERTY) {
#ifdef ENABLE_CONTEXT_ARBITOR
		ctx_option_t *out_option = NULL;
		int i;

		if (ctx_set_option(p_session_state->handle, p_cmd->parameter, (char *)p_cmd->buf, &out_option) == -1)
			return ERR_CMD_NOT_SUPPORT;

		if (out_option == NULL)
			return ERR_CMD_NOT_SUPPORT;

		for (i = 0; i < out_option->len; i++) {
			send_set_property(p_sensor_state, p_session_state, out_option->items[i].prop, out_option->items[i].size, (unsigned char *)out_option->items[i].value);
		}

		if (out_option->len == 0)
			*reply_now = 1;

		ctx_option_release(out_option);
#endif
#ifndef ENABLE_CONTEXT_ARBITOR
		send_set_property(p_sensor_state, p_session_state, p_cmd->parameter, p_cmd->parameter1, p_cmd->buf);    // property type, property size, property value
#endif
	} else if (cmd == CMD_GET_PROPERTY) {
		send_get_property(p_sensor_state, p_session_state, p_cmd->parameter, p_cmd->buf);
		*reply_now = 0;
	}

	return SUCCESS;
}

static void handle_message(int fd, char *message)
{
	int event_type = *((int *) message);

	log_message(DEBUG, "[%s]: fd is %d, event_type is %d\n",
					__func__,fd, event_type);

	if (event_type == EVENT_HELLO_WITH_SENSOR_TYPE) {
		hello_with_sensor_type_ack_event hello_with_sensor_type_ack;
		session_state_t *p_session_state, **next;
		hello_with_sensor_type_event *p_hello_with_sensor_type =
					(hello_with_sensor_type_event *)message;
		sensor_state_t *p_sensor_state = NULL;

		log_message(DEBUG, "EVENT_HELLO_WITH_SENSOR_TYPE name = %s\n",p_hello_with_sensor_type->name);

		if ((p_sensor_state = get_sensor_state_with_name(p_hello_with_sensor_type->name)) == NULL) {
			hello_with_sensor_type_ack.event_type = EVENT_HELLO_WITH_SENSOR_TYPE;
			hello_with_sensor_type_ack.session_id = 0;

			if (send(fd, &hello_with_sensor_type_ack, sizeof(hello_with_sensor_type_ack), 0) < 0)
				log_message(CRITICAL, "%s line: %d: send message to client error: %s\n", __FUNCTION__, __LINE__, strerror(errno));

			log_message(CRITICAL, "sensor type %s not supported \n", p_hello_with_sensor_type->name);
			return;
		}
		session_id_t session_id = allocate_session_id();

		/* allocate session ID and return it to client;
		   allocate session_state and add it to sensor_list */
		hello_with_sensor_type_ack.event_type =
					EVENT_HELLO_WITH_SENSOR_TYPE_ACK;
		hello_with_sensor_type_ack.session_id = session_id;
		if (send(fd, &hello_with_sensor_type_ack, sizeof(hello_with_sensor_type_ack), 0) < 0)
			log_message(CRITICAL, "%s line: %d: send message to client error: %s\n", __FUNCTION__, __LINE__, strerror(errno));

		p_session_state = (session_state_t*) malloc(sizeof(session_state_t));
		if (p_session_state == NULL) {
			log_message(CRITICAL, "malloc failed \n");
			return;
		}

		memset(p_session_state, 0, sizeof(session_state_t));
		p_session_state->datafd = fd;
		p_session_state->session_id = session_id;
#ifdef ENABLE_CONTEXT_ARBITOR
		p_session_state->handle = ctx_open_session(p_sensor_state->name);
#endif

		p_session_state->next = p_sensor_state->list;
		p_sensor_state->list = p_session_state;

	} else if (event_type == EVENT_HELLO_WITH_SESSION_ID) {
		hello_with_session_id_ack_event hello_with_session_id_ack;
		hello_with_session_id_event *p_hello_with_session_id =
					(hello_with_session_id_event *)message;
		session_id_t session_id = p_hello_with_session_id->session_id;
		session_state_t *p_session_state =
				get_session_state_with_session_id(session_id);

		if (p_session_state == NULL) {
			log_message(CRITICAL, "EVENT_HELLO_WITH_SESSION_ID, not find matching session_id \n");
			return;
		}

		p_session_state->ctlfd = fd;

		hello_with_session_id_ack.event_type =
					EVENT_HELLO_WITH_SESSION_ID_ACK;
		hello_with_session_id_ack.ret = SUCCESS;

		if (send(fd, &hello_with_session_id_ack, sizeof(hello_with_session_id_ack), 0) < 0)
			log_message(CRITICAL, "%s line: %d: send message to client error: %s\n", __FUNCTION__, __LINE__, strerror(errno));

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

		if (send(fd, &cmd_ack, sizeof(cmd_ack), 0) < 0)
			log_message(CRITICAL, "%s line: %d: send message to client error: %s\n", __FUNCTION__, __LINE__, strerror(errno));
	} else {
		/* TODO: unknown message and drop it */
	}
}

sensor_info_t bist_info = {
	.name = "BIST",
	.vendor = "Intel inc.",
	.sensor_type = SENSOR_BIST,
	.use_case = USE_CASE_HUB,
};

/* 1 (re)open control node and send 'reset' cmd
   2 (re)open data node
   3 (re)open Unix socket */
static void reset_sensorhub()
{
	int i, j;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (sockfd != -1) {
		close(sockfd);
		sockfd = -1;
	}

	memset((void *)&sensor_list, 0, MAX_SENSOR_INDEX * sizeof(sensor_state_t));
	current_sensor_index = 0;

	i = sizeof(ish_platf) / sizeof(ish_platform_t);
	for (j = 0; j < i; j++) {
		if (ish_platf[j].init) {
			ish_platf[j].index_start = current_sensor_index;

			if (ish_platf[j].init((void *)sensor_list, &current_sensor_index)) {
				ish_platf[j].index_start = MAX_SENSOR_INDEX;
				ish_platf[j].index_end = MAX_SENSOR_INDEX;
			} else {
				ish_platf[j].index_end = current_sensor_index - 1;
			}
		}
	}

	log_message(DEBUG,"after reset sensor num %d\n", current_sensor_index);

	/* create bist sensor state */
	sensor_list[current_sensor_index++].sensor_info = &bist_info;

	/* create sensorhubd Unix socket */
	sockfd = android_get_control_socket(UNIX_SOCKET_PATH);

	log_message(DEBUG,"sockFD is %d\n", sockfd);

	listen(sockfd, MAX_Q_LENGTH);
}

static void send_data_to_clients(sensor_state_t *p_sensor_state, void *data,
						int size)
{
	session_state_t *p_session_state = p_sensor_state->list;
	char buf[MAX_MESSAGE_LENGTH];

	/* Use slide window mechanism to send data to target client,
	   buffer_delay is gauranteed, data count is not gauranteed.
	   calculate count of incoming data, send to clients by count  */
	for (; p_session_state != NULL;
		p_session_state = p_session_state->next) {
		int step, index = 0, i = 0;

		if ((p_session_state->state != ACTIVE) && (p_session_state->state != ALWAYS_ON))
			continue;

#ifdef ENABLE_CONTEXT_ARBITOR
		void *out_data;
		int out_size;

		if (p_session_state->handle == NULL) {
			if (send(p_session_state->datafd, data, size, MSG_NOSIGNAL|MSG_DONTWAIT) < 0) {
				log_message(CRITICAL, "%s line: %d: send message to client error: %s name: %s\n",
							__func__, __LINE__, strerror(errno), p_sensor_state->sensor_info->name);
				return;
			}
		} else if (ctx_dispatch_data(p_session_state->handle, data, size, &out_data, &out_size) == 1) {
			if (send(p_session_state->datafd, out_data, out_size, MSG_NOSIGNAL|MSG_DONTWAIT) < 0) {
				log_message(CRITICAL, "%s line: %d: send message to client error: %s name: %s\n",
							__func__, __LINE__, strerror(errno), p_sensor_state->sensor_info->name);
				return;
			}
		}
#else
		if (send(p_session_state->datafd, data, size, MSG_NOSIGNAL|MSG_DONTWAIT) < 0) {
			log_message(CRITICAL, "%s line: %d: send message to client error: %s name: %s\n",
						__func__, __LINE__, strerror(errno), p_sensor_state->sensor_info->name);
			return;
		}
#endif
	}
}

void dispatch_streaming(struct cmd_resp *p_cmd_resp)
{
	ish_sensor_t sensor_type = p_cmd_resp->sensor_type;
	sensor_state_t *p_sensor_state = get_sensor_state_with_type(sensor_type);

	if (p_sensor_state == NULL) {
		log_message(CRITICAL, "p_sensor_state is NULL with type %d \n", sensor_type);
		return;
	}

	send_data_to_clients(p_sensor_state, p_cmd_resp->buf, p_cmd_resp->data_len);
}

void dispatch_flush()
{
	unsigned int i;

        for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p_session_state = sensor_list[i].list;

		for (; p_session_state != NULL; p_session_state = p_session_state->next) {
                        while (p_session_state->flush_count > 0 && (p_session_state->flush_complete_event_size <= MAX_UNIT_SIZE)) {

                                p_session_state->flush_count--;

                                if (send(p_session_state->datafd, flush_completion_frame,
					p_session_state->flush_complete_event_size, MSG_NOSIGNAL|MSG_DONTWAIT) < 0) {
                                        log_message(CRITICAL, "%s line: %d: send message to client error: %s name: %s\n",
								__func__, __LINE__, strerror(errno), sensor_list[i].sensor_info->name);
                                        continue;
                                }
			}
		}

        }

	return;
}

static void remove_session_by_fd(int fd)
{
	unsigned int i = 0;

	log_message(DEBUG, "[%s] enter\n", __func__);

	for (i = 0; i < current_sensor_index; i ++) {
		session_state_t *p, *p_session_state = sensor_list[i].list;

		p = p_session_state;

		for (; p_session_state != NULL;
			p_session_state = p_session_state->next) {
			if ((p_session_state->ctlfd != fd)
				&& (p_session_state->datafd != fd)) {
				p = p_session_state;
				continue;
			}

			//close(p_session_state->ctlfd);
			//close(p_session_state->datafd);

			stop_streaming(&sensor_list[i], p_session_state);

			if (p_session_state == sensor_list[i].list)
				sensor_list[i].list = p_session_state->next;
			else
				p->next = p_session_state->next;

#ifdef ENABLE_CONTEXT_ARBITOR
			ctx_option_t *out_option = NULL;
			int j;

			if (ctx_close_session(p_session_state->handle, &out_option) == 1) {
				if (out_option != NULL) {
					for (j = 0; j < out_option->len; ++j) {
						send_set_property(sensor_list + i, p_session_state, out_option->items[j].prop,
							out_option->items[j].size, (unsigned char *)out_option->items[j].value);
					}

					ctx_option_release(out_option);
				}
			}
#endif

			log_message(DEBUG, "session with datafd %d, ctlfd %d "
				"is removed \n", p_session_state->datafd,
				p_session_state->ctlfd);
			free(p_session_state);
			return;
		}
        }
}

int add_platform_fds(int maxfd, void *read_fds, int *hw_fds, int *hw_fds_num)
{
	int i, j;
	int ret = 0;

	i = sizeof(ish_platf) / sizeof(ish_platform_t);
	for (j = 0; j < i; j++) {
		if (ish_platf[j].add_fds) {
			ret = ish_platf[j].add_fds(maxfd, read_fds, hw_fds, hw_fds_num);

			if (ret > maxfd)
				maxfd = ret;
		}
	}

	return ret;
}

void process_platform_fd(int fd)
{
	int i, j;

	i = sizeof(ish_platf) / sizeof(ish_platform_t);
	for (j = 0; j < i; j++)
		if (ish_platf[j].process_fd)
			ish_platf[j].process_fd(fd);
}

/* 1 create data thread
   2 wait and handle the request from client in main thread */
static void start_sensorhubd()
{
	fd_set listen_fds, read_fds;
	pthread_t t;
	int fixed_maxfd;

	log_message(DEBUG, "[%s] enter\n", __func__);

	/* two fd_set for I/O multiplexing */
	FD_ZERO(&listen_fds);
	FD_ZERO(&read_fds);

	FD_SET(sockfd, &listen_fds);
	fixed_maxfd = sockfd;

	while (1) {
		int hw_fds[255], hw_fds_num;
		int maxfd;
		int i;

		read_fds = listen_fds;
		maxfd = fixed_maxfd;
		hw_fds_num = 0;
		maxfd = add_platform_fds(maxfd, &read_fds, hw_fds, &hw_fds_num);
		if (maxfd <= 0)
			continue;

		if (select(maxfd + 1, &read_fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			else {
				log_message(CRITICAL, "sensorhubd socket "
					"select() failed. errno "
					"is %d\n", errno);
				exit(EXIT_FAILURE);
			}
		}

		/* handle events from hw */
		for (i = 0; i < hw_fds_num; i++) {
			if (FD_ISSET(hw_fds[i], &read_fds)) {
				process_platform_fd(hw_fds[i]);

				FD_CLR(hw_fds[i], &read_fds);
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
					"accept() failed.\n");
				exit(EXIT_FAILURE);
			} else {
				log_message(DEBUG, "new connection from client adding FD = %d\n", clientfd);
				FD_SET(clientfd, &listen_fds);
				if (clientfd > maxfd)
					fixed_maxfd = clientfd;
			}

			FD_CLR(sockfd, &read_fds);
		}

		/* handle request from clients */
		for (i = maxfd; i >= 0; i--) {
			char message[MAX_MESSAGE_LENGTH];

			if (!FD_ISSET(i, &read_fds))
				continue;

			int length = recv(i, message, MAX_MESSAGE_LENGTH, 0);
			if (length <= 0) {
				/* release session resource if necessary */
				remove_session_by_fd(i);
				close(i);
				log_message(WARNING, "fd %d:error reading message \n", i);
				FD_CLR(i, &listen_fds);
			} else {
				/* process message */
				handle_message(i, message);
			}
			FD_CLR(i, &read_fds);
		}

	}
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
	int log_level = WARNING;

	set_log_file(log_file);
	set_log_level((message_level)log_level);

	/* Ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	memset(flush_completion_frame, 0xff, sizeof(flush_completion_frame));

	while (1) {
		static struct option opts[] = {
			{"log-level", 2, NULL, 'l'},
			{"help", 0, NULL, 'h'},
			{0, 0, NULL, 0}
		};

		int index, o;

		o = getopt_long(argc, argv, "d:l::h", opts, &index);
		if (o == -1)
			break;

		switch (o) {
		case 'l':
			if (optarg != NULL)
				log_level = strtod(optarg, NULL);
			log_message(DEBUG, "log_level is %d \n", log_level);
			set_log_level((message_level) log_level);
			break;
		case 'h':
			usage();
			break;
		default:
			usage();
		}
	}

	while (1) {
		reset_sensorhub();
		start_sensorhubd();
	}
	return 0;
}
