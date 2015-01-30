#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <cutils/log.h>

#include "libsensorhub.h"
#include "message.h"

static void dump_sensor_info(sensor_info_t *sensor_list, int sensor_num)
{
	int i;

	for (i = 0; i < sensor_num; i++) {
		sensor_info_t *list = sensor_list + i;

		printf("*** sensor index %d ***\n", i);
		printf("name: %s \n", list->name);
		printf("vendor: %s \n", list->vendor);
	}
}

static void dump_accel_data(int fd)
{
	char buf[512];
	int size = 0;
	struct accel_data *p_accel_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_accel_data = (struct accel_data *)buf;
		while (size > 0) {
			printf("x, y, z is: %d, %d, %d, size is %d \n",
					p_accel_data->x, p_accel_data->y,
					p_accel_data->z, size);
			size = size - sizeof(struct accel_data);
			p = p + sizeof(struct accel_data);
			p_accel_data = (struct accel_data *)p;
		}
	}
}

static void dump_gyro_data(int fd)
{
	char buf[512];
	int size = 0;
	struct gyro_raw_data *p_gyro_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_gyro_raw_data = (struct gyro_raw_data *)buf;
		while (size > 0) {
		printf("x, y, z is: %d, %d, %d, size is %d \n",
					p_gyro_raw_data->x, p_gyro_raw_data->y,
					p_gyro_raw_data->z, size);
			size = size - sizeof(struct gyro_raw_data);
			p = p + sizeof(struct gyro_raw_data);
			p_gyro_raw_data = (struct gyro_raw_data *)p;
		}
	}
}

static void dump_comp_data(int fd)
{
	char buf[512];
	int size = 0;
	struct compass_raw_data *p_compass_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_compass_raw_data = (struct compass_raw_data *)buf;
		while (size > 0) {
		printf("x, y, z is: %d, %d, %d, size is %d \n",
				p_compass_raw_data->x, p_compass_raw_data->y,
				p_compass_raw_data->z, size);
			size = size - sizeof(struct compass_raw_data);
			p = p + sizeof(struct compass_raw_data);
			p_compass_raw_data = (struct compass_raw_data *)p;
		}
	}
}

static void dump_light_data(int fd)
{
	char buf[512];
	int size = 0;
	struct als_raw_data *p_als_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_als_raw_data = (struct als_raw_data *)buf;
		while (size > 0) {
		printf("lux is: %d, size is %d \n",
					p_als_raw_data->lux, size);
			size = size - sizeof(struct als_raw_data);
			p = p + sizeof(struct als_raw_data);
			p_als_raw_data = (struct als_raw_data *)p;
		}
	}
}

static void dump_pressure_data(int fd)
{
	char buf[512];
	int size = 0;
	struct baro_raw_data *p_baro_raw_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_baro_raw_data = (struct baro_raw_data *)buf;
		while (size > 0) {
		printf("pressure is: %d, size is %d \n",
					p_baro_raw_data->p, size);
			size = size - sizeof(struct baro_raw_data);
			p = p + sizeof(struct baro_raw_data);
			p_baro_raw_data = (struct baro_raw_data *)p;
		}
	}
}

static void dump_proximity_data(int fd)
{
	char buf[512];
	int size = 0;
	struct ps_phy_data *p_ps_phy_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_ps_phy_data = (struct ps_phy_data *)buf;
		while (size > 0) {
		printf("proximity is: %d, size is %d \n",
					p_ps_phy_data->near, size);
			size = size - sizeof(struct ps_phy_data);
			p = p + sizeof(struct ps_phy_data);
			p_ps_phy_data = (struct ps_phy_data *)p;
		}
	}
}

static void dump_stepc_data(int fd)
{
	char buf[512];
	int size = 0;
	struct stepcounter_data *p_stepcounter_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_stepcounter_data = (struct stepcounter_data *)buf;
		while (size > 0) {
		printf("stepc is: %d, %d, %d, %d, %d size is %d \n",
					p_stepcounter_data->reset_flag,
					p_stepcounter_data->walk_step_count,
					p_stepcounter_data->walk_step_duration,
					p_stepcounter_data->run_step_count,
					p_stepcounter_data->run_step_duration,
					size);
			size = size - sizeof(struct stepcounter_data);
			p = p + sizeof(struct stepcounter_data);
			p_stepcounter_data = (struct stepcounter_data *)p;
		}
	}
}

