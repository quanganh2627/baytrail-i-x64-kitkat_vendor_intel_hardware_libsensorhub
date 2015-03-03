#ifndef _MESSAGE_H
#define _MESSAGE_H

#include "libsensorhub.h"

typedef unsigned int session_id_t;

struct sensor_name {
        char name[SNR_NAME_MAX_LEN + 1];
};

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
	int datafd;
	int ctlfd;
	unsigned char trans_id;
	session_id_t session_id;
	unsigned int data_rate;
	unsigned int buffer_delay;
	void *handle;
	struct session_state_t *next;
} session_state_t;

/* data structure to keep state for a particular type of sensor */
typedef struct {
	sensor_info_t *sensor_info;	// Basic senor information
	unsigned int index;		// current index in sensor_list[]
	unsigned int data_rate;		// Current sensor streaming data rate
	unsigned int buffer_delay;	// Current sensor buffer_dealy
	session_state_t *list;		// Sensor's clients list
} sensor_state_t;

typedef enum {
	EVENT_HELLO_WITH_SENSOR_TYPE = 0,
	EVENT_HELLO_WITH_SENSOR_TYPE_ACK,
	EVENT_HELLO_WITH_SESSION_ID,
	EVENT_HELLO_WITH_SESSION_ID_ACK,
	EVENT_CMD,
	EVENT_CMD_ACK,
	EVENT_DATA
} event_t;

typedef enum {
	SUCCESS = 0,
	ERR_SENSOR_NOT_SUPPORT = -1,
	ERR_SESSION_NOT_EXIST = -2,
	ERR_SENSOR_NO_RESPONSE = -3,
	ERR_CMD_NOT_SUPPORT = -4,
	ERR_DATA_RATE_NOT_SUPPORT = -5,
	ERR_NO_MEMORY = -6,
	ERR_WRONG_PARAMETER = -7
} ret_t;

static struct sensor_name sensor_type_to_name_str[SENSOR_MAX] = {
	{"ACCEL"}, {"GYRO"}, {"COMPS"}, {"BARO"}, {"ALS_P"}, {"PS_P"}, {"TERMC"}, {"LPE_P"},
	{"ACC1"}, {"GYRO1"}, {"COMP1"}, {"ALS1"}, {"PS1"}, {"BARO1"}, {"PHYAC"}, {"GSSPT"},
	{"GSFLK"}, {"RVECT"}, {"GRAVI"}, {"LACCL"}, {"ORIEN"}, {"COMPC"}, {"GYROC"}, {"9DOF"},
	{"PEDOM"}, {"MAGHD"}, {"SHAKI"}, {"MOVDT"}, {"STAP"}, {"PZOOM"}, {"LTVTL"}, {"DVPOS"},
	{"SCOUN"}, {"SDET"}, {"SIGMT"}, {"6AGRV"}, {"6AMRV"}, {"6DOFG"}, {"6DOFM"}, {"LIFT"},
	{"DTWGS"}, {"GSPX"}, {"GSETH"}, {"PDR"}, {"ISACT"},
	{"UCMPS"}, {"UGYRO"}, {"UACC"}, {"MOTDT"},
	{"BIST"}
};

typedef struct {
	event_t event_type;
	char name[SNR_NAME_MAX_LEN + 1];
} hello_with_sensor_type_event;

typedef struct {
	event_t event_type;
	session_id_t session_id;
} hello_with_sensor_type_ack_event;

typedef struct {
	event_t event_type;
	session_id_t session_id;
} hello_with_session_id_event;

typedef struct {
	event_t event_type;
	ret_t ret;
} hello_with_session_id_ack_event;

typedef struct {
	event_t event_type;
	ret_t ret;
	int buf_len;
	unsigned char buf[];
} cmd_ack_event;

typedef struct {
	event_t event_type;
	int payload_size;
	char payload[];
} data_event;

typedef enum {
	CMD_START_STREAMING,
	CMD_STOP_STREAMING,
	CMD_SET_PROPERTY,
	CMD_GET_PROPERTY,
	CMD_FLUSH_STREAMING,
	CMD_MAX
} cmd_t;

typedef struct {
	event_t event_type;
	cmd_t cmd;
	int parameter;
	int parameter1;
	int parameter2;
	unsigned char buf[];
} cmd_event;

#define MAX_PROP_VALUE_LEN	4
struct cmd_send {
	unsigned char tran_id;
	unsigned char cmd_id;
	ish_sensor_t sensor_type;
	union {
		struct {
			unsigned short data_rate;
			unsigned short buffer_delay;
			unsigned short bit_cfg;
		} start_stream;
		struct {
			property_type prop_type;
			unsigned int len;
			unsigned char value[MAX_PROP_VALUE_LEN];
		} set_prop;
	};
} __attribute__ ((packed));

struct cmd_resp {
	unsigned char tran_id;
	unsigned char cmd_type;
	ish_sensor_t sensor_type;
	unsigned short data_len;
	char buf[0];
} __attribute__ ((packed));

enum resp_type {
	RESP_CMD_ACK,
	RESP_GET_TIME,
	RESP_STREAMING,
};

#define MAX_SENSOR_INDEX 50

#endif
