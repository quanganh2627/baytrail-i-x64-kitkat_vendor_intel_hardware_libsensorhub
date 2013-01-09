LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := psh.bin
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := psh.bin.rc121130
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#
# sensorhubd - sensorhub daemon
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := src/daemon/main.c \
			src/utils/utils.c

LOCAL_SHARED_LIBRARIES := 

LOCAL_MODULE := sensorhubd

include $(BUILD_EXECUTABLE)

#
# libsensorhub - sensorhub client library
#
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := src/lib/libsensorhub.c \
			src/utils/utils.c

LOCAL_MODULE := libsensorhub

include $(BUILD_SHARED_LIBRARY)

#
# copy header file
#
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsensorhub
LOCAL_COPY_HEADERS := src/include/libsensorhub.h

include $(BUILD_COPY_HEADERS)

#
# sensorhub_client - sensorhub test client
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := src/tests/sensor_hub_client.c

LOCAL_SHARED_LIBRARIES += libsensorhub liblog
LOCAL_MODULE := sensorhub_client

include $(BUILD_EXECUTABLE)

#
# test.c from Alek
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := src/tests/test.c

# LOCAL_SHARED_LIBRARIES += libsensorhub
LOCAL_MODULE := test_alek

#
# compass calibration test.
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := src/tests/calibration.c

LOCAL_SHARED_LIBRARIES += libsensorhub

LOCAL_MODULE	:= calibration

include $(BUILD_EXECUTABLE)

#
# event notification test.
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := src/tests/event_notification.c

LOCAL_SHARED_LIBRARIES += libsensorhub liblog

LOCAL_MODULE    := event_notification

include $(BUILD_EXECUTABLE)

