#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <linux/ioctl.h>
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
#include "generic_sensors_tab.h"
#include "generic_sensor_heci.h"

/* sensor main entry path */
#define SENSOR_EVENT_PATH		"/dev/sensor-collection"
#define SENSOR_COL_PATH			"/sys/bus/platform/devices/sensor_collection/"

/* sensor data path */
#define SENSOR_DATA_PATH		SENSOR_COL_PATH "/data/sensors_data"

/* sensor information entry path for each sensors */
#define SENSOR_DIR_PATH			SENSOR_COL_PATH "sensor_%X_def/"

/* sensor name path */
#define SENSOR_NAME_PATH		SENSOR_DIR_PATH "name"

/* sensor name path */
#define SENSOR_FLUSH_PATH		SENSOR_DIR_PATH "flush"

/* sensor property */
#define SENSOR_PROP_PATH		SENSOR_DIR_PATH "properties/"
#define PROP_DESCP_PATH			SENSOR_PROP_PATH "property_sensor_description/value"

#define PROP_INTERVAL			"property_report_interval"
#define PROP_MIN_INTERVAL		"property_minimum_report_interval"
#define PROP_DELAY			"property_report_interval_resolution"
#define PROP_POWER_STATE		"property_power_state"
#define PROP_REPORT_STATE		"property_reporting_state"
#define PROP_SENSITIVITY_SUFFIX		"_chg_sensitivity_abs"

/* sensor data field */
#define SENSOR_DATA_FIELD_PATH		SENSOR_DIR_PATH "data_fields/"
#define DATA_FIELD_USAGE_ID_FILE	"usage_id"
#define DATA_FIELD_INDEX_FILE		"index"
#define DATA_FIELD_LENGTH_FILE		"length"

#define SENSOR_DIR_PREFIX		"sensor_"
#define SENSOR_DIR_SUFFIX		"_def"

#define CUSTOM_SENSOR_NAME		"type_other_custom"

#define MAX_PATH_LEN			256
#define MAX_VALUE_LEN			64

static int timestamp_mode = 0;

static void *saved_sensor_list = NULL;
static unsigned int index_start;
static unsigned int index_end;

static int hwFdData = -1;
static int hwFdEvent = -1;

static int reset_notifyfds[2];

void sig_handler(int sig)
{
	log_message(INFO, "Receive a signel %d! \n", sig);
	write(reset_notifyfds[1], &sig, sizeof(sig));
}

/* time different between host and firmware, unit is ns*/
static int64_t time_diff_ns = 0;

static uint64_t get_current_time_ns()
{
	struct timespec t;

	clock_gettime(CLOCK_BOOTTIME, &t);

	return ((t.tv_sec * 1000000000) + t.tv_nsec);
}

static int get_time_diff()
{
	struct smhi_msg_header req;
	struct smhi_get_time_response resp;
	uint64_t cur_time;
	int heci_fd = -1;
	int ret;
	int i;

	if ((heci_fd = heci_open()) == -1) {
		time_diff_ns = 0;
		log_message(CRITICAL, "can not open heci! \n");
		return ERROR_NOT_AVAILABLE;
	}

	time_diff_ns = 0;

	for (i = 0; i < 10; i++) {
		int64_t temp_diff = 0;

		/* get current host time, unit is ns */
		cur_time = get_current_time_ns();

		memset(req.reserved, 0, sizeof(req.reserved));
		req.status = 0;
		req.is_response = 0;
		req.command = SMHI_GET_TIME;

		/* send the GET_TIME cmd */
		ret = heci_write(heci_fd, (void *)&req, sizeof(req));
		if (ret < 0)
			log_message(CRITICAL, "heci_write failed ret=%d \n", ret);

		/* get current firmware time, unit is ms */
		ret = heci_read(heci_fd, (void *)&resp, sizeof(resp));
		if (ret < 0)
			log_message(CRITICAL, "heci_read failed  ret=%d \n", ret);

		if (resp.header.status) {
			time_diff_ns = 0;
			heci_close(heci_fd);
			log_message(CRITICAL, "get time from heci failed! \n");
			return ERROR_NOT_AVAILABLE;
		}

		temp_diff = cur_time - (resp.time_ms * 1000000);

		log_message(DEBUG, "get time diff %lld! \n", temp_diff);

		if (time_diff_ns == 0 || time_diff_ns > temp_diff)
			time_diff_ns = temp_diff;
	}

	log_message(INFO, "current time diff %lld! \n", time_diff_ns);

	heci_close(heci_fd);

	if (timestamp_mode)
		log_message(INFO, "Current ISHFW timestamp is 64bit\n");
	else
		log_message(INFO, "Current ISHFW timestamp is 32bit\n");

	return ERROR_NONE;
}

