#ifndef _LIBSENSORHUB_H
#define _LIBSENSORHUB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
        SENSOR_INVALID = -1,
	SENSOR_ACCELEROMETER = 0,
	SENSOR_GYRO,
	SENSOR_COMP,
	SENSOR_BARO,
	SENSOR_ALS,
	SENSOR_PROXIMITY,
	SENSOR_TC,
	SENSOR_LPE,
	SENSOR_ACCELEROMETER_SEC,
	SENSOR_GYRO_SEC,
	SENSOR_COMP_SEC,
	SENSOR_ALS_SEC,
	SENSOR_PROXIMITY_SEC,
	SENSOR_BARO_SEC,

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
	SENSOR_SHAKING,
	SENSOR_MOVE_DETECT,
	SENSOR_STAP,
	SENSOR_PAN_TILT_ZOOM,
	SENSOR_LIFT_VERTICAL,
	SENSOR_DEVICE_POSITION,
	SENSOR_STEPCOUNTER,
	SENSOR_STEPDETECTOR,
	SENSOR_SIGNIFICANT_MOTION,
	SENSOR_GAME_ROTATION_VECTOR,
	SENSOR_GEOMAGNETIC_ROTATION_VECTOR,
	SENSOR_6DOFAG,
	SENSOR_6DOFAM,
	SENSOR_LIFT,
	SENSOR_DTWGS,
	SENSOR_GESTURE_HMM,
	SENSOR_GESTURE_EARTOUCH,

	SENSOR_PEDESTRIAN_DEAD_RECKONING,
	SENSOR_INSTANT_ACTIVITY,
	SENSOR_UNCAL_COMP,
	SENSOR_UNCAL_GYRO,
	SENSOR_UNCAL_ACC,
	SENSOR_MOTION_DETECT,
	SENSOR_TILT_DETECT,

	SENSOR_BIST,

	SENSOR_MAX
} ish_sensor_t;

typedef enum {
	USE_CASE_HAL = 0,	// only can be used for android sensor hal
	USE_CASE_CSP,		// not only for sensor hal, but also for csp
	USE_CASE_HUB		// all sensor hub
} ish_usecase_t;

#define SNR_NAME_MAX_LEN	5
#define VEND_NAME_MAX_LEN	20
#define MAX_AXIS		6
#define MS_TO_US		1000
#define DEFAULT_MIN_DELAY	10000	// min dalay default is 10ms, 100HZ, unit is microsecond
#define DEFAULT_MAX_DELAY	(0x7fffffff)	// max delay default is max
#define DEFAULT_FIFO_MAX	200	// default fifo set to 0, before batch mode enable in fw
#define DEFAULT_FIFO_RESERVED	0	// default to 0
typedef struct {
	char name[SNR_NAME_MAX_LEN + 1];	// sensor name
	char vendor[VEND_NAME_MAX_LEN + 1];	// sensor vendor's name
	ish_sensor_t sensor_type;		// sensor type, ACCEL, COMPSS, GYRO ...
	ish_usecase_t use_case;
	unsigned char is_wake_sensor;		// sensor wake mode: 0- non-wake sensor; 1 - wake sensor
	unsigned int version;			// sensor version
	unsigned int min_delay;			// sensor's minimum delay
	unsigned int max_delay;			// sensor's maximum delay
	unsigned int fifo_max_event_count;	// sensor's fifo count for batch mode
	unsigned int fifo_reserved_event_count;	// sensos's fifo reserved for batch mode
	unsigned int axis_num;			// sensor's axises
	float axis_scale[MAX_AXIS];		// sensor's individual axis scale
	float max_range;			// sensor's maximum value range
	float resolution;			// sensor's resolution
	float power;				// senosr's power
	void *plat_data;			// Sensor's private data which related to different platform
} sensor_info_t;

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

