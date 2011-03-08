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

#ifndef _H_SENSOR_HUB_H
#define _H_SENSOR_HUB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned u32;

enum sensor_id {
	ACCEL_VALUE,
	ACCEL_MAG,
	GYRO_VALUE,
	GYRO_MAG,
	GYRO_TEMP_ID,
	COMPASS_VALUE,
	COMPASS_MAG,
	AUDIO_RAW,
	AUDIO_NO_DC,
	ALT_PRE,
	ALT_TEMP,
	LIGHT_AMB,
	LIGHT_IR,
	ORIENTATION_XY_ID,
	ORIENTATION_Z_ID,
	HEADING_ID,
	NOISE_ID,
	SENSOR_ID_MAX,
};

enum data_format {
	DATA_RAW,
	DATA_CALCULATED,
};

enum condition_relation {
	CONDITION_AND,
	CONDITION_OR,
};

enum condition_type {
	ABOVE_THRESHOLD,
	BELOW_THRESHOLD,
	EQUAL_THRESHOLD,
	ABSOLUTE_ABOVE_THRESHOLD,
	ABSOLUTE_BELOW_THRESHOLD,
	IN_RANGE,
	OUT_OF_RANGE,
};

enum channel_id {
	CHANNEL1 = 0x1,
	CHANNEL2 = 0x2,
	CHANNEL3 = 0x4,
};

enum function_id {
	FUNCTION_NONE,
	FUNCTION_AVG,
	FUNCTION_MAX,
	FUNCTION_MIN,
	FUNCTION_DELTA,
	FUNCTION_VARIANCE,
	FUNCTION_MAG,
	FUNCTION_AUDIO_ENERGY,
	FUNCTION_GESTURE_SPOTTER,
	FUNCTION_FFT,
	FUNCTION_FIR,
};

enum event_mode {
	TRIGGER_ONCE,
	RETRIGGER,
};

enum event_reset {
	WAIT_NONE,
	WAIT_RESET,
};

enum event_wakeup {
	WAKEUP_ALWAYS,
	ONLY_IA_ACTIVE,
};

int init_sensor_hub(void);
int reset_sensor_hub(void);
int sensor_hub_set_time(u32 time);
int sensor_hub_get_time(u32 *time);
int sensor_hub_set_buffer_delay_msec(int msec);
int sensor_hub_get_single_sample(int sensor_id, char *buf, size_t size);
int sensor_hub_set_sample_rate(int sensor_id, int rate);
int sensor_hub_set_sample_range(int sensor_id, int range);
int sensor_hub_set_sample_number(int sensor_id, int number);
int sensor_hub_set_data_format(int sensor_id, int format);
int sensor_hub_set_capture_mode(int sensor_id, int enable);
int sensor_hub_set_streaming_mode(int sensor_id, int enable);
/* composite the header of condition array */
int sensor_hub_init_condition_array(char *str, int num, int type);
/* composite function parameter string */
int sensor_hub_init_parameter_string(char *str, int parameter_number, int p1, int p2);
/* composite the condition string will feed to sensor_hub_add_condition_string */
int sensor_hub_init_condition_string(char *str, int type, int sid, int thd_min, int thd_max, int data_format, int function_id, int mask, const char *parameter);
/* append the condition string to condition array */
int sensor_hub_add_condition_string(char *str, const char *cond);
int sensor_hub_set_capture_condition(int sensor_id, const char *capture_array);
int sensor_hub_init_delivery_string(char *str, int function_id, const char *parameter, const char *cond_array);
int sensor_hub_add_delivery_string(char *str, const char *delivery);
int sensor_hub_set_delivery_config(int sensor_id, const char *delivery_array);
/* set whole capture config */
int sensor_hub_start_capture(int enable);
/* start streaming */
int sensor_hub_start_streaming(int enable);
/* start store/forwarding */
int sensor_hub_start_store_forwarding(int enable);
int sensor_hub_retrieve_stored_data(int stime, int etime);
int sensor_hub_get_streaming_data(int sensor_id, char *buf, size_t size, int timeout);
int sensor_hub_get_streaming_data_size(int sensor_id, int timeout);
/* add a event */
int sensor_hub_add_event(int mode, int cond_reset, int retrigger_time, int wakeup_ia, const char *cond_array);
/* remove a event */
int sensor_hub_remove_event(int eid);
int sensor_hub_clear_all_triggered_events(void);
int sensor_hub_clear_triggered_event(int eid);
int sensor_hub_wait_on_event(int eids[], int out_mask[], int count, int timeout);

int sensor_hub_is_streaming();


#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif
