LOCAL_PATH := $(call my-dir)

ifneq (,$(findstring $(CUSTOM_BOARD), mfld_pr1))

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_SRC_FILES :=libsensorhub.c
LOCAL_MODULE :=libsensorhub
LOCAL_MODULE_TAGS := optional
LOCAL_COPY_HEADERS_TO := libsensorhub
LOCAL_COPY_HEADERS := libsensorhub.h
# LOCAL_MODULE_TAGS := debug
include $(BUILD_SHARED_LIBRARY)
ALL_PREBUILT += $(copy_to)

endif