static unsigned int get_serial_num(char *dir_name)
{
	int prefix_len = strlen(SENSOR_DIR_PREFIX);
	int suffix_len = strlen(SENSOR_DIR_SUFFIX);
	int dir_len = strlen(dir_name);
	int serial_str_len = dir_len - prefix_len - suffix_len;
	unsigned int serial_num;
	int i;
	char serial_str[10];

	if (serial_str_len <= 0 || serial_str_len >= 10)
		return 0;

	for (i = 0; i < serial_str_len; i++)
		serial_str[i] = dir_name[prefix_len + i];

	serial_str[i] = '\0';

	sscanf(serial_str, "%X", &serial_num);

	return serial_num;
}

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

static int read_sysfs_node(char *file_name, char *buf, int size)
{
	int ret;
	int fd = open(file_name, O_RDONLY);
	if (fd < 0){
		log_message(CRITICAL, "%s: Error opening sysfs node %s, error %s\n", __func__, file_name, strerror(errno));
		return -1;
	}

	ret = read(fd, buf, size);
	close(fd);

	if (ret >= 0 && ret < size)
		buf[ret] = '\0';
	else
		buf[0] = '\0';

	return ret;
}

static char *create_path(char *dest, const char *src1, const char *src2, unsigned int max_size)
{
	unsigned int len1, len2;

	if (dest == NULL || src1 == NULL || src2 == NULL)
		return NULL;

	len1 = strlen(src1);
	len2 = strlen(src2);

	if (len1 >= max_size || len2 >= max_size || (len1 + len2) >= max_size)
		return NULL;

	memset(dest, 0, max_size);
	strcpy(dest, src1);
	strcat(dest, src2);

	return dest;
}

static char *strcat_path(char *dest, const char *src1, const char *src2, unsigned int max_size)
{
	unsigned int len1, len2, len3;

	if (dest == NULL || src1 == NULL)
		return NULL;

	len1 = strlen(dest);
	len2 = strlen(src1);
	if (src2)
		len3 = strlen(src2);

	if (len1 >= max_size || len2 >= max_size || (len1 + len2) >= max_size)
		return NULL;

	if (src2 && (len3 >= max_size || (len1 + len2 + len3) >= max_size))
		return NULL;

	strcat(dest, src1);

	if (src2)
		strcat(dest, src2);

	return dest;
}

static int set_sensor_property(unsigned int serial_num, const char *property_name, const char *value, int len)
{
	char path[MAX_PATH_LEN];
	int fd;
	int ret;

	log_message(DEBUG, "[%s] enter\n", __func__);

	snprintf(path, sizeof(path), SENSOR_PROP_PATH, serial_num);
	strcat_path(path, property_name, "/value", sizeof(path));

	fd = open(path, O_RDWR);
	if (fd < 0) {
		log_message(CRITICAL, "error opening property entry %s, error %s\n", path, strerror(errno));
		return -1;
	}

	log_message(INFO, "[%lld]before write value %s into entry %s \n", get_current_time_ns(), value, path);
	ret = write(fd, value, len);
	log_message(INFO, "[%lld]after value %s into entry %s \n", get_current_time_ns(), value, path);

	close(fd);

	if (ret < 0) {
		log_message(CRITICAL, "write value %s into entry %s failed, error %s\n", value, path, strerror(errno));
		return -1;
	} else {
		log_message(DEBUG, "write value %s into entry %s success\n", value, path);
		return 0;
	}
}

