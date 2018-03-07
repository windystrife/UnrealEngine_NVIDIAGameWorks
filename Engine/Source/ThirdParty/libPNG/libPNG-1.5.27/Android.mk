LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=\
	png.c \
	pngerror.c \
	pngget.c \
	pngmem.c \
	pngpread.c \
	pngread.c \
	pngrio.c \
	pngrtran.c \
	pngrutil.c \
	pngset.c \
	pngtest.c \
	pngtrans.c \
	pngwio.c \
	pngwrite.c \
	pngwtran.c \
	pngwutil.c

LOCAL_MODULE := libpng
LOCAL_CLANG := true
LOCAL_CFLAGS := -O2 -Os -DNDEBUG -fomit-frame-pointer -g0
ifeq ($(TARGET_ARCH_ABI),x86)
	LOCAL_CFLAGS += -fno-stack-protector
endif
ifeq ($(TARGET_ARCH_ABI),x86_64)
	LOCAL_CFLAGS += -fno-stack-protector
endif
LOCAL_EXPORT_LDLIBS := -lz
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/.
LOCAL_SDK_VERSION := 9
include $(BUILD_STATIC_LIBRARY)