typedef enum {
	PROP_GENERIC_START = 0,
	PROP_STOP_REPORTING = 1,
	PROP_SENSITIVITY,
	PROP_GENERIC_END = 20,

	PROP_PEDOMETER_START = 20,
	PROP_PEDOMETER_MODE = 21,
	PROP_PEDOMETER_N = 22,
	PROP_PEDOMETER_END = 40,

	PROP_ACT_START = 40,
	PROP_ACT_MODE,
	PROP_ACT_CLSMASK,
	PROP_ACT_N,
	PROP_ACT_DUTY_M,
	PROP_ACT_END = 60,

	PROP_GFLICK_START = 60,
	PROP_GFLICK_CLSMASK,
	PROP_GFLICK_LEVEL,
	PROP_GFLICK_END = 80,

	PROP_SHAKING_START = 80,
	PROP_SHAKING_SENSITIVITY,
	PROP_SHAKING_END = 100,

	PROP_STAP_START = 100,
	PROP_STAP_CLSMASK,
	PROP_STAP_LEVEL,
	PROP_STAP_END = 120,

	PROP_DTWGSM_START = 120,
	PROP_DTWGSM_LEVEL,
	PROP_DTWGSM_DST,
	PROP_DTWGSM_TEMPLATE,
	PROP_DTWGSM_END = 140,

	PROP_PEDOPLUS_START = 140,
	PROP_PEDOPLUS_PIAMODE,
	PROP_PEDOPLUS_SCOUNTERMODE,
	PROP_PEDOPLUS_NSC,
	PROP_PEDOPLUS_NPIA,
	PROP_PEDOPLUS_ADMISSION,
	PROP_PEDOPLUS_CLSMASK,
	PROP_PEDOPLUS_END = 160,

	PROP_EARTOUCH_START = 160,
	PROP_EARTOUCH_CLSMASK,
	PROP_EARTOUCH_END = 180,
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
	PROP_PEDOPLUS_MODE_NONCHANGE,
	PROP_PEDOPLUS_MODE_ONCHANGE,
} property_pedoplus_mode;

typedef enum {
	STOP_WHEN_SCREEN_OFF = 0,
	NO_STOP_WHEN_SCREEN_OFF = 1,
	NO_STOP_NO_REPORT_WHEN_SCREEN_OFF = 2,
} streaming_flag;

typedef void * handle_t;

/* return NULL if failed */
handle_t ish_open_session(ish_sensor_t sensor_type);
handle_t ish_open_session_with_name(const char *name);

void ish_close_session(handle_t handle);

/* return 0 when success
   data_rate: the unit is HZ;
   buffer_delay: the unit is ms. It's used to tell ish that application
   can't wait more than 'buffer_delay' ms to get data. In another word
   data can arrive before 'buffer_delay' ms elaps.
   So every time the data is returned, the data size may vary and the
   application need to buffer the data by itself */
error_t ish_start_streaming(handle_t handle, int data_rate, int buffer_delay);

/* flag: 2 means no_stop_no_report when screen off; 1 means no_stop when screen off; 0 means stop when screen off */
error_t ish_start_streaming_with_flag(handle_t handle, int data_rate, int buffer_delay, streaming_flag flag);

#define MAX_UNIT_SIZE 128
#define FLUSH_CMPL_FLAG 0xFFFFFFFF
error_t ish_flush_streaming(handle_t handle, unsigned int data_unit_size);

error_t ish_stop_streaming(handle_t handle);

/* return -1 if failed */
int ish_get_fd(handle_t handle);

/* set properties */
error_t ish_set_property(handle_t handle, property_type prop_type, void *value);
error_t ish_set_property_with_size(handle_t handle, property_type prop_type, int size, void *value);

error_t get_sensors_list(ish_usecase_t usecase, void *buf, int *sensor_num);

/* data format of each sensor type */
struct accel_data {
	int64_t ts;
	int x;
	int y;
	int z;
	unsigned char motion;
} __attribute__ ((packed));

