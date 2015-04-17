#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <cutils/log.h>
#include <hardware/hardware.h>
#include <awarelibs/aware.h>
#include "message.h"
#include "lpe.h"

#define LPE_DEBUG false

#define FILT_COEFF_QFACTOR      15
#define ALPHA_QFACTOR           14
#define NUM_FRAME_SPER_BLOCK    64
#define NUM_FRAME_SPER_ANALYSIS 4

using namespace android;

static aware_classifier_param_t aware_classifier = {
        framesize: 128,
        mfccorder: 12,
        gmmorder: 20,
        fborder: 24,
        minNumFrms: 30 * NUM_FRAME_SPER_ANALYSIS * NUM_FRAME_SPER_BLOCK / 100,
        thresholdSpeech: -2,
        thresholdNonspeech: -1,
        dB_offset: 21,
        alpha: 0.95 * (1 << ALPHA_QFACTOR),
        rastacoeff_a: 0.98 * (1 << FILT_COEFF_QFACTOR),
        rastacoeff_b0: 0.2 * (1 << FILT_COEFF_QFACTOR),
        rastacoeff_b1: 0.1 * (1 << FILT_COEFF_QFACTOR),
        numframesperblock: NUM_FRAME_SPER_BLOCK,
        numblocksperanalysis: NUM_FRAME_SPER_ANALYSIS,
};

static const hw_module_t *module;
static aware_hw_device_t *device;

static int pipefd[2] = { -1, -1 };

// Using C89 style to avoid "outside aggregate intializer" in C++
static const sensor_info_t lpe_sensor_info = {
        "LPE_P",                // name
        "Intel Inc.",           // vendor
        SENSOR_LPE,             // sensor_type
        USE_CASE_CSP,           // use_case
        0,                      // is_wake_sensor
        1,                      // version
        0,                      // min_delay
        0,                      // max_delay
        0,                      // fifo_max_event_count
        0,                      // fifo_reserved_event_count
        1,                      // axis_num
        {1, 1, 1, 1, 1, 1},     // axis_scale
        1,                      // max_range
        1,                      // resolution
        0.35,                   // power
        NULL,                   // plat_data
};

static bool activated = false;

static int64_t getTimestamp()
{
        struct timespec t = { 0, 0 };
        clock_gettime(CLOCK_BOOTTIME, &t);
        return static_cast<int64_t>(t.tv_sec)*1000000000LL + static_cast<int64_t>(t.tv_nsec);
}

static void on_lpe_changed(const aware_classifier_result_t aware_result)
{
        struct lpe_phy_data lpe_data;
        int ret;

        if(!activated)
                return;

        ALOGD_IF(LPE_DEBUG, "%s line: %d LPE: classid=%d dBval=%d", __FUNCTION__, __LINE__, aware_result.classid, aware_result.dBval);
        lpe_data.ts = getTimestamp();
        lpe_data.lpe_msg = aware_result.classid << 16 | aware_result.dBval;

        ret = write(pipefd[1], &lpe_data, sizeof(struct lpe_phy_data));
        if (ret < 0) {
                ALOGE("%s line: %d write LPE data error: %s", __FUNCTION__, __LINE__, strerror(errno));
        }
}

int lpe_init(void *p_sensor_list, unsigned int *index)
{
        status_t result;
        sensor_state_t *lpe_sensor = reinterpret_cast<sensor_state_t *>(p_sensor_list) + *index;

        lpe_sensor->sensor_info = const_cast<sensor_info_t *>(&lpe_sensor_info);
        lpe_sensor->index = *index;
        (*index)++;

        ALOGD_IF(LPE_DEBUG, "LPE: %s line: %d", __FUNCTION__, __LINE__);
        result = hw_get_module_by_class(AWARE_HARDWARE_MODULE_ID, AWARE_HARDWARE_MODULE_ID_PRIMARY, &module);
        if (result != NO_ERROR) {
                ALOGE("%s line: %d load LPE module error: %d", __FUNCTION__, __LINE__, result);
                return -2;
        }

        return 0;
}