static int get_sensor_property(unsigned int serial_num, const char *property_name, char *value, int len)
{
	char path[MAX_PATH_LEN];
	int fd;
	int ret;

	log_message(DEBUG, "[%s] enter\n", __func__);

	snprintf(path, sizeof(path), SENSOR_PROP_PATH, serial_num);
	strcat_path(path, property_name, "/value", sizeof(path));

	fd = open(path, O_RDWR);
	if (fd < 0) {
		log_message(CRITICAL, "error opening property entry %s, error %s\n", path, strerror(errno));
		return ERROR_NOT_AVAILABLE;
	}

	log_message(INFO, "[%lld]before read entry %s value \n", get_current_time_ns(), path);
	ret = read(fd, value, len);
	log_message(INFO, "[%lld]after read entry %s value \n", get_current_time_ns(), path);

	close(fd);

	if (ret < 0) {
		log_message(CRITICAL, "read entry %s value failed, error %s\n", path, strerror(errno));
		return ERROR_NOT_AVAILABLE;
	} else {
		log_message(DEBUG, "read entry %s value success\n", path);
		return ERROR_NONE;
	}
}

static int enable_sensor(unsigned int serial_num, int enabled, unsigned char wake)
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

		if(wake)
			report_state_val =
				USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_WAKE_ENUM;
		else
			report_state_val =
				USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_ENUM;
	} else {
		power_state_val =
			USAGE_SENSOR_PROPERTY_POWER_STATE_D1_LOW_POWER_ENUM;

		report_state_val =
			USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_ENUM;
	}

	snprintf(report_state_str, sizeof(report_state_str), "%d", report_state_val);
	snprintf(power_state_str, sizeof(power_state_str), "%d", power_state_val);

	if (enabled) {
		ret =set_sensor_property(serial_num, PROP_POWER_STATE, power_state_str, strlen(power_state_str));
		ret |= set_sensor_property(serial_num, PROP_REPORT_STATE, report_state_str, strlen(report_state_str));
	} else {
		ret = set_sensor_property(serial_num, PROP_REPORT_STATE, report_state_str, strlen(report_state_str));
		ret |=set_sensor_property(serial_num, PROP_POWER_STATE, power_state_str, strlen(power_state_str));
	}

	return ret;
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

