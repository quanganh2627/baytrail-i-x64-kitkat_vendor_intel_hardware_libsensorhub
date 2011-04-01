/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include "libsensorhub.h"

#define NAME "sensor_hub"
#define NAME_LEN (sizeof(NAME) - 1)
#define SENSOR_HUB_SYSFS_PREF "/sys/class/hwmon/"
#define MAX_PATH 256
#define MAX_STR 256

//#define LOG_NDEBUG 0
#define LOG_TAG "libsensorhub"
#include <cutils/log.h>

const char *node_name[SENSOR_ID_MAX] = {
	"accel", "accel",
	"gyro", "gyro", "gyro_temp",
	"compass", "compass",
	"audio", "audio",
	"alt", "alt"
	"light", "light",
	"orientation_xy",
	"orientation_z",
	"heading",
	"noise",
};

static char sensor_hub_sysfs_path[MAX_PATH];


static int open_sysfs_node(const char *node, int read)
{
	char fname[MAX_PATH];
	int fd;
	
	snprintf(fname, MAX_PATH, "%s%s", sensor_hub_sysfs_path, node);
	fd = open(fname, read ? O_RDONLY : O_WRONLY);
	return fd;
}

static size_t sensor_hub_read_node(const char *node, char *buf, size_t size)
{
	int fd = open_sysfs_node(node, 1);
	size_t ret;
	
	if (fd == -1) {
	  LOGE("Sysfs node %s%s can't be opened", sensor_hub_sysfs_path, node);
	  return -1;
	}
	ret = read(fd, buf, size);
	close(fd);
	return ret;
}

static size_t sensor_hub_write_node(const char *node, const char *buf, size_t size)
{
	int fd = open_sysfs_node(node, 0);
	size_t ret;

	if (fd == -1)
		return -1;
	ret = write(fd, buf, size);
	close(fd);
	return ret;
}

static size_t sensor_hub_write_node_uint(const char *node, u32 value)
{
	char str[MAX_STR];
	size_t size;

	size = snprintf(str, MAX_STR, "%u", value);
	if (size <= 0 || size > MAX_STR)
		return -1;
	return sensor_hub_write_node(node, str, size);
}

static int file_select(const struct dirent *entry)
{
	return !strncmp(entry->d_name, "hwmon", 5);
}

/* search all hwmon* directories and see if it is sensor_hub
 * return 1 if found one sensor_hub node
 */
static int sensor_hub_search_sysfs_path(void)
{
	struct dirent **files, **entry;
	int count;
	int found = 0;

	if (sensor_hub_sysfs_path[0])
		return 1;
	count = scandir(SENSOR_HUB_SYSFS_PREF, &files, file_select, NULL);
	entry = files;
	if (count <= 0)
		return found;
	while (count--) {
		char buff[MAX_STR];
		size_t ret;

		LOGV("SENSORHUB1");

		if (!found) {
		  snprintf(sensor_hub_sysfs_path, MAX_PATH, "%s%s/device/",
			   SENSOR_HUB_SYSFS_PREF, (*entry)->d_name);

		  LOGV("SENSORHUB2: %s", sensor_hub_sysfs_path);

		  ret = sensor_hub_read_node("modalias", buff, NAME_LEN);

		  if (ret == NAME_LEN)
		    if (!strncmp(buff, NAME, NAME_LEN))
		      found = 1;

		  LOGV("SENSORHUB3");

		}
		free(*entry);
		entry++;
	}
	free(files);
	return found;
}

static int fw_select(const struct dirent *entry)
{
	return !strncmp(entry->d_name, "SensorHub_FW", 12);
}

/* search all hwmon* directories and see if it is sensor_hub
 * return 1 if found one sensor_hub node
 */
