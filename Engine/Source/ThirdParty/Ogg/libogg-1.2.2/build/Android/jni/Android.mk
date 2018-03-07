LOCAL_PATH := $(call my-dir)

# Define vars for library that will be build statically.
include $(CLEAR_VARS)
LOCAL_MODULE := ogg
LOCAL_C_INCLUDES := ../../../include
LOCAL_SRC_FILES :=  ../../../src/bitwise.c \
					../../../src/framing.c

# Optional compiler flags.
LOCAL_CFLAGS   = -DLIBOGG_EXPORTS

include $(BUILD_STATIC_LIBRARY)
