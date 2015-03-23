ifeq ($(ENABLE_SENSOR_HUB_ISH),true)

LOCAL_PATH := $(call my-dir)

#
# sensorhubd - sensorhub daemon
#
include $(CLEAR_VARS)
LOCAL_MODULE := sensorhubd
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_SRC_FILES := \
    daemon/main.c \
    daemon/generic_sensors.c \
    utils/utils.c
LOCAL_SHARED_LIBRARIES := liblog libhardware_legacy libcutils
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

#ifeq ($(strip $(INTEL_FEATURE_AWARESERVICE)),true)
#LOCAL_SHARED_LIBRARIES += libcontextarbitor
#LOCAL_CFLAGS := -DENABLE_CONTEXT_ARBITOR
#endif

include $(BUILD_EXECUTABLE)

#
# libsensorhub - sensorhub client library
#
include $(CLEAR_VARS)
LOCAL_MODULE := libsensorhub
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    lib/libsensorhub.c \
    utils/utils.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
include $(BUILD_SHARED_LIBRARY)

#
# sensorhub_client - sensorhub test client
#
include $(CLEAR_VARS)
LOCAL_MODULE := sensorhub_client
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := tests/sensor_hub_client.c
LOCAL_SHARED_LIBRARIES := libsensorhub liblog
include $(BUILD_EXECUTABLE)

#
# androidcalibrationtool - calibration tool for AGM
#
include $(CLEAR_VARS)
LOCAL_MODULE := AndroidCalibrationTool
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_SRC_FILES := calibrationtool/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

endif