static int sensor_hub_search_new_fw(int major, int minor, int rev)
{
	struct dirent **files, **entry;
	int count;
	int found = 0;

	count = scandir("/etc/firmware", &files, fw_select, NULL);
	entry = files;
	if (count <= 0)
		return found;
	while (count--) {
		int r1, r2, r3;
		char tmp[MAX_PATH];

		if (sscanf((*entry)->d_name, "SensorHub_FW_%d_%d_%d_",
				&r1, &r2, &r3) == 3) {
			if (r1 > major || (r1 == major && r2 > minor) ||
				(r1 == major && r2 == minor && r3 > rev)) {
				snprintf(tmp, MAX_PATH, "/etc/firmware/%s", (*entry)->d_name);
				property_set("hw.sensorhub.newfw", tmp);
				major = r1;
				minor = r2;
				rev = r3;
				found = 1;
			}
		}
		free(*entry);
		entry++;
	}
	free(files);
	return found;
}

static void *fw_check(void *arg)
{
	char tmp[MAX_PATH] = "";
	int r1, r2, r3;

	property_get("hw.sensorhub.newfw", tmp, "");
	if (strlen(tmp))
		return NULL;
	sensor_hub_read_node("sensor_hub/firmware_rev", tmp, MAX_PATH);
	if (sscanf(tmp, "%d.%d.%d", &r1, &r2, &r3) != 3)
			r1 = r2 = r3 = 0;
	sensor_hub_search_new_fw(r1, r2, r3);
	return NULL;
}

/* initialize sensor_hub, return 0 means ok */
int init_sensor_hub(void)
{
	pthread_t thread_id;
	int i;

	if (!sensor_hub_search_sysfs_path()) {
		fprintf(stderr, "can not find sensor hub sysfs node.\n");
		LOGE("can not find sensor hub sysfs node.\n");
		return -1;
	}
	pthread_create(&thread_id, NULL, &fw_check, NULL);
		
	return 0;
}

/* normally we needn't call this function, since the driver will reset
 * and jump to app fw by default
 */
int reset_sensor_hub(void)
{
	sensor_hub_write_node_uint("sensor_hub/reset", 2); /* 2 means reset and jump */
	return 0;
}

int sensor_hub_set_time(u32 time)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/time", time);
	if (ret <= 0) {
		fprintf(stderr, "set sensor_hub time error\n");
		return -1;
	}
	return 0;
}

int sensor_hub_get_time(u32 *time)
{
	char str[MAX_STR];
	int ret;

	ret = sensor_hub_read_node("sensor_hub/time", str, MAX_STR);
	if (ret <= 0) {
		fprintf(stderr, "get sensor_hub time error\n");
		return -1;
	} else
		sscanf(str, "%d", time);
	return 0;
}

int sensor_hub_get_single_sample(int sensor_id, char *buf, size_t size)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/sensor_id", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, sensor_id);
	if (ret <= 0) {
		fprintf(stderr, "set %s sensor_id error.\n", node_name[sensor_id]);
		LOGE("Set %s sensor_id error", node_name[sensor_id]);
		return -1;
	}
	snprintf(node, MAX_STR, "%s/single_sample", node_name[sensor_id]);
	ret = sensor_hub_read_node(node, buf, size);
	if (ret <= 0) {
		fprintf(stderr, "read %s single sample error.\n", node_name[sensor_id]);
		LOGE("Read %s single sample error", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_set_sample_rate(int sensor_id, int rate)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;

	snprintf(node, MAX_STR, "%s/rate", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, rate);
	if (ret <= 0) {
		fprintf(stderr, "set %s rate error.\n", node_name[sensor_id]);
		return -1;
	}

	return 0;
}

int sensor_hub_set_sample_range(int sensor_id, int range)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/range", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, range);
	if (ret <= 0) {
		fprintf(stderr, "set %s range error.\n", node_name[sensor_id]);
		return -1;
	}
	
	return 0;
}