static int ish_add_sensor(sensor_state_t *sensor_list, char *dir_name)
{
	unsigned int serial_num = get_serial_num(dir_name);
	char path[MAX_PATH_LEN];
	char buf[MAX_VALUE_LEN];
	int ret;
	int cur_sensor, sensor_num;

	if (serial_num == 0)
		return ERROR_NOT_AVAILABLE;

	snprintf(path, sizeof(path), SENSOR_NAME_PATH, serial_num);
	ret = read_sysfs_node(path, buf, sizeof(buf));
	log_message(DEBUG, "open path %s - %s\n", path, buf);
	if (ret < 0) {
		log_message(CRITICAL, "read path %s failed\n", path);
		return ERROR_NOT_AVAILABLE;
	}

	if (!strncmp(buf, CUSTOM_SENSOR_NAME, strlen(CUSTOM_SENSOR_NAME))) {
		memset (buf, 0, sizeof(buf));

		snprintf(path, sizeof(path), PROP_DESCP_PATH, serial_num);
		log_message(DEBUG, "read path %s \n", path);
		ret = read_sysfs_node(path, buf, sizeof(buf));
		if (ret < 0) {
			log_message(CRITICAL, "read path %s failed\n", path);
			return ERROR_NOT_AVAILABLE;
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

		if (g_tmp->installed) {
			log_message(DEBUG, "this sensor is already installed! continue! \n");
			continue;
		}

		if(!strncmp(buf, g_tmp->friend_name, strlen(g_tmp->friend_name))) {
			DIR *dp;

			log_message(INFO, "match a sensor name %s, serial %x\n", buf, serial_num);

			g_tmp->serial_num = serial_num;

			memset(path, 0, sizeof(path));
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

					/* read usage_id */
					create_path(tmp_path, path, dirp->d_name, sizeof(tmp_path));
					strcat_path(tmp_path, "/", DATA_FIELD_USAGE_ID_FILE, sizeof(tmp_path));
					log_message(DEBUG, "tmp_path %s\n", tmp_path);
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
					create_path(tmp_path, path, dirp->d_name, sizeof(tmp_path));
					strcat_path(tmp_path, "/", DATA_FIELD_INDEX_FILE, sizeof(tmp_path));
					log_message(DEBUG, "tmp_path %s\n", tmp_path);
					memset (buf, 0, sizeof(buf));
					ret = read_sysfs_node(tmp_path, buf, sizeof(buf));
					if (ret < 0) {
						log_message(CRITICAL, "read path %s failed\n", tmp_path);
						continue;
					}

					log_message(DEBUG, "path %s value %s\n", tmp_path, buf);
					sscanf(buf, "%u", &value);
					datafield->index = value;

					/* read offset length */
					create_path(tmp_path, path, dirp->d_name, sizeof(tmp_path));
					strcat_path(tmp_path, "/", DATA_FIELD_LENGTH_FILE, sizeof(tmp_path));
					log_message(DEBUG, "tmp_path %s\n", tmp_path);
					memset (buf, 0, sizeof(buf));
					ret = read_sysfs_node(tmp_path, buf, sizeof(buf));
					if (ret < 0) {
						log_message(CRITICAL, "read path %s failed\n", tmp_path);
						continue;
					}

					log_message(DEBUG, "path %s value %s\n", tmp_path, buf);
					sscanf(buf, "%u", &value);
					datafield->length = value;

					if (datafield->usage_id == USAGE_SENSOR_DATA_CUSTOM_VALUE_27 && datafield->length)
						timestamp_mode = 1;
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

				closedir(dp);

			} else {
				log_message(CRITICAL, "open path %s failed, error %s\n", path, strerror(errno));
				return ERROR_NOT_AVAILABLE;
			}

			/* TODO: add coding to check sensor's infor to determine if it's wake sensor or non-wake sensor */
			g_sensor_info[cur_sensor].is_wake_sensor = 0;

			memset (buf, 0, sizeof(buf));
			if (!get_sensor_property(serial_num, PROP_MIN_INTERVAL, buf, sizeof(buf))) {
				unsigned int value;

				sscanf(buf, "%u", &value);

				if (value)
					g_sensor_info[cur_sensor].min_delay = value * MS_TO_US;
				else
					g_sensor_info[cur_sensor].min_delay = DEFAULT_MIN_DELAY;
			}

			g_sensor_info[cur_sensor].max_delay = DEFAULT_MAX_DELAY;
			g_sensor_info[cur_sensor].fifo_max_event_count = DEFAULT_FIFO_MAX;
			g_sensor_info[cur_sensor].fifo_reserved_event_count = DEFAULT_FIFO_RESERVED;

			g_tmp->installed = 1;

			break;
		}
	}

	if (cur_sensor < sensor_num) {
		sensor_list->sensor_info = &g_sensor_info[cur_sensor];
		return ERROR_NONE;
	} else
		return ERROR_NOT_AVAILABLE;
}