struct gyro_raw_data {
	int64_t ts;
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct compass_raw_data {
	int64_t ts;
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct tc_data {
	int64_t ts;
	unsigned char state;
} __attribute__ ((packed));

struct baro_raw_data {
	int64_t ts;
	int p;
} __attribute__ ((packed));

struct als_raw_data {
	int64_t ts;
	unsigned int lux;
} __attribute__ ((packed));

struct phy_activity_data {
	int64_t ts;
	int values[7];
} __attribute__ ((packed));

struct gs_data {
	int64_t ts;
	unsigned short size; //unit is byte
	short sample[0];
} __attribute__ ((packed));

struct gesture_hmm_data {
	int64_t ts;
	short prox_gesture; //proximity if not use context arbiter; gesture if use context arbiter
	unsigned short size; //unit is byte
	short sample[0];
} __attribute__ ((packed));

struct pdr_data {
	int64_t ts;
	int x;
	int y;
	char floor;
	int heading;
	unsigned int step;
	unsigned int distance;
	unsigned int speed;
	unsigned int pos_error;
	unsigned int heading_conf;
} __attribute__ ((packed));

struct gesture_eartouch_data {
	int64_t ts;
	short eartouch;
} __attribute__ ((packed));

struct ps_phy_data {
	int64_t ts;
	char near;
} __attribute__ ((packed));

struct gesture_flick_data {
	int64_t ts;
	unsigned char flick;
} __attribute__ ((packed));

struct shaking_data {
	int64_t ts;
	unsigned char shaking;
} __attribute__ ((packed));

struct stap_data {
	int64_t ts;
	unsigned char stap;
} __attribute__ ((packed));

struct pz_data {
	int64_t ts;
	int deltX;
	int deltY;		/* deltX and deltY: 0.01deg/s */
}__attribute__ ((packed));

struct rotation_vector_data {
	int64_t ts;
        short x;
        short y;
        short z;
        short w;
	unsigned char mag_accuracy;
	unsigned char mag_err_rad;
} __attribute__ ((packed));

struct game_rotation_vector_data {
	int64_t ts;
	short x;
	short y;
	short z;
	short w;
} __attribute__ ((packed));

struct geomagnetic_rotation_vector_data {
	int64_t ts;
        short x;
        short y;
        short z;
        short w;
} __attribute__ ((packed));

struct gravity_data {
	int64_t ts;
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct linear_accel_data {
	int64_t ts;
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct uncalib_gyro_data {
	int64_t ts;
        int x_uncalib;
	int x_calib;
        int y_uncalib;
	int y_calib;
        int z_uncalib;
	int z_calib;
} __attribute__ ((packed));

struct uncalib_compass_data {
	int64_t ts;
        int x_uncalib;
        int x_calib;
        int y_uncalib;
        int y_calib;
        int z_uncalib;
        int z_calib;
} __attribute__ ((packed));

struct uncalib_acc_data {
	int64_t ts;
        int x_uncalib;
        int y_uncalib;
        int z_uncalib;
} __attribute__ ((packed));

struct motion_detect_data {
	int64_t ts;
	char eventData1;
	char eventData2;
	char eventData3;
	char eventData4;
	char eventData5;
} __attribute__ ((packed));

struct orientation_data {
	int64_t ts;
	int tiltx;
	int tilty;
	int tiltz;
	unsigned char mag_accuracy;
	unsigned char mag_err_rad;
} __attribute__ ((packed));

struct device_orientation_data {
	int64_t ts;
	int w;
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct ndof_data {
	int	m[9];
} __attribute__ ((packed));

struct ndofag_data {
	int     m[9];
} __attribute__ ((packed));

struct ndofam_data {
	int     m[9];
} __attribute__ ((packed));

struct pedometer_data {
	int num;
	short mode;
	int vec[0];
} __attribute__ ((packed));

struct mag_heading_data {
	int heading;
	int x;
	int y;
	int z;
} __attribute__ ((packed));

struct lpe_phy_data {
	unsigned int lpe_msg;
} __attribute__ ((packed));

#define MD_STATE_UNCHANGE 0
#define MD_STATE_MOVE 1
#define MD_STATE_STILL 2

struct md_data {
	int64_t ts;
	short state;
} __attribute__ ((packed));

struct device_position_data {
	int64_t ts;
	short pos;
} __attribute__ ((packed));

struct sm_data {
	int64_t ts;
	unsigned char state;
} __attribute__ ((packed));

struct stepcounter_data {
	int64_t ts;
	unsigned char reset_flag;
	int32_t walk_step_count;
	int32_t walk_step_duration;
	int32_t run_step_count;
	int32_t run_step_duration;
} __attribute__ ((packed));

struct stepdetector_data {
	int64_t ts;
	int32_t step_event_counter;
	unsigned char step_type;
	int32_t step_count;
	int32_t step_duration;
} __attribute__ ((packed));

struct instant_activity_data {
	int64_t ts;
	unsigned char typeclass;
} __attribute__ ((packed));

struct lift_data {
	int64_t ts;
	char look;
	char vertical;
} __attribute__ ((packed));

struct dtwgs_data {
	int64_t ts;
	short gsnum;
	int score;
} __attribute__ ((packed));

struct tilt_data {
	int64_t ts;
	char tilt_event;
} __attribute__ ((packed));

#ifdef __cplusplus
}
#endif

#endif /* libsensorhub.h */