int sensor_hub_set_sample_number(int sensor_id, int number)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/num_samples", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, number);
	if (ret <= 0) {
		fprintf(stderr, "set %s range error.\n", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_set_buffer_delay_msec(int msec)
{
	char node[MAX_STR];
	int ret;

	snprintf(node, MAX_STR, "sensor_hub/buffer_delay_msec");
	ret = sensor_hub_write_node_uint(node, msec);
	if (ret <= 0) {
		fprintf(stderr, "set sensor_hub buffer_delay_msec error.\n");
		return -1;
	}
	return 0;
}

int sensor_hub_set_data_format(int sensor_id, int format)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/data_format", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, format);
	if (ret <= 0) {
		fprintf(stderr, "set %s data_format error.\n", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_set_capture_mode(int sensor_id, int enable)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/sensor_id", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, sensor_id);
	if (ret <= 0) {
		fprintf(stderr, "set %s sensor_id error.\n", node_name[sensor_id]);
		return -1;
	}
	snprintf(node, MAX_STR, "%s/set_capture", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, enable);
	if (ret <= 0) {
		fprintf(stderr, "set %s set_capture error.\n", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_set_streaming_mode(int sensor_id, int enable)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/sensor_id", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, sensor_id);
	if (ret <= 0) {
		fprintf(stderr, "set %s sensor_id error.\n", node_name[sensor_id]);
		return -1;
	}
	snprintf(node, MAX_STR, "%s/set_streaming", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, enable);
	if (ret <= 0) {
		fprintf(stderr, "set %s set_streaming error.\n", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_init_condition_array(char *str, int num, int type)
{
	if (!str)
		return -1;
	sprintf(str, "%d %d ", num, type);
	return 0;
}

int sensor_hub_init_parameter_string(char *str, int parameter_number, int p1, int p2)
{
	if (!str)
		return -1;
	str[0] = '\0';
	if (parameter_number) {
		sprintf(str, "%d ", p1);
		if (parameter_number == 2)
			sprintf(str + strlen(str), "%d ", p2);
	}
	return 0;
}

int sensor_hub_init_condition_string(char *str, int type, int sid, int thd_min, int thd_max, int data_format, int function_id, int mask, const char *parameter)
{
	if (!str)
		return -1;
	sprintf(str + strlen(str), "%d %d %d %d %d %d %d %s", type, sid, thd_min, thd_max, data_format, function_id, mask, parameter ? parameter : "");
	return 0;
}

int sensor_hub_add_condition_string(char *str, const char *cond)
{
	if (!str)
		return -1;
	sprintf(str + strlen(str), ", %s ", cond);
	return 0;
}

int sensor_hub_set_capture_condition(int sensor_id, const char *capture_array)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;
	snprintf(node, MAX_STR, "%s/sensor_id", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, sensor_id);
	if (ret <= 0) {
		fprintf(stderr, "set %s sensor_id error.\n", node_name[sensor_id]);
		return -1;
	}
	snprintf(node, MAX_STR, "%s/capture_conditions", node_name[sensor_id]);
	ret = sensor_hub_write_node(node, capture_array, strlen(capture_array));
	if (ret <= 0) {
		fprintf(stderr, "set %s set_streaming error.\n", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_init_delivery_string(char *str, int function_id, const char *parameter, const char *cond_array)
{
	if (!str)
		return -1;
	sprintf(str, "%d %s , %s ", function_id, parameter ? parameter : "", cond_array ? cond_array : "");
	return 0;
}

int sensor_hub_add_delivery_string(char *str, const char *delivery)
{
	if (!str || !delivery)
		return -1;
	sprintf(str + strlen(str), "| %s", delivery);
	return 0;
}

int sensor_hub_set_delivery_config(int sensor_id, const char *delivery_array)
{
	char node[MAX_STR];
	int ret;

	if (sensor_id >= SENSOR_ID_MAX)
		return -1;
	if (!node_name[sensor_id])
		return -1;

	snprintf(node, MAX_STR, "%s/sensor_id", node_name[sensor_id]);
	ret = sensor_hub_write_node_uint(node, sensor_id);
	if (ret <= 0) {
		fprintf(stderr, "set %s sensor_id error.\n", node_name[sensor_id]);
		return -1;
	}
	snprintf(node, MAX_STR, "%s/data_delivery_config", node_name[sensor_id]);
	ret = sensor_hub_write_node(node, delivery_array, strlen(delivery_array));
	if (ret <= 0) {
		fprintf(stderr, "set %s set_streaming error.\n", node_name[sensor_id]);
		return -1;
	}
	return 0;
}

int sensor_hub_start_capture(int enable)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/start_capture", enable);
	if (ret <= 0) {
		fprintf(stderr, "set sensor_hub start_capture error.\n");
		return -1;
	}
	return 0;
}

int sensor_hub_start_streaming(int enable)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/start_streaming", enable);
	if (ret <= 0) {
		fprintf(stderr, "set sensor_hub start_streaming error.\n");
		return -1;
	}
	return 0;
}

int sensor_hub_start_store_forwarding(int enable)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/start_store", enable);
	if (ret <= 0) {
		fprintf(stderr, "set sensor_hub start_store error.\n");
		return -1;
	}
	return 0;
}

int sensor_hub_retrieve_stored_data(int stime, int etime)
{
	int ret;
	char tmp[MAX_STR];

	sprintf(tmp, "%d %d", stime, etime);
	ret = sensor_hub_write_node("sensor_hub/retrieve_stored_data", tmp, strlen(tmp));
	if (ret <= 0) {
		fprintf(stderr, "retrieve stored data error.\n");
		return -1;
	}
	return 0;
}


int sensor_hub_get_streaming_data_size(int sensor_id, int timeout) {
  char node[MAX_STR];
  char tmp[MAX_STR];
  struct pollfd fds[1];
  int fd;
  int data = 0;
  int ret = -1;
  
  if (sensor_id >= SENSOR_ID_MAX)
    goto exit;
  if (!node_name[sensor_id])
    goto exit;
  
  LOGV("get_streaming_data_size id = %d",sensor_id);
  
  snprintf(node, MAX_STR, "%s%s/streaming_data_size", sensor_hub_sysfs_path, node_name[sensor_id]);
  fd = open(node, O_RDONLY);
  if (fd <= 0) {
    fprintf(stderr, "open streaming_data_size error.\n");
    LOGE("open streaming_data_size error.\n");
    goto close;
  }
again:
  lseek(fd, 0, SEEK_SET);
  ret = read(fd, tmp, MAX_STR);
  if (ret <= 0) {
    LOGE("Poll error after the read: ret is %d", ret);
    goto close;
  }
  sscanf(tmp, "%d", &data);
  LOGV("get_streaming_data_size data %d", data);
  if (!data) { 
    fds[0].fd = fd;
    fds[0].events = POLLPRI;
    
    LOGV("get_streaming_data_size blocked for data");
    /* blocked for data */
    ret = poll(fds, 1, -1);
    if (ret <= 0) {
      LOGE("Poll error: ret is %d", ret);
      goto close;
    }
    goto again;
  }
  
 close:
    close(fd);
 exit:
    LOGV("Get streaming data size returning %d", data);
    return data;    
}


int sensor_hub_get_streaming_data(int sensor_id, char *buf, size_t size, int timeout)
{
	char node[MAX_STR];
	char tmp[MAX_STR];
	struct pollfd fds[1];
	int fd, fd2;
	int data;
	int ret = -1;
	int total = 0;
	char *sbuf;
	sbuf = buf;

	if (sensor_id >= SENSOR_ID_MAX)
		goto exit;
	if (!node_name[sensor_id])
		goto exit;
	
	LOGV("get_streaming_data id = %d, size = %d",sensor_id, size);

	snprintf(node, MAX_STR, "%s%s/streaming_data", sensor_hub_sysfs_path, node_name[sensor_id]);
	LOGV("Opening node %s",node);
	fd = open(node, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open streaming_data error.\n");
		LOGE("open streaming_data error.\n");
		goto exit;
	}
	snprintf(node, MAX_STR, "%s%s/streaming_data_size", sensor_hub_sysfs_path, node_name[sensor_id]);
	fd2 = open(node, O_RDONLY);
	if (fd2 < 0) {
		fprintf(stderr, "open streaming_data_size error.\n");
		LOGE("open streaming_data_size error.\n");
		goto close;
	}
again:
        lseek(fd2, 0, SEEK_SET);
	ret = read(fd2, tmp, MAX_STR);
	if (ret <= 0)
		goto close1;
	sscanf(tmp, "%d", &data);
	LOGV(" data %d", data);
	if (!data) { 
		fds[0].fd = fd2;
		fds[0].events = POLLPRI;

		LOGV("blocked for data");
		/* blocked for data */
		ret = poll(fds, 1, timeout);
		if (ret <= 0)
			goto close1;
		goto again;
	}
	LOGV("size %d\n",size);
	while (size) {
		ret = read(fd, buf, size);
		if (ret > 0) {
			LOGV("Read %d bytes from fd",ret);
			buf += ret;
			size -= ret;
			total += ret;
		} else {
			LOGV(" 0 data read from fd");
			break;
		}
	}
	ret = total;
	
close1:
	close(fd2);
close:
	close(fd);
exit:
        LOGV("Get streaming data returning %d", ret);
	return ret;

}

int sensor_hub_add_event(int mode, int cond_reset, int retrigger_time, int wakeup_ia, const char *cond_array)
{
	int ret;
	char buf[MAX_STR];
	int len;

	len = snprintf(buf, MAX_STR,
		"%d %d %d %d, %s ", mode, cond_reset, retrigger_time, wakeup_ia, cond_array ? cond_array : "");
	ret = sensor_hub_write_node("sensor_hub/add_event", buf, len);
	if (ret <= 0) {
		fprintf(stderr, "sensor_hub add_event error.\n");
		return -1;
	}
	ret = sensor_hub_read_node("sensor_hub/last_event", buf, MAX_STR);
	if (ret <= 0) {
		fprintf(stderr, "sensor_hub get event id error.\n");
		return -1;
	}

	return atoi(buf);
}

int sensor_hub_remove_event(int eid)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/clear_event", eid);
	if (ret <= 0) {
		fprintf(stderr, "sensor hub clear event error.\n");
		return -1;
	}
	return 0;
}

int sensor_hub_clear_all_triggered_events(void)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/clear_all_triggered_events", 1);
	if (ret <= 0) {
		fprintf(stderr, "sensor hub clear all events error.\n");
		return -1;
	}
	return ret;
}

int sensor_hub_clear_triggered_event(int eid)
{
	int ret;

	ret = sensor_hub_write_node_uint("sensor_hub/clear_triggered_event", eid);
	if (ret <= 0) {
		fprintf(stderr, "sensor hub clear all events error.\n");
		return -1;
	}
	return ret;
}

int sensor_hub_wait_on_event(int eids[], int out_mask[], int count, int timeout)
{
	char node[MAX_STR];
	char tmp[MAX_STR];
	char *ptr;
	struct pollfd fds[1];
	int fd;
	int eid,time;
	int ret = -1;
	int triggered = 0;

	snprintf(node, MAX_STR, "%s/sensor_hub/triggered_events", sensor_hub_sysfs_path);
	fd = open(node, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open triggered_events error.\n");
		goto exit;
	}
	memset(out_mask, 0, count * sizeof(int));
again:
	*tmp='\0';
	ret = read(fd, tmp, MAX_STR);
	if (ret < 0)
		goto close;
	ptr = strtok(tmp, ",");
	while (ptr) {
		ret = sscanf(ptr, "%d@%d", &eid, &time);
		if (ret == 2) {
			int loop;

			for (loop = 0; loop < count; loop++)
				if (eids[loop] == eid) {
					out_mask[loop] = 1;
					triggered = 1;
					sensor_hub_clear_triggered_event(eid);
				}
		}
		ptr = strtok(NULL, ",");
	}
	if (triggered)
		return 1;
	fds[0].fd = fd;
	fds[0].events = POLLPRI;

	/* blocked for data */
	ret = poll(fds, 1, timeout);
	if (ret <= 0)
		goto close;
	lseek(fd, 0, SEEK_SET);
	goto again;
close:
	close(fd);
exit:
	return ret;
}


int sensor_hub_is_streaming()
{
  char buf[MAX_STR];
  int result;
  
  /* Sensor hub should be locked by calling function */
  sensor_hub_read_node("sensor_hub/start_streaming", buf, MAX_STR);
  sscanf(buf, "%d", &result);
  return result;
}