static void dump_stepd_data(int fd)
{
	char buf[512];
	int size = 0;
	struct stepdetector_data *p_stepdetector_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_stepdetector_data = (struct stepdetector_data *)buf;
		while (size > 0) {
		printf("stepd is: %d, %d, %d, %d size is %d \n",
					p_stepdetector_data->step_event_counter,
					p_stepdetector_data->step_type,
					p_stepdetector_data->step_count,
					p_stepdetector_data->step_duration,
					size);
			size = size - sizeof(struct stepdetector_data);
			p = p + sizeof(struct stepdetector_data);
			p_stepdetector_data = (struct stepdetector_data *)p;
		}
	}
}

static void dump_tap_data(int fd)
{
	char buf[512];
	int size = 0;
	struct stap_data *p_stap_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_stap_data = (struct stap_data *)buf;
		while (size > 0) {
		printf("stepd is: %d, size is %d \n",
					p_stap_data->stap,
					size);
			size = size - sizeof(struct stap_data);
			p = p + sizeof(struct stap_data);
			p_stap_data = (struct stap_data *)p;
		}
	}
}

static void dump_sm_data(int fd)
{
	char buf[512];
	int size = 0;
	struct sm_data *p_sm_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_sm_data = (struct sm_data *)buf;
		while (size > 0) {
		printf("stepd is: %d, size is %d \n",
					p_sm_data->state,
					size);
			size = size - sizeof(struct sm_data);
			p = p + sizeof(struct sm_data);
			p_sm_data = (struct sm_data *)p;
		}
	}
}

static void dump_lift_data(int fd)
{
	char buf[512];
	int size = 0;
	struct lift_data *p_lift_data;

	while ((size = read(fd, buf, 512)) > 0) {
		char *p = buf;

		p_lift_data = (struct lift_data *)buf;
		while (size > 0) {
		printf("lift is: %d, %d size is %d \n",
					p_lift_data->look,
					p_lift_data->vertical,
					size);
			size = size - sizeof(struct lift_data);
			p = p + sizeof(struct lift_data);
			p_lift_data = (struct lift_data *)p;
		}
	}
}

static void usage()
{
	printf("\n Usage: sensorhub_client [OPTION...] \n");
	printf("  -c, --cmd-type	1 get_streaming \n");
	printf("  -t, --sensor-type	ACCEL, accelerometer;        GYRO, gyroscope;                    COMPS, compass;\n"
		"			BARO, barometer;             ALS_P, ALS;                         PS_P, Proximity;\n"
		"			TERMC, terminal context;     LPE_P, LPE;                         PHYAC, physical activity;\n"
		"			GSSPT, gesture spotting;     GSFLK, gesture flick;               RVECT, rotation vector;\n"
		"			GRAVI, gravity;              LACCL, linear acceleration;         ORIEN, orientation;\n"
		"			9DOF, 9dof;                  PEDOM, pedometer;                   MAGHD, magnetic heading;\n"
		"			SHAKI, shaking;              MOVDT, move detect;                 STAP, stap;\n"
		"			PTZ, pan tilt zoom;          LTVTL, lift vertical;               DVPOS, device position;\n"
		"			SCOUN, step counter;         SDET, step detector;                SIGMT, significant motion;\n"
		"			6AGRV, game_rotation vector; 6AMRV, geomagnetic_rotation vector; 6DOFG, 6dofag;\n"
		"			6DOFM, 6dofam;               LIFT, lift;                   DTWGS, dtwgs;\n"
		"			GSPX, gesture hmm;           GSETH, gesture eartouch;            PDR, \n"
		"			ISACT, instant activity	     UCMPS, uncalibrated compass         UGYRO, uncalibrated gyro\n"
		"			UACC, uncalibrated accelerometer,  MOTDT, move detector\n");
	printf("  -r, --date-rate	unit is Hz\n");
	printf("  -d, --buffer-delay	unit is ms, i.e. 1/1000 second\n");
	printf("  -p, --property-set	format: <property id>,<property value>\n");
	printf("  -h, --help		show this help message \n");

	exit(EXIT_SUCCESS);
}

int parse_prop_set(char *opt, int *prop, int *val)
{
	if (sscanf(opt, "%d,%d", prop, val) == 2)
		return 0;
	return -1;
}

