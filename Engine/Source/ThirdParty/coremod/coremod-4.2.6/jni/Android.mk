LOCAL_PATH	:= $(call my-dir)/..

include $(CLEAR_VARS)

include $(LOCAL_PATH)/src/Makefile
include $(LOCAL_PATH)/src/loaders/Makefile

SRC_SOURCES	:= $(addprefix src/,$(SRC_OBJS))
LOADERS_SOURCES := $(addprefix src/loaders/,$(LOADERS_OBJS))

LOCAL_MODULE    := xmp-coremod
LOCAL_CFLAGS	:= -O3 -I$(LOCAL_PATH)/include/coremod -I$(LOCAL_PATH)/src \
		   -DLIBXMP_CORE_PLAYER
LOCAL_SRC_FILES := $(SRC_SOURCES:.o=.c.arm) \
		   $(LOADERS_SOURCES:.o=.c)

include $(BUILD_STATIC_LIBRARY)
