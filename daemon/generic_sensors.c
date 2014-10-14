#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
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
#include "generic_sensors.h"

#define SENSOR_UP_FILE			"/data/sensorsUp"

#define SENSOR_EVENT_PATH		"/dev/sensor-collection"

#define SENSOR_COL_PATH			"/sys/bus/platform/devices/sensor_collection/"

#define SENSOR_DATA_PATH		SENSOR_COL_PATH "/data/sensors_data"

#define SENSOR_DIR_PATH			SENSOR_COL_PATH "sensor_%X_def/"

#define SENSOR_NAME_PATH		SENSOR_DIR_PATH "name"

#define SENSOR_PROP_PATH		SENSOR_DIR_PATH "properties/"
#define SENSOR_DESCP_PATH		SENSOR_DIR_PATH "properties/property_sensor_description/value"

#define SENSOR_DATA_FIELD_PATH		SENSOR_DIR_PATH "data_fields/"
#define DATA_FIELD_USAGE_ID_FILE	"usage_id"
#define DATA_FIELD_INDEX_FILE		"index"
#define DATA_FIELD_LENGTH_FILE		"length"


#define SENSOR_DIR_PREFIX		"sensor_"
#define SENSOR_DIR_SUFFIX		"_def"

#define CUSTOM_SENSOR_NAME		"type_other_custom"

#define MAX_PATH_LEN			256
#define MAX_VALUE_LEN			64

generic_sensor_info_t g_info[] = {
	/* index 0 accelerometer */
	[0] = {
		.friend_name = "type_motion_accelerometer_3d",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_ACCELERATION_X_AXIS,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_ACCELERATION_Y_AXIS,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_ACCELERATION_Z_AXIS,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 1 gyrometer*/
	[1] = {
		.friend_name = "type_motion_gyrometer_3d",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_GYROMETER_X_AXIS,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_GYROMETER_Y_AXIS,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_GYROMETER_Z_AXIS,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 2 compass*/
	[2] = {
		.friend_name = "type_orientation_compass_3d",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_MAGNETIC_FLUX_X_AXIS,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_MAGNETIC_FLUX_Y_AXIS,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_MAGNETIC_FLUX_Z_AXIS,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 3 light*/
	[3] = {
		.friend_name = "type_light_ambientlight",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
	/* index 4 barometer*/
	[4] = {
		.friend_name = "type_environmental_atmospheric_pressure",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_ENVIRONMENT_ATMOSPHERIC_PRESSURE,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
	/* index 5 proximity*/
	[5] = {
		.friend_name = "type_biometric_presence",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_BIOMETRIC_HUMANCE_PRESENCE,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
	/* index 6 -RVECT- orientation*/
	[6] = {
		.friend_name = "type_orientation_device_orientation",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_QUATERNION,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
	/* index 7 -LACCL- linear accelerometer */
	[7] = {
		.friend_name = "0x202",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 8 -GRAVI- gravity*/
	[8] = {
		.friend_name = "0x209",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 9 -MOTDT- motion detect*/
	[9] = {
		.friend_name = "0x204",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 5,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 6,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 7,
		},
		.data_field[5] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_5,
			.exposed = 1,
			.exposed_offset = 8,
		},
	},
	/* index 10 -6AMRV- GEOMAGNETIC_ROTATION_VECTOR*/
	[10] = {
		.friend_name = "0x201",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 6,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 10,
		},
	},
	/* index 11 -SDET- step detector */
	[11] = {
		.friend_name = "0x234",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 5,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 6,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 10,
		},
	},
	/* index 12 -SCOUN- step counter */
	[12] = {
		.friend_name = "0x230",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 5,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 9,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 13,
		},
		.data_field[5] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_5,
			.exposed = 1,
			.exposed_offset = 17,
		},
	},
	/* index 13 -6AGRV- game rotation vector */
	[13] = {
		.friend_name = "0x200",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 6,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 10,
		},
	},
	/* index 14 -- */
	[14] = {
		.friend_name = "null",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 15 -UACC- uncalibrated accelerometer */
	[15] = {
		.friend_name = "0x73",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 12,
		},
	},
	/* index 16 -UGYRO- uncalibrated gyrometer */
	[16] = {
		.friend_name = "0x241",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 12,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 16,
		},
		.data_field[5] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_5,
			.exposed = 1,
			.exposed_offset = 20,
		},
		.data_field[6] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_6,
			.exposed = 1,
			.exposed_offset = 24,
		},
	},
	/* index 17 -UCMPS- uncalibrated compass */
	[17] = {
		.friend_name = "0x242",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 12,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 16,
		},
		.data_field[5] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_5,
			.exposed = 1,
			.exposed_offset = 20,
		},
		.data_field[6] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_6,
			.exposed = 1,
			.exposed_offset = 24,
		},
	},
	/* index 18 - STAP */
	[18] = {
		.friend_name = "0x210",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
	/* index 19 - LIFT */
	[19] = {
		.friend_name = "0x211",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 5,
		},
	},
	/* index 20 - PZOOM */
	[20] = {
		.friend_name = "0x212",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 6,
		},
	},
	/* index 21 - Physical Activity */
	[21] = {
		.friend_name = "0x232",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_2,
			.exposed = 1,
			.exposed_offset = 5,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_3,
			.exposed = 1,
			.exposed_offset = 6,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_4,
			.exposed = 1,
			.exposed_offset = 7,
		},
		.data_field[5] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_5,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[6] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_6,
			.exposed = 1,
			.exposed_offset = 9,
		},
		.data_field[7] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_7,
			.exposed = 1,
			.exposed_offset = 10,
		},
		.data_field[8] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_8,
			.exposed = 1,
			.exposed_offset = 11,
		},
		.data_field[9] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_9,
			.exposed = 1,
			.exposed_offset = 12,
		},
		.data_field[10] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_10,
			.exposed = 1,
			.exposed_offset = 13,
		},
		.data_field[11] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_11,
			.exposed = 1,
			.exposed_offset = 14,
		},
		.data_field[12] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_12,
			.exposed = 1,
			.exposed_offset = 15,
		},
		.data_field[13] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_13,
			.exposed = 1,
			.exposed_offset = 16,
		},
		.data_field[14] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_14,
			.exposed = 1,
			.exposed_offset = 17,
		},
	},
	/* index 22 - Instant Activity */
	[22] = {
		.friend_name = "0x237",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
	/* index 23 - Significant Motion */
	[23] = {
		.friend_name = "0x236",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 4,
		},
	},
};