#define WAITING_SENSOR_UP_TIME		50
#define MAX_SENSOR_DIR_NUM		7
int init_generic_sensors(void *p_sensor_list, unsigned int *index)
{
	FILE *file;
	int try_count;
	int file_num;
	struct sigaction usr_action;
	sigset_t block_mask;

	log_message(DEBUG, "[%s] enter\n", __func__);

	saved_sensor_list = p_sensor_list;
	index_start = *index;

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

			closedir(dp);

			log_message(DEBUG, "find %d dirs\n", num_tmp);

			if ((num_tmp != file_num) || (num_tmp <= MAX_SENSOR_DIR_NUM))
				file_num = num_tmp;
			else
				break;
		}

		sleep(1);
		try_count++;
	}

	if (try_count != WAITING_SENSOR_UP_TIME && file_num != 0) {
		DIR *dp = opendir(SENSOR_COL_PATH);
		struct dirent *dirp;
		sensor_state_t *tmp_list = p_sensor_list;

		if (dp == NULL)
			return ERROR_NOT_AVAILABLE;

		while ((dirp = readdir(dp)) != NULL) {
			if (strncmp(dirp->d_name, SENSOR_DIR_PREFIX, strlen(SENSOR_DIR_PREFIX)) == 0) {
				log_message(DEBUG, "process index %d dir %s\n", *index, dirp->d_name);

				if (!ish_add_sensor(tmp_list + *index, dirp->d_name)) {
					(tmp_list + *index)->index = *index;
					(*index)++;
				}
			}
		}

		closedir(dp);
	}

	index_end = *index;

	hwFdData = open(SENSOR_DATA_PATH, O_RDONLY);
	if (hwFdData < 0 ) {
		log_message(CRITICAL, "%s open %s error, error %s\n", __func__, SENSOR_DATA_PATH, strerror(errno));
		return ERROR_NOT_AVAILABLE;
	}

	/* use /dev/sensor-collection for event polling */
	hwFdEvent = open(SENSOR_EVENT_PATH, O_RDWR);
	if(hwFdEvent < 0) {
		log_message(CRITICAL, "%s open event node error, error %s\n",__func__, strerror(errno));
		return ERROR_NOT_AVAILABLE;
	}

	/* sync ishfw time with host */
	if (get_time_diff())
		log_message(CRITICAL, "get_time_diff failed \n");

	/* create pipe for send ishfw reset signal */
	if (pipe(reset_notifyfds) < 0) {
		log_message(CRITICAL, "failed to create pipe for reset signal notification! \n");
		return ERROR_NOT_AVAILABLE;
	}

	sigemptyset(&block_mask);
	usr_action.sa_handler = sig_handler;
	usr_action.sa_mask = block_mask;
	usr_action.sa_flags = SA_NODEFER;
	sigaction(SIGUSR1, &usr_action, NULL);

	log_message(DEBUG, "[%s] exit\n", __func__);

	return ERROR_NONE;
}

