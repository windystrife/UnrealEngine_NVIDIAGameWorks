LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := bsdsignal
LOCAL_SRC_FILES := bsdsignal.c

include $(BUILD_STATIC_LIBRARY)