sensor_info_t g_sensor_info[] = {
	[0] = {
		.name = "ACCEL",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_ACCELEROMETER,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.001, 0.001, 0.001},
		.max_range = 2,
		.resolution = 0.000488281,
		.power = 0.006,
		.plat_data = &g_info[0],
	},
	[1] = {
		.name = "ALS_P",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_ALS,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {0.1},
		.max_range = 6553.5,
		.resolution = 0.1,
		.power = 0.35,
		.plat_data = &g_info[3],
	},
	[2] = {
		.name = "PS_P",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_PROXIMITY,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {5.0},
		.max_range = 5.0,
		.resolution = 5.0,
		.power = 0.35,
		.plat_data = &g_info[5],
	},
	[3] = {
		.name = "COMPS",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_COMP,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.1, 0.1, 0.1},
		.max_range = 800.0,
		.resolution = 0.5,
		.power = 0.35,
		.plat_data = &g_info[2],
	},
	[4] = {
		.name = "GYRO",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GYRO,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.000555556, 0.000555556, 0.000555556},
		.max_range = 11.111111,
		.resolution = 0.000555556,
		.power = 6.1,
		.plat_data = &g_info[1],
	},
	[5] = {
		.name = "BARO",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_BARO,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 2,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[4],
	},
	[6] = {
		.name = "GRAVI",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GRAVITY,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.0000000152587890625, 0.0000000152587890625, 0.0000000152587890625},
		.max_range = 2,
		.resolution = 0.000488281,
		.power = 0.006,
		.plat_data = &g_info[8],
	},
	[7] = {
		.name = "LACCL",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_LINEAR_ACCEL,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.001, 0.001, 0.001},
		.max_range = 2,
		.resolution = 0.000488281,
		.power = 0.006,
		.plat_data = &g_info[7],
	},
	[8] = {
		.name = "RVECT",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_ROTATION_VECTOR,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.0000152587890625, 0.0000152587890625, 0.0000152587890625},
		.max_range = 1,
		.resolution = 0.0000000596046,
		.power = 0.106,
		.plat_data = &g_info[6],
	},
	[9] = {
		.name = "ORIEN",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_ORIENTATION,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.0000152587890625, 0.0000152587890625, 0.0000152587890625},
		.max_range = 360,
		.resolution = 1.0,
		.power = 0.106,
		.plat_data = NULL,
	},
	[10] = {
		.name = "SDET",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_STEPDETECTOR,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 2,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[11],
	},
	[11] = {
		.name = "SCOUN",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_STEPCOUNTER,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 2,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[12],
	},
	[12] = {
		.name = "6AGRV",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GAME_ROTATION_VECTOR,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 4,
		.axis_scale = {1, 1, 1, 1},
		.max_range = 2,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[13],
	},
	[13] = {
		.name = "6AMRV",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GEOMAGNETIC_ROTATION_VECTOR,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 4,
		.axis_scale = {1, 1, 1, 1},
		.max_range = 2,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[10],
	},
	[14] = {
		.name = "STAP",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_STAP,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[18],
	},
	[15] = {
		.name = "LIFT",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_LIFT,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.min_delay = 0,
		.axis_num = 2,
		.axis_scale = {1, 1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[19],
	},
	[16] = {
		.name = "PZOOM",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_PAN_TILT_ZOOM,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.min_delay = 0,
		.axis_num = 2,
		.axis_scale = {1, 1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[20],
	},
	[17] = {
		.name = "PHYAC",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_ACTIVITY,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.min_delay = 0,
		.axis_num = 2,
		.axis_scale = {1, 1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[21],
	},
	[18] = {
		.name = "ISACT",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_INSTANT_ACTIVITY,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[22],
	},
	[19] = {
		.name = "SIGMT",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_SIGNIFICANT_MOTION,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 0,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[23],
	},
	[20] = {
		.name = "UACC",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_UNCAL_ACC,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 3,
		.axis_scale = {0.001, 0.001, 0.001},
		.max_range = 2,
		.resolution = 0.000488281,
		.power = 0.006,
		.plat_data = &g_info[15],
	},
	[21] = {
		.name = "UCMPS",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_UNCAL_COMP,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 6,
		.axis_scale = {0.1, 0.1, 0.1, 0.1, 0.1, 0.1},
		.max_range = 800.0,
		.resolution = 0.5,
		.power = 0.35,
		.plat_data = &g_info[17],
	},
	[22] = {
		.name = "UGYRO",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_UNCAL_GYRO,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.min_delay = 10000,
		.axis_num = 6,
		.axis_scale = {0.000555556, 0.000555556, 0.000555556, 0.000555556, 0.000555556, 0.000555556},
		.max_range = 11.111111,
		.resolution = 0.000555556,
		.power = 6.1,
		.plat_data = &g_info[16],
	},
};

void dispatch_streaming(struct cmd_resp *p_cmd_resp);

static int hwFdData = -1;
static int hwFdEvent = -1;

int get_serial_num(char *dir_name)
{
	int prefix_len = strlen(SENSOR_DIR_PREFIX);
	int suffix_len = strlen(SENSOR_DIR_SUFFIX);
	int dir_len = strlen(dir_name);
	int serial_str_len = dir_len - prefix_len - suffix_len;
	int serial_num;
	int i;
	char serial_str[10];

	if (serial_str_len <= 0 || serial_str_len >= 10)
		return -1;

	for (i = 0; i < serial_str_len; i++)
		serial_str[i] = dir_name[prefix_len + i];

	serial_str[i] = '\0';

	sscanf(serial_str, "%X", &serial_num);

	return serial_num;
}

int read_sysfs_node(char *file_name, char *buf, int size)
{
	int ret;
	int fd = open(file_name, O_RDONLY);
	if (fd < 0){
		log_message(CRITICAL, "%s: Error opening sysfs node %s\n", __func__, file_name);
		return -1;
	}

	ret = read(fd, buf, size);
	close(fd);

	return ret;
}

int ish_add_sensor(sensor_state_t *sensor_list, char *dir_name)
{
	int serial_num = get_serial_num(dir_name);
	char path[MAX_PATH_LEN];
	char buf[MAX_VALUE_LEN];
	int ret;
	int cur_sensor, sensor_num;

	if (serial_num <= 0)
		return -1;

	snprintf(path, sizeof(path), SENSOR_NAME_PATH, serial_num);
	ret = read_sysfs_node(path, buf, sizeof(buf));
	log_message(DEBUG, "open path %s - %s\n", path, buf);
	if (ret < 0) {
		log_message(CRITICAL, "read path %s failed\n", path);
		return -1;
	}

	if (!strncmp(buf, CUSTOM_SENSOR_NAME, strlen(CUSTOM_SENSOR_NAME))) {
		memset (buf, 0, sizeof(buf));

		snprintf(path, sizeof(path), SENSOR_DESCP_PATH, serial_num);
		log_message(DEBUG, "read path %s \n", path);
		ret = read_sysfs_node(path, buf, sizeof(buf));
		if (ret < 0) {
			log_message(CRITICAL, "read path %s failed\n", path);
			return -1;
		}
	}

	log_message(DEBUG, "read sensor name %s\n", buf);

	sensor_num = sizeof(g_sensor_info) / sizeof(sensor_info_t);
	for (cur_sensor = 0; cur_sensor < sensor_num; cur_sensor++) {
		generic_sensor_info_t *g_tmp = (generic_sensor_info_t *)g_sensor_info[cur_sensor].plat_data;
		if (!g_tmp) {
			log_message(DEBUG, "no platform data, continue\n");
			continue;
		}

		if(!strncmp(buf, g_tmp->friend_name, strlen(g_tmp->friend_name))) {
			DIR *dp;

			log_message(DEBUG, "match a sensor name %d\n", serial_num);

			g_tmp->serial_num = serial_num;

			memset (path, 0, sizeof(path));
			snprintf(path, sizeof(path), SENSOR_DATA_FIELD_PATH, serial_num);

			log_message(DEBUG, "open sensor data field %s\n", path);
			if ((dp = opendir(path)) != NULL) {
				struct dirent *dirp;
				unsigned int data_field_num = 0;
				unsigned int cur_offset, cur_index;

				log_message(DEBUG, "open sensor data field %s success\n", path);

				while ((dirp = readdir(dp)) != NULL) {
					char tmp_path[MAX_PATH_LEN];
					int ret;
					unsigned int value;
					datafield_map_t *datafield;
					unsigned int i;

					log_message(DEBUG, "find %dth datafield %s\n", data_field_num, dirp->d_name);

					if (!strncmp(dirp->d_name, ".", strlen(".")) || !strncmp(dirp->d_name, "..", strlen("..")))
						continue;

					strcpy(tmp_path, path);
					strcat(tmp_path, dirp->d_name);
					strcat(tmp_path, "/");

					/* read usage_id */
					strcat(tmp_path, DATA_FIELD_USAGE_ID_FILE);
					log_message(CRITICAL, "tmp_path %s\n", tmp_path);
					memset (buf, 0, sizeof(buf));
					ret = read_sysfs_node(tmp_path, buf, sizeof(buf));
					if (ret < 0) {
						log_message(CRITICAL, "read path %s failed\n", tmp_path);
						continue;
					}

					log_message(DEBUG, "path %s value %s\n", tmp_path, buf);
					sscanf(buf, "%x", &value);

					/* this field has usage_id, so it's an avaialbe data filed */
					data_field_num++;

					datafield = NULL;

					for (i = 0; i < MAX_DATA_FIELD; i++) {
						if (g_tmp->data_field[i].usage_id == 0) {
							datafield = &g_tmp->data_field[i];
							datafield->usage_id = value;
							break;
						} else if (g_tmp->data_field[i].usage_id == value) {
							datafield = &g_tmp->data_field[i];
							break;
						}
					}

					if (datafield == NULL) {
						log_message(DEBUG, "datafield not match any data filed, continue\n");
						continue;
					}

					/* read offset index */
					tmp_path[strlen(tmp_path) - strlen(DATA_FIELD_USAGE_ID_FILE)] = '\0';
					strcat(tmp_path, DATA_FIELD_INDEX_FILE);
					log_message(DEBUG, "tmp_path %s\n", tmp_path);
					memset (buf, 0, sizeof(buf));
					ret = read_sysfs_node(tmp_path, buf, sizeof(buf));
					if (ret < 0) {
						log_message(CRITICAL, "read path %s failed\n", tmp_path);
						continue;
					}

					log_message(DEBUG, "path %s value %s\n", tmp_path, buf);
					sscanf(buf, "%d", &value);
					datafield->index = value;

					/* read offset length */
					tmp_path[strlen(tmp_path) - strlen(DATA_FIELD_INDEX_FILE)] = '\0';
					strcat(tmp_path, DATA_FIELD_LENGTH_FILE);
					log_message(DEBUG, "tmp_path %s\n", tmp_path);
					memset (buf, 0, sizeof(buf));
					ret = read_sysfs_node(tmp_path, buf, sizeof(buf));
					if (ret < 0) {
						log_message(CRITICAL, "read path %s failed\n", tmp_path);
						continue;
					}

					log_message(DEBUG, "path %s value %s\n", tmp_path, buf);
					sscanf(buf, "%d", &value);
					datafield->length = value;
				}

				for (cur_index = 0; cur_index < data_field_num; cur_index++) {
					log_message(DEBUG, "index %d, length=%d\n",
								g_tmp->data_field[cur_index].index, g_tmp->data_field[cur_index].length);
				}

				/* calculate offset for every index */
				for (cur_index = cur_offset = 0; cur_index < data_field_num; cur_index++) {
					datafield_map_t *datafield = NULL;
					unsigned int i;

					for (i = 0; i < data_field_num; i++) {
						if (cur_index == g_tmp->data_field[i].index && g_tmp->data_field[i].length)
							datafield = &g_tmp->data_field[i];
					}

					if (datafield != NULL) {
						datafield->offset = cur_offset;
						cur_offset += datafield->length;
						log_message(DEBUG, "index %d, offset=%d, length=%d\n",
								cur_index, datafield->offset, datafield->length);
					}
				}

			} else {
				log_message(CRITICAL, "open path %s failed\n", path);
				return -1;
			}

			break;
		}
	}

	if (cur_sensor < sensor_num) {
		sensor_list->sensor_info = &g_sensor_info[cur_sensor];
		return 0;
	} else
		return -1;
}

#define WAITING_SENSOR_UP_TIME		50
int init_generic_sensors(void *p_sensor_list, unsigned int *index)
{
	FILE *file;
	int try_count;
	int file_num;

	log_message(DEBUG, "[%s] enter\n", __func__);

	/* if file exist - delete it, because sensors aren't up yet */
	file = fopen(SENSOR_UP_FILE, "r");
	if (file != NULL) {
		fclose(file);
		remove(SENSOR_UP_FILE);
	}

	try_count = 0;
	file_num = 0;

	while (try_count < WAITING_SENSOR_UP_TIME) {
		DIR *dp;

		if ((dp = opendir(SENSOR_COL_PATH)) != NULL){
			struct dirent *dirp;
			int num_tmp = 0;

			log_message(DEBUG, "open %s success\n", SENSOR_COL_PATH);
			while ((dirp = readdir(dp)) != NULL)
				num_tmp++;

			log_message(DEBUG, "find %d dirs\n", num_tmp);

			if (num_tmp != file_num)
				file_num = num_tmp;
			else
				break;

			closedir(dp);
		}

		sleep(1);
		try_count++;
	}

	if (try_count != WAITING_SENSOR_UP_TIME && file_num != 0) {
		DIR *dp = opendir(SENSOR_COL_PATH);
		struct dirent *dirp;
		sensor_state_t *tmp_list = p_sensor_list;

		while ((dirp = readdir(dp)) != NULL) {
			if (strncmp(dirp->d_name, SENSOR_DIR_PREFIX, strlen(SENSOR_DIR_PREFIX)) == 0) {
				log_message(DEBUG, "process index %d dir %s\n", *index, dirp->d_name);

				if (!ish_add_sensor(tmp_list + *index, dirp->d_name)) {
					(tmp_list + *index)->index = *index;
					(*index)++;
				}
			}
		}
	}

	hwFdData = open(SENSOR_DATA_PATH, O_RDONLY);
	if (hwFdData < 0 ) {
		log_message(CRITICAL, "%s open %s error\n", __func__, SENSOR_DATA_PATH);
		return -1;
	}

	/* use /dev/sensor-collection for event polling */
	hwFdEvent = open(SENSOR_EVENT_PATH, O_RDONLY);
	if(hwFdEvent < 0) {
		log_message(CRITICAL, "%s open event node error\n",__func__);
		return -1;
	}

	file = fopen(SENSOR_UP_FILE, "w");
	if (file) {
		log_message(DEBUG, "create sync file %s\n", SENSOR_UP_FILE);
		fclose(file);
		system("chmod 666 /data/sensorsUp");
	}

	log_message(DEBUG, "[%s] exit\n", __func__);

	return 0;
}

static int set_sensor_property(unsigned int serial_num, const char *property_name, const char *value, int len)
{
	char path[MAX_PATH_LEN];
	int fd;
	int ret;

	log_message(DEBUG, "[%s] enter\n", __func__);

	snprintf(path, sizeof(path), SENSOR_PROP_PATH, serial_num);
	strcat(path, property_name);
	strcat(path, "/value");

	fd = open(path, O_RDWR);
	if (fd < 0) {
		log_message(CRITICAL, "error opening property entry %s\n", path);
		return -1;
	}

	log_message(DEBUG, "write value %s into entry %s\n", value, path);

	ret = write(fd, value, len);

	close(fd);

	if (ret < 0)
		return -1;
	else
		return 0;
}

static int enable_sensor(unsigned int serial_num, int enabled)
{
	int power_state_val;
	int report_state_val;
	char power_state_str[MAX_VALUE_LEN];
	char report_state_str[MAX_VALUE_LEN];
	int ret;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if(enabled) {
		power_state_val =
			USAGE_SENSOR_PROPERTY_POWER_STATE_D0_FULL_POWER_ENUM;
		report_state_val =
			USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_ENUM;
	} else {
		power_state_val =
			USAGE_SENSOR_PROPERTY_POWER_STATE_D1_LOW_POWER_ENUM;
		report_state_val =
			USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_ENUM;
	}

	snprintf(power_state_str, sizeof(power_state_str), "%d", power_state_val);
	snprintf(report_state_str, sizeof(report_state_str), "%d", report_state_val);

	ret = set_sensor_property(serial_num, "property_power_state", power_state_str, strlen(power_state_str));
	if (ret)
		return ret;

	return set_sensor_property(serial_num, "property_reporting_state", report_state_str, strlen(report_state_str));
}

static unsigned int sensor_type_to_serial_num(ish_sensor_t sensor_type)
{
	int i, j;

	j = sizeof(g_sensor_info) / sizeof(sensor_info_t);
	for (i = 0; i < j; i++) {
		if (g_sensor_info[i].sensor_type == sensor_type) {
			generic_sensor_info_t *g_tmp = (generic_sensor_info_t *)g_sensor_info[i].plat_data;
			if (g_tmp)
				return g_tmp->serial_num;
			else
				return 0;
		}
	}

	return 0;
}

/* 0 on success; -1 on fail */
int generic_sensor_send_cmd(int tran_id, int cmd_id, ish_sensor_t sensor_type,
			unsigned short data_rate, unsigned short buffer_delay, unsigned short bit_cfg)
{
	unsigned int serial_num = sensor_type_to_serial_num(sensor_type);

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (serial_num == 0)
		return -1;

	switch(cmd_id) {
		case CMD_START_STREAMING:
		{
			char report_interval_str[MAX_VALUE_LEN];

			if (enable_sensor(serial_num, 1) < 0)
				return -1;

			snprintf(report_interval_str, sizeof(report_interval_str), "%d", (1000 / data_rate));

			set_sensor_property(serial_num, "property_report_interval", report_interval_str, strlen(report_interval_str));

			break;
		}

		case CMD_STOP_STREAMING:
		{
			if (enable_sensor(serial_num, 0) < 0)
				return -1;

			break;
		}
	}

	return 0;
}

/* Sample structure from senscol sysfs */
struct senscol_sample {
	unsigned int id;
	unsigned int size;
	char data[0];
} __attribute__((packed));

static sensor_info_t *get_sensor_info_by_serial_num(unsigned int serial_num)
{
	int i, j;

	j = sizeof(g_sensor_info) / sizeof(sensor_info_t);
	for (i = 0; i < j; i++) {
		generic_sensor_info_t *g_tmp = (generic_sensor_info_t *)g_sensor_info[i].plat_data;

		if (g_tmp && g_tmp->serial_num == serial_num)
			return &g_sensor_info[i];
	}

	return NULL;
}

static unsigned int get_real_sensor_data_size(sensor_info_t *p_sens_inf)
{
	generic_sensor_info_t *p_g_sens_inf = (generic_sensor_info_t *)p_sens_inf->plat_data;
	unsigned int data_len;
	unsigned int i;

	data_len = 0;

	for (i = 0; i < MAX_DATA_FIELD; i++)
		if (p_g_sens_inf->data_field[i].exposed)
			data_len += p_g_sens_inf->data_field[i].length;

	return data_len;
}

#define MAX_BUF_LEN 4096
static void generic_sensor_dispatch(int fd)
{
	char buf[MAX_BUF_LEN];
	int read_count;

	log_message(DEBUG, "[%s] enter\n", __func__);

	while ((read_count = read(hwFdData, buf, sizeof(buf))) > 0) {
		int cur_point = 0;

		while (cur_point < read_count) {
			struct senscol_sample *sample = (struct senscol_sample *)(buf + cur_point);
			char *tmp_buf = (char *)sample->data;
			struct cmd_resp *p_cmd_resp;
			sensor_info_t *p_sens_inf;
			generic_sensor_info_t *p_g_sens_inf;
			unsigned int real_data_len;
			unsigned int offset;
			unsigned int length;
			unsigned int outdata_offset;
			unsigned int i;

			/* cur_point move to next sample */
			cur_point += sample->size;

			p_sens_inf = get_sensor_info_by_serial_num(sample->id);
			if (p_sens_inf)
				p_g_sens_inf = (generic_sensor_info_t *)p_sens_inf->plat_data;
			else
				continue;

			real_data_len = get_real_sensor_data_size(p_sens_inf);
			if (real_data_len > sample->size)
				continue;

			p_cmd_resp = (struct cmd_resp *)malloc(sizeof(struct cmd_resp) + real_data_len);
			if (p_cmd_resp == NULL)
				continue;

			p_cmd_resp->cmd_type = RESP_STREAMING;
			p_cmd_resp->data_len = real_data_len;
			p_cmd_resp->sensor_type = p_sens_inf->sensor_type;

			/* copy sensor data */
			for (i = 0; i < MAX_DATA_FIELD; i++) {
				if (p_g_sens_inf->data_field[i].exposed == 0)
					continue;

				offset = p_g_sens_inf->data_field[i].offset;
				length = p_g_sens_inf->data_field[i].length;
				outdata_offset = p_g_sens_inf->data_field[i].exposed_offset;

				memcpy((char *)p_cmd_resp->buf + outdata_offset, tmp_buf + offset, length);
			}

			dispatch_streaming(p_cmd_resp);

			free(p_cmd_resp);
		}
	}
}

int process_generic_sensor_fd(int fd)
{
	generic_sensor_dispatch(fd);

	return 0;
}

int add_generic_sensor_fds(int maxfd, void *read_fds, int *hw_fds, int *hw_fds_num)
{
	int tmpRead = 0;
	char tmpBuf[1024];
	int new_max_fd = maxfd;

	FD_SET(hwFdEvent, (fd_set*)read_fds);

	if (hwFdEvent > new_max_fd)
		new_max_fd = hwFdEvent;

	(*hw_fds_num)++;

	hw_fds[0] = hwFdEvent;

	/* read from event sysfs entry before select */
	tmpRead = read(hwFdEvent, tmpBuf, 1024);

	return new_max_fd;
}