static int reenum_generic_sensors(void *p_sensor_list, unsigned int start, unsigned int end)
{
	int try_count = 0;
	int file_num = 0;
	unsigned int check_num = 0;
	unsigned int num = end - start;

	if ((p_sensor_list == NULL) || (start == end))
		return ERROR_NOT_AVAILABLE;

	while (try_count < WAITING_SENSOR_UP_TIME) {
		DIR *dp;

		if ((dp = opendir(SENSOR_COL_PATH)) != NULL){
			struct dirent *dirp;
			int num_tmp = 0;

			log_message(DEBUG, "open %s success\n", SENSOR_COL_PATH);

			while ((dirp = readdir(dp)) != NULL)
				num_tmp++;

			closedir(dp);

			log_message(DEBUG, "find %d dirs\n", num_tmp);

			if ((num_tmp != file_num) || (num_tmp <= MAX_SENSOR_DIR_NUM))
				file_num = num_tmp;
			else
				break;
		}

		sleep(1);
		try_count++;
	}

	if (try_count != WAITING_SENSOR_UP_TIME && file_num != 0) {
		DIR *dp = opendir(SENSOR_COL_PATH);
		struct dirent *dirp;
		sensor_state_t *tmp_list = p_sensor_list;
		unsigned int i;

		if (dp == NULL)
			return ERROR_NOT_AVAILABLE;

		for (i = 0; i < num; i++) {
			generic_sensor_info_t *g_tmp = (generic_sensor_info_t *)((tmp_list + start + i)->sensor_info->plat_data);
			g_tmp->installed = 0;
		}

		while ((dirp = readdir(dp)) != NULL) {
			if (strncmp(dirp->d_name, SENSOR_DIR_PREFIX, strlen(SENSOR_DIR_PREFIX)) == 0) {
				unsigned int serial_num = get_serial_num(dirp->d_name);
				char path[MAX_PATH_LEN];
				char buf[MAX_VALUE_LEN];
				int ret;

				log_message(DEBUG, "process dir %s\n", dirp->d_name);

				if (serial_num == 0) {
					log_message(CRITICAL, "dir %s isn't a valid dir for sensor\n", dirp->d_name);
					continue;
				}

				snprintf(path, sizeof(path), SENSOR_NAME_PATH, serial_num);
				ret = read_sysfs_node(path, buf, sizeof(buf));
				log_message(DEBUG, "open path %s - %s\n", path, buf);
				if (ret < 0) {
					log_message(CRITICAL, "read path %s failed\n", path);
					continue;
				}

				if (!strncmp(buf, CUSTOM_SENSOR_NAME, strlen(CUSTOM_SENSOR_NAME))) {
					memset (buf, 0, sizeof(buf));

					snprintf(path, sizeof(path), PROP_DESCP_PATH, serial_num);
					log_message(DEBUG, "read path %s \n", path);
					ret = read_sysfs_node(path, buf, sizeof(buf));
					if (ret < 0) {
						log_message(CRITICAL, "read path %s failed\n", path);
						continue;
					}
				}

				for (i = 0; i < num; i++) {
					generic_sensor_info_t *g_tmp = (generic_sensor_info_t *)((tmp_list + start + i)->sensor_info->plat_data);

					if (g_tmp->installed)
						continue;

					if (!strncmp(buf, g_tmp->friend_name, strlen(g_tmp->friend_name))) {
						check_num++;

						log_message(INFO, "reenum sensor serial_num matched from %x to %x\n",
												g_tmp->serial_num, serial_num);

						if (g_tmp->serial_num != serial_num)
							g_tmp->serial_num = serial_num;

						g_tmp->installed = 1;

						/* restart sensor */
						if ((tmp_list + start + i)->data_rate) {
							struct cmd_send cmd;
							sensor_state_t *tmp_state = (tmp_list + start + i);

							cmd.tran_id = 0;
							cmd.cmd_id = CMD_START_STREAMING;
							cmd.sensor_type = tmp_state->sensor_info->sensor_type;
							cmd.start_stream.data_rate = tmp_state->data_rate;
							cmd.start_stream.buffer_delay = tmp_state->buffer_delay;
							generic_sensor_send_cmd(&cmd);
						}
					}
				}
			}
		}

		closedir(dp);
	}

	if (check_num != num)
		log_message(CRITICAL, "Sensor number not match after re-enum (org:%d, now:%d)\n", num, check_num);

	if (check_num == 0)
		return ERROR_NOT_AVAILABLE;
	else
		return ERROR_NONE;
}


