#ifndef _LIBSENSORHUB_H
#define _LIBSENSORHUB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SENSOR_ACCELEROMETER = 0,
	SENSOR_GYRO,
	SENSOR_COMP,
	SENSOR_BARO,
	SENSOR_ALS,
	SENSOR_PROXIMITY,

	SENSOR_TC,
	SENSOR_ACTIVITY,
	SENSOR_GS,
	SENSOR_GESTURE_FLICK,

	SENSOR_ROTATION_VECTOR,
	SENSOR_GRAVITY,
	SENSOR_LINEAR_ACCEL,
	SENSOR_ORIENTATION,
	SENSOR_CALIBRATION_COMP,
	SENSOR_CALIBRATION_GYRO,
	SENSOR_9DOF,
	SENSOR_PEDOMETER,
	SENSOR_MAG_HEADING,

	SENSOR_EVENT,
	SENSOR_MAX
} psh_sensor_t;

typedef enum {
	ERROR_NONE = 0,
	ERROR_DATA_RATE_NOT_SUPPORTED = -1,
	ERROR_NOT_AVAILABLE = -2,
	ERROR_MESSAGE_NOT_SENT = -3,
	ERROR_CAN_NOT_GET_REPLY = -4,
	ERROR_WRONG_ACTION_ON_SENSOR_TYPE = -5,
	ERROR_WRONG_PARAMETER = -6,
	ERROR_PROPERTY_NOT_SUPPORTED = -7
} error_t;

enum {
	OP_EQUAL = 0,
	OP_GREAT,
	OP_LESS,
	OP_BETWEEN,
	OP_MAX
};

typedef enum {
	PROP_GENERIC_START = 0,
	PROP_STOP_REPORTING = 1,
	PROP_GENERIC_END = 20,

	PROP_PEDOMETER_START = 20,
	PROP_PEDOMETER_MODE = 21,
	PROP_PEDOMETER_N = 22,
	PROP_PEDOMETER_END = 40,

	PROP_ACT_START = 40,
	PROP_ACT_MODE,
	PROP_ACT_CLSMASK,
	PROP_ACT_N,
	PROP_ACT_END = 60,

	PROP_GFLICK_START = 60,
	PROP_GFLICK_CLSMASK,
	PROP_GFLICK_SENSITIVITY,
	PROP_GFLICK_END = 80,
} property_type;

typedef enum {
	PROP_PEDO_MODE_NCYCLE,
	PROP_PEDO_MODE_ONCHANGE,
} property_pedo_mode;

typedef enum {
	PROP_ACT_MODE_NCYCLE,
	PROP_ACT_MODE_ONCHANGE
} property_act_mode;

typedef enum {
	STOP_WHEN_SCREEN_OFF = 0,
	NO_STOP_WHEN_SCREEN_OFF = 1,
	NO_STOP_NO_REPORT_WHEN_SCREEN_OFF = 2,
} streaming_flag;

typedef enum {
	AND = 0,
	OR = 1,
} relation;

typedef void * handle_t;

/* return NULL if failed */
handle_t psh_open_session(psh_sensor_t sensor_type);

void psh_close_session(handle_t handle);

/* return 0 when success
   data_rate: the unit is HZ;
   buffer_delay: the unit is ms. It's used to tell psh that application
   can't wait more than 'buffer_delay' ms to get data. In another word
   data can arrive before 'buffer_delay' ms elaps.
   So every time the data is returned, the data size may vary and the
   application need to buffer the data by itself */
error_t psh_start_streaming(handle_t handle, int data_rate, int buffer_delay);

/* flag: 2 means no_stop_no_report when screen off; 1 means no_stop when screen off; 0 means stop when screen off */
error_t psh_start_streaming_with_flag(handle_t handle, int data_rate, int buffer_delay, streaming_flag flag);

error_t psh_stop_streaming(handle_t handle);