int lpe_add_fd(int maxfd, void *read_fds, int *hw_fds, int *hw_fds_num)
{
	int fd = lpe_get_fd();

	if (fd < 0)
		return maxfd;

	FD_SET(fd, reinterpret_cast<fd_set *>(read_fds));

	hw_fds[*hw_fds_num] = fd;

	(*hw_fds_num)++;

	return fd > maxfd ? fd : maxfd;
}

int lpe_process_fd(int fd)
{
        struct cmd_resp *p_cmd_resp;

        if (fd != pipefd[0])
                return 0;

        ALOGD_IF(LPE_DEBUG, "LPE: %s line: %d", __FUNCTION__, __LINE__);
        p_cmd_resp = (struct cmd_resp *)malloc(sizeof(struct cmd_resp) + sizeof(struct lpe_phy_data));
        if (p_cmd_resp == NULL) {
                ALOGE("LPE: %s line: %d malloc error!", __FUNCTION__, __LINE__);
                return -2;
        }
        p_cmd_resp->cmd_type = RESP_STREAMING;
        p_cmd_resp->data_len = sizeof(struct lpe_phy_data);
        p_cmd_resp->sensor_type = SENSOR_LPE;

        read(fd, p_cmd_resp->buf, sizeof(struct lpe_phy_data));

        dispatch_streaming(p_cmd_resp);

        return 0;
}

int lpe_get_fd() {
        if (pipefd[0] < 0) {
                if (pipe(pipefd) < 0) {
                        ALOGE("%s line: %d pipe error: %s", __FUNCTION__, __LINE__, strerror(errno));
                        return -1;
                }

                aware_register_callback(on_lpe_changed);
        }
        ALOGD_IF(LPE_DEBUG, "LPE: %s line: %d pipefd[0]: %d pipefd[1]: %d", __FUNCTION__, __LINE__, pipefd[0], pipefd[1]);
        return pipefd[0];
}

error_t lpe_start_streaming(int data_rate, int buffer_delay) {
        status_t result;
        DelayMode mode = static_cast<DelayMode>(data_rate);

        if (mode != NONE && mode == INSTANT && mode != ON_CHANGE && mode != DYMAMIC_INTERVAL) {
                ALOGE("%s line: %d invaild DelayMode: %d", __FUNCTION__, __LINE__, data_rate);
                return ERROR_NOT_AVAILABLE;
        }

        ALOGD_IF(LPE_DEBUG, "LPE: %s line: %d", __FUNCTION__, __LINE__);
        result = aware_hw_device_open(module, &device);
        if (result != NO_ERROR) {
                ALOGE("%s line: %d open LPE device error: %d", __FUNCTION__, __LINE__, result);
                return ERROR_NOT_AVAILABLE;
        }

        result = activate_aware_session(device, &aware_classifier);
        if (result != NO_ERROR) {
                ALOGE("%s line: %d activate_aware_session error: %d", __FUNCTION__, __LINE__, result);
                if (device != NULL) {
                        aware_hw_device_close(device);
                        device == NULL;
                }
                return ERROR_NOT_AVAILABLE;
        }

        aware_setDelay(device, mode, buffer_delay);

        activated = true;

        return ERROR_NONE;
}

error_t lpe_stop_streaming() {
        status_t result;
        error_t ret = ERROR_NONE;

        ALOGD_IF(LPE_DEBUG, "LPE: %s line: %d", __FUNCTION__, __LINE__);
        result = deactivate_aware_session(device);
        if (result != NO_ERROR) {
                ALOGD("%s line: %d deactivate_aware_session error: %d", __FUNCTION__, __LINE__, result);
                ret = ERROR_NOT_AVAILABLE;
        }

        aware_hw_device_close(device);
        device == NULL;

        activated = false;

        return ret;
}

#ifdef SUPPORT_ANDROID_SENSOR_HAL
error_t lpe_flush_streaming() {
        // Todo: must write flush complete event to pipe asynchronous.
        // Also need to add lock to where writing the pipe.
        return ERROR_NONE;
}
#endif