/* 0 on success; -1 on fail */
int generic_sensor_send_cmd(struct cmd_send *cmd)
{
	unsigned int serial_num = sensor_type_to_serial_num(cmd->sensor_type);
	sensor_info_t *sensor_info;

	log_message(DEBUG, "[%s] enter\n", __func__);

	if (serial_num == 0)
		return ERROR_WRONG_PARAMETER;

	sensor_info = get_sensor_info_by_serial_num(serial_num);
	if (sensor_info == NULL)
		return ERROR_NOT_AVAILABLE;

	switch(cmd->cmd_id) {
		case CMD_START_STREAMING:
		{
			char report_interval_str[MAX_VALUE_LEN];
			char report_delay_str[MAX_VALUE_LEN];

			get_time_diff();

			if (enable_sensor(serial_num, 1, sensor_info->is_wake_sensor) != ERROR_NONE)
				return ERROR_NOT_AVAILABLE;

			if (cmd->start_stream.data_rate) {
				snprintf(report_interval_str, sizeof(report_interval_str), "%d", (1000 / cmd->start_stream.data_rate));
				set_sensor_property(serial_num, PROP_INTERVAL, report_interval_str, strlen(report_interval_str));
			}

			snprintf(report_delay_str, sizeof(report_delay_str), "%d", cmd->start_stream.buffer_delay);
			set_sensor_property(serial_num, PROP_DELAY, report_delay_str, strlen(report_delay_str));

			break;
		}

		case CMD_STOP_STREAMING:
		{
			char report_interval_str[MAX_VALUE_LEN];

			if (enable_sensor(serial_num, 0, sensor_info->is_wake_sensor) != ERROR_NONE)
				return ERROR_NOT_AVAILABLE;

			snprintf(report_interval_str, sizeof(report_interval_str), "%d", 0);
			set_sensor_property(serial_num, PROP_INTERVAL, report_interval_str, strlen(report_interval_str));

			break;
		}

		case CMD_FLUSH_STREAMING:
		{
			char path[MAX_PATH_LEN];
			char buf[MAX_VALUE_LEN];
			int ret;

			snprintf(path, sizeof(path), SENSOR_FLUSH_PATH, serial_num);
			log_message(DEBUG, "read path %s \n", path);
			ret = read_sysfs_node(path, buf, sizeof(buf));
			if (ret < 0) {
				log_message(CRITICAL, "read path %s failed\n", path);
				return ERROR_NOT_AVAILABLE;
			}

			break;
		}

		case CMD_SET_PROPERTY:
		{
			if (cmd->set_prop.prop_type == PROP_SENSITIVITY) {
				char path[MAX_PATH_LEN];
				DIR *dp;

				snprintf(path, sizeof(path), SENSOR_PROP_PATH, serial_num);

				if ((dp = opendir(path)) != NULL){
					struct dirent *dirp;
					int ret = -1;

					log_message(DEBUG, "open %s success\n", path);

					while ((dirp = readdir(dp)) != NULL) {
						if (strstr(dirp->d_name, PROP_SENSITIVITY_SUFFIX)) {
							char sensitivity_str[MAX_VALUE_LEN];

							snprintf(sensitivity_str, sizeof(sensitivity_str), "%u",
										*(unsigned int *)cmd->set_prop.value);

							log_message(DEBUG, "get dir %s for property\n", dirp->d_name);

							ret = set_sensor_property(serial_num, dirp->d_name,
										sensitivity_str, strlen(sensitivity_str));
							break;
						}
					}

					closedir(dp);

					if (ret) {
						log_message(CRITICAL, "Sensor %d not support sensitivity property\n", cmd->sensor_type);
						return ERROR_WRONG_ACTION_ON_SENSOR_TYPE;
					} else {
						log_message(DEBUG, "Sensor %d set sensitivity property success\n", cmd->sensor_type);
						return ERROR_NONE;
					}
				} else {
					log_message(CRITICAL, "open %s failed, error %s\n", path, strerror(errno));
					return ERROR_NOT_AVAILABLE;
				}
			} else {
				log_message(DEBUG, "Currently, not support other property\n");
				return ERROR_PROPERTY_NOT_SUPPORTED;
			}

			break;
		}

		default:
			break;
	}

	return ERROR_NONE;
}

/* Sample structure from senscol sysfs */
struct senscol_sample {
	unsigned int id;
	unsigned int size;
	char data[0];
} __attribute__((packed));

#define PSEUSDO_EVENT_BIT 	(1 << 31)