/* return data size if success; error_t if failure */
error_t psh_get_single(handle_t handle, void *buf, int buf_size);

struct cmd_calibration_param;
/* set the calibration data of compass sensor
   If param not NULL, it will set calibration parameters.  */
error_t psh_set_calibration(handle_t handle, struct cmd_calibration_param * param);

error_t psh_get_calibration(handle_t handle, struct cmd_calibration_param * param);
/* return -1 if failed */
int psh_get_fd(handle_t handle);

/* relation: 0, AND; 1, OR; default is AND */
error_t psh_event_set_relation(handle_t handle, relation relation);

struct sub_event;
error_t psh_event_append(handle_t handle, struct sub_event *sub_evt);

error_t psh_add_event(handle_t handle);

error_t psh_clear_event(handle_t handle);

/* 0 means psh_add_event() has not been called */
short psh_get_event_id(handle_t handle);

/* set properties */
error_t psh_set_property(handle_t handle, property_type prop_type, void *value);

/* data format of each sensor type */
struct accel_data {
	short x;
	short y;
	short z;
} __attribute__ ((packed));

struct gyro_raw_data {
	short x;
	short y;
	short z;
	short accuracy;
} __attribute__ ((packed));

struct compass_raw_data {
	short x;
	short y;
	short z;
	short accuracy;		/* high or low */
} __attribute__ ((packed));

struct tc_data {
	short orien_xy;
	short orien_z;
} __attribute__ ((packed));

struct baro_raw_data {
	int p;
} __attribute__ ((packed));

struct als_raw_data {
	unsigned short lux;
} __attribute__ ((packed));

struct phy_activity_data {
	short len;
	short values[32];
} __attribute__ ((packed));

struct gs_data {
	unsigned short size; //unit is byte
	short sample[0];
} __attribute__ ((packed));

struct ps_phy_data {
	unsigned short near;
} __attribute__ ((packed));

struct gesture_flick_data {
	short flick;
} __attribute__ ((packed));

struct rotation_vector_data {
	int x;
	int y;
	int z;
	int w;
} __attribute__ ((packed));

struct gravity_data {
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct linear_accel_data {
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct orientation_data {
	int azimuth;
	int pitch;
	int roll;
} __attribute__ ((packed));

struct compasscal_info {
	int minx;
	int maxx;
	int miny;
	int maxy;
	int minz;
	int maxz;
} __attribute__ ((packed));

struct gyrocal_info {
	short x;
	short y;
	short z;
} __attribute__ ((packed));

#define SUBCMD_CALIBRATION_SET		(0x1)
#define SUBCMD_CALIBRATION_GET		(0x2)
#define SUBCMD_CALIBRATION_START	(0x3)
#define SUBCMD_CALIBRATION_STOP		(0x4)

#define SUBCMD_CALIBRATION_TRUE		(0x1)
#define SUBCMD_CALIBRATION_FALSE 	(0x0)
struct cmd_calibration_param {
	psh_sensor_t sensor_type;
	unsigned char sub_cmd;
	unsigned char calibrated;
	union {
		struct compasscal_info compass;
		struct gyrocal_info gyro;
	} cal_param;
} __attribute__ ((packed));

struct sub_event {
	unsigned char sensor_id;
	unsigned char chan_id;		/* 1:x, 2:y, 4:z, 7:all */
	unsigned char opt_id;		/* 0:OP_EQUAL, 1:OP_GREAT, 2:OP_LESS, 3:OP_BETWEEN */
	int param1;
	int param2;
} __attribute__ ((packed));

struct event_notification_data {
	unsigned char event_id;
} __attribute__ ((packed));

struct ndof_data {
	int	m[9];
} __attribute__ ((packed));

struct pedometer_data {
	int num;
	short mode;
	int vec[0];
} __attribute__ ((packed));

struct mag_heading_data {
	int heading;
} __attribute__ ((packed));

#ifdef __cplusplus
}
#endif

#endif /* libsensorhub.h */