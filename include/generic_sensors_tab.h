#ifndef _GENERIC_SENSORS_TAB_H
#define _GENERIC_SENSORS_TAB_H

#include "libsensorhub.h"
#include "message.h"
#include "generic_sensors.h"

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
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_MOTION_STATE,
			.exposed = 1,
			.exposed_offset = 16,
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
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATE_ORIENTATION_MAGNETIC_ACCURACY,
			.exposed = 1,
			.exposed_offset = 12,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 13,
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
			.exposed_offset = 8,
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
	/* index 14 -- Magnetic */
	[14] = {
		.friend_name = "0x20f",
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
			.exposed_offset = 8,
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
		.data_field[7] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_7,
			.exposed = 1,
			.exposed_offset = 28,
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
	/* index 24 - Shaking */
	[24] = {
		.friend_name = "0x233",
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
	/* index 25 - Simple device orientation, Terminal */
	[25] = {
		.friend_name = "0x205",
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
	/* index 26 - Flick */
	[26] = {
		.friend_name = "0x213",
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
	/* index 27 - PDR */
	[27] = {
		.friend_name = "0x231",
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
		.data_field[6] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_6,
			.exposed = 1,
			.exposed_offset = 21,
		},
		.data_field[7] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_7,
			.exposed = 1,
			.exposed_offset = 25,
		},
		.data_field[8] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_8,
			.exposed = 1,
			.exposed_offset = 29,
		},
		.data_field[9] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_9,
			.exposed = 1,
			.exposed_offset = 33,
		},
	},
	/* index 28 - Mag Heading */
	[28] = {
		.friend_name = "0x83",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_COMPENSATED_MAGNETIC_NORTH,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_MAGNETIC_FLUX_X_AXIS,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_MAGNETIC_FLUX_Y_AXIS,
			.exposed = 1,
			.exposed_offset = 12,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_MAGNETIC_FLUX_Z_AXIS,
			.exposed = 1,
			.exposed_offset = 16,
		},
	},
	/* index 29 - Orientation */
	[29] = {
		.friend_name = "type_orientation_inclinometer_3d",
		.data_field[0] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_28,
			.exposed = 1,
			.exposed_offset = 0,
		},
		.data_field[1] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_TILT_X,
			.exposed = 1,
			.exposed_offset = 4,
		},
		.data_field[2] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_TILT_Y,
			.exposed = 1,
			.exposed_offset = 8,
		},
		.data_field[3] = {
			.usage_id = USAGE_SENSOR_DATA_ORIENTATION_TILT_Z,
			.exposed = 1,
			.exposed_offset = 12,
		},
		.data_field[4] = {
			.usage_id = USAGE_SENSOR_DATE_ORIENTATION_MAGNETIC_ACCURACY,
			.exposed = 1,
			.exposed_offset = 13,
		},
		.data_field[5] = {
			.usage_id = USAGE_SENSOR_DATA_CUSTOM_VALUE_1,
			.exposed = 1,
			.exposed_offset = 14,
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
		.axis_num = 3,
		.axis_scale = {-0.0000098, -0.0000098, -0.0000098},
		.max_range = 4 * 9.8,
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
		.axis_num = 1,
		.axis_scale = {0.001},
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
		.axis_num = 3,
		.axis_scale = {0.0001, 0.0001, 0.0001},
		.max_range = 800.0,
		.resolution = 0.1,
		.power = 0.35,
		.plat_data = &g_info[14],
	},
	[4] = {
		.name = "GYRO",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GYRO,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 3,
		.axis_scale = {0.000000174444, 0.000000174444, 0.000000174444},
		.max_range = 34.888889,
		.resolution = 0.0174444,
		.power = 6.1,
		.plat_data = &g_info[1],
	},
	[5] = {
		.name = "BARO",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_BARO,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 1,
		.axis_scale = {0.01},
		.max_range = 2000,
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
		.axis_num = 3,
		.axis_scale = {0.0000098, 0.0000098, 0.0000098},
		.max_range = 2 * 9.8,
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
		.axis_num = 3,
		.axis_scale = {-0.0000098, -0.0000098, -0.0000098},
		.max_range = 2 * 9.8,
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
		.axis_num = 5,
		.axis_scale = {0.0001, 0.0001, 0.0001, 0.0001, 0.01},
		.max_range = 1,
		.resolution = 0.1,
		.power = 0.106,
		.plat_data = &g_info[6],
	},
	[9] = {
		.name = "ORIEN",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_ORIENTATION,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 3,
		.axis_scale = {0.00001, 0.00001, 0.00001},
		.max_range = 360,
		.resolution = 0.1,
		.power = 0.106,
		.plat_data = &g_info[29],
	},
	[10] = {
		.name = "SDET",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_STEPDETECTOR,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
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
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 0xFFFFFFFF,
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
		.axis_num = 4,
		.axis_scale = {0.0001, 0.0001, 0.0001, 0.0001},
		.max_range = 2,
		.resolution = 0.1,
		.power = 0.006,
		.plat_data = &g_info[13],
	},
	[13] = {
		.name = "6AMRV",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GEOMAGNETIC_ROTATION_VECTOR,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 4,
		.axis_scale = {0.0001, 0.0001, 0.0001, 0.0001},
		.max_range = 2,
		.resolution = 0.00001,
		.power = 0.006,
		.plat_data = &g_info[10],
	},
	[14] = {
		.name = "STAP",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_STAP,
		.use_case = USE_CASE_HAL,
		.version = 1,
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
		.use_case = USE_CASE_HAL,
		.version = 1,
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
		.use_case = USE_CASE_HAL,
		.version = 1,
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
		.use_case = USE_CASE_HAL,
		.version = 1,
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
		.axis_num = 3,
		.axis_scale = {0.0000098, 0.0000098, 0.0000098},
		.max_range = 4 * 9.8,
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
		.axis_num = 6,
		.axis_scale = {0.0001, 0.0001, 0.0001, 0.0001, 0.0001, 0.0001},
		.max_range = 800.0,
		.resolution = 0.1,
		.power = 0.35,
		.plat_data = &g_info[17],
	},
	[22] = {
		.name = "UGYRO",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_UNCAL_GYRO,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 6,
		.axis_scale = {0.000000174444, 0.000000174444, 0.000000174444, 0.000000174444, 0.000000174444, 0.000000174444},
		.max_range = 34.888889,
		.resolution = 0.0174444,
		.power = 6.1,
		.plat_data = &g_info[16],
	},
	[23] = {
		.name = "SHAKI",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_SHAKING,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[24],
	},
	[24] = {
		.name = "TERMC",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_TC,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[25],
	},
	[25] = {
		.name = "GSFLK",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_GESTURE_FLICK,
		.use_case = USE_CASE_HAL,
		.version = 1,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[26],
	},
	[26] = {
		.name = "PDR",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_PEDESTRIAN_DEAD_RECKONING,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.axis_num = 1,
		.axis_scale = {1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[27],
	},
	[27] = {
		.name = "MAGHD",
		.vendor = "Intel Inc.",
		.sensor_type = SENSOR_MAG_HEADING,
		.use_case = USE_CASE_CSP,
		.version = 1,
		.axis_num = 4,
		.axis_scale = {1, 1, 1, 1},
		.max_range = 1,
		.resolution = 1.0,
		.power = 0.006,
		.plat_data = &g_info[28],
	},
};

#endif