#define FLUSH_CMPL_BIT		(1 << 0)

static unsigned int get_real_sensor_data_size(sensor_info_t *p_sens_inf)
{
	generic_sensor_info_t *p_g_sens_inf = (generic_sensor_info_t *)p_sens_inf->plat_data;
	unsigned int data_len;
	unsigned int i;

	data_len = 0;

	for (i = 0; i < MAX_DATA_FIELD; i++)
		if (p_g_sens_inf->data_field[i].exposed)
			data_len += p_g_sens_inf->data_field[i].length;

	// If current ish only support 32bit timestamp, we need add 4bytes.
	// Because sensorhubd expected time data field is 64bit.
	if (timestamp_mode == 0)
		data_len += 4;

	return data_len;
}

#define MAX_BUF_LEN 4096
static void generic_sensor_dispatch(int fd)
{
	char buf[MAX_BUF_LEN];
	int read_count;

	if (fd != hwFdEvent)
		return;

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

			if (sample->id & PSEUSDO_EVENT_BIT) {
				int flag = *((int *)sample->data);

				if (flag & FLUSH_CMPL_BIT)
					dispatch_flush();

			} else {
				p_sens_inf = get_sensor_info_by_serial_num(sample->id);
				if (p_sens_inf)
					p_g_sens_inf = (generic_sensor_info_t *)p_sens_inf->plat_data;
				else {
					log_message(CRITICAL, "found unknow sample id 0x%x\n", sample->id);
					continue;
				}

				real_data_len = get_real_sensor_data_size(p_sens_inf);
				if (real_data_len > sample->size)
					continue;

				p_cmd_resp = (struct cmd_resp *)malloc(sizeof(struct cmd_resp) + real_data_len);
				if (p_cmd_resp == NULL)
					continue;

				memset((void*)p_cmd_resp, 0, sizeof(struct cmd_resp) + real_data_len);

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

				uint64_t *ts = (uint64_t *)p_cmd_resp->buf;
				if (timestamp_mode)
					*ts = *ts * 1000 + time_diff_ns;
				else
					*ts = *ts * 125000 + time_diff_ns;

				if (*ts > get_current_time_ns()) {
					*ts = get_current_time_ns();
				}

				dispatch_streaming(p_cmd_resp);

				free(p_cmd_resp);
			}

		}
	}
}

int process_generic_sensor_fd(int fd)
{
	if (fd == hwFdEvent) {
		generic_sensor_dispatch(fd);
	} else if (fd == reset_notifyfds[0]) {
		char buf[10];

		read(reset_notifyfds[0], buf, sizeof(buf));
		reenum_generic_sensors(saved_sensor_list, index_start, index_end);

		/* re-sync ishfw time with host */
		if (get_time_diff())
			log_message(CRITICAL, "get_time_diff failed \n");
	}

	return ERROR_NONE;
}

int add_generic_sensor_fds(int maxfd, void *read_fds, int *hw_fds, int *hw_fds_num)
{
	int tmpRead = 0;
	char tmpBuf[1024];
	int new_max_fd = maxfd;

	if (hwFdEvent < 0)
		return new_max_fd;

	FD_SET(hwFdEvent, (fd_set*)read_fds);

	if (hwFdEvent > new_max_fd)
		new_max_fd = hwFdEvent;

	hw_fds[*hw_fds_num] = hwFdEvent;
	(*hw_fds_num)++;

	/* read from event sysfs entry before select */
	tmpRead = read(hwFdEvent, tmpBuf, 1024);

	if (reset_notifyfds[0] < 0)
		return new_max_fd;

	FD_SET(reset_notifyfds[0], (fd_set*)read_fds);

	if (reset_notifyfds[0] > new_max_fd)
		new_max_fd = reset_notifyfds[0];

	hw_fds[*hw_fds_num] = reset_notifyfds[0];
	(*hw_fds_num)++;

	return new_max_fd;
}