int main(int argc, char **argv)
{
	handle_t handle;
	error_t ret;
	int fd, size = 0, cmd_type = -1, data_rate = -1, buffer_delay = -1;
	char *sensor_name = NULL;
	int prop_ids[10];
	int prop_vals[10];
	int prop_count = 0;
	int prop_tmp = 1;
	char *sensor_list;
	int sensor_num;
	int i;

	while (1) {
		static struct option opts[] = {
			{"cmd", 1, NULL, 'c'},
			{"sensor-type", 1, NULL, 't'},
			{"data-rate", 1, NULL, 'r'},
			{"buffer-delay", 1, NULL, 'd'},
			{"property-set", 2, NULL, 'p'},
			{0, 0, NULL, 0}
		};
		int index, o;

		o = getopt_long(argc, argv, "c:t:r::d::p:", opts, &index);
		if (o == -1)
			break;
		switch (o) {
		case 'c':
			cmd_type = strtod(optarg, NULL);
			break;
		case 't':
			sensor_name = strdup(optarg);
			break;
		case 'r':
			data_rate = strtod(optarg, NULL);
			break;
		case 'd':
			buffer_delay = strtod(optarg, NULL);
			break;
		case 'p':
			if (prop_count == sizeof(prop_ids) / sizeof(prop_ids[0]))
				break;
			if (parse_prop_set(optarg,
				prop_ids + prop_count,
				prop_vals + prop_count)) {
				usage();
			}
			++prop_count;
			break;
		case 'h':
			usage();
			break;
		default:
			;
		}
	}

	if ((sensor_name == NULL) || (cmd_type != 1)) {
		usage();
		return 0;
	}

	if ((cmd_type == 1) && ((data_rate == -1) || (buffer_delay == -1))) {
		usage();
		return 0;
	}

	printf("cmd_type is %d, sensor_name is %s, data_rate is %d Hz, "
			"buffer_delay is %d ms\n", cmd_type, sensor_name,
						data_rate, buffer_delay);

	sensor_num = 0;
	sensor_list = malloc(MAX_SENSOR_INDEX * sizeof(sensor_info_t));
	if (sensor_list == NULL) {
		printf("no memory\n");
		return 0;
	}

	if (get_sensors_list(USE_CASE_CSP, sensor_list, &sensor_num) == ERROR_NONE)
		dump_sensor_info((sensor_info_t *)sensor_list, sensor_num);

	free(sensor_list);

	handle = ish_open_session_with_name(sensor_name);

	if (handle == NULL) {
		printf("ish_open_session() returned NULL handle. \n");
		return -1;
	}

	if (ish_start_streaming(handle, data_rate, buffer_delay)) {
		printf("ish_start_streaming() failed. \n");
		ish_close_session(handle);
		return -1;
	}

	fd = ish_get_fd(handle);

	if (strncmp(sensor_name, "ACCEL", SNR_NAME_MAX_LEN) == 0)
		dump_accel_data(fd);
	else if (strncmp(sensor_name, "GYRO", SNR_NAME_MAX_LEN) == 0)
		dump_gyro_data(fd);
	else if (strncmp(sensor_name, "COMPS", SNR_NAME_MAX_LEN) == 0)
		dump_comp_data(fd);
	else if (strncmp(sensor_name, "ALS_P", SNR_NAME_MAX_LEN) == 0)
		dump_light_data(fd);
	else if (strncmp(sensor_name, "PS_P", SNR_NAME_MAX_LEN) == 0)
		dump_proximity_data(fd);
	else if (strncmp(sensor_name, "BARO", SNR_NAME_MAX_LEN) == 0)
		dump_pressure_data(fd);
	else if (strncmp(sensor_name, "SDET", SNR_NAME_MAX_LEN) == 0)
		dump_stepd_data(fd);
	else if (strncmp(sensor_name, "SCOUN", SNR_NAME_MAX_LEN) == 0)
		dump_stepc_data(fd);
	else if (strncmp(sensor_name, "STAP", SNR_NAME_MAX_LEN) == 0)
		dump_tap_data(fd);
	else if (strncmp(sensor_name, "SIGMT", SNR_NAME_MAX_LEN) == 0)
		dump_sm_data(fd);
	else if (strncmp(sensor_name, "LIFT", SNR_NAME_MAX_LEN) == 0)
		dump_lift_data(fd);
	else
		printf("current not supported!\n");

	ish_stop_streaming(handle);

	ish_close_session(handle);

	return 0;
}
