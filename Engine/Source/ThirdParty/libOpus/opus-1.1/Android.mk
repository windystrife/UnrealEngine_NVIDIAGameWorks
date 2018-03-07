##
# To compile libopus for Android run
#  localpath/ndk-build NDK_APPLICATION_MK=Application.mk NDK_PROJECT_PATH=build
# This will place static libs under a directory called 'build'.
# Once complete move them to an appropriate location.
##


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/celt_sources.mk
include $(LOCAL_PATH)/opus_sources.mk
include $(LOCAL_PATH)/silk_sources.mk

LOCAL_MODULE    := libopus
OGG_DIR         := external/libogg

APP_ABI := armeabi arm64-v8a x86_64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/src $(LOCAL_PATH)/silk \
                    $(LOCAL_PATH)/celt $(LOCAL_PATH)/silk/fixed $(OGG_DIR)/include
LOCAL_SRC_FILES := $(CELT_SOURCES) $(SILK_SOURCES) $(SILK_SOURCES_FIXED) \
                   $(OPUS_SOURCES) $(OPUS_SOURCES_FLOAT) src/repacketizer_demo.c

LOCAL_CFLAGS        := -DNULL=0 -DSOCKLEN_T=socklen_t -DLOCALE_NOT_USED \
                       -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
                       -Drestrict='' -D__EMX__ -DOPUS_BUILD -DFIXED_POINT \
                       -DUSE_ALLOCA -DHAVE_LRINT -DHAVE_LRINTF -O2 -fno-math-errno
LOCAL_CPPFLAGS      := -DBSD=1 -ffast-math -O2 -funroll-loops

# Note: OPUS enhanced DSP/NEON implementation is not yet compatible with arm64.
# Only add the appropriate defines for 32-bit arm architecture.
LOCAL_SRC_FILES_arm += $(CELT_SOURCES_ARM)
LOCAL_CFLAGS_arm += -DOPUS_ARM_ASM -DOPUS_ARM_INLINE_ASM \
                    -DOPUS_ARM_MAY_HAVE_EDSP -DOPUS_ARM_INLINE_EDSP \
                    -DOPUS_ARM_MAY_HAVE_MEDIA -DOPUS_ARM_INLINE_MEDIA \
                    -DOPUS_HAVE_RTCD -DANDROID
# DSP, MEDIA and NEON instructions are in the same assembler file - thus we
# need to include it even if NEON is not supported on target platform.
LOCAL_SRC_FILES_arm += $(subst .s,_gnu.s,$(CELT_SOURCES_ARM_ASM))
ifeq ($(ARCH_ARM_HAVE_NEON),true)
LOCAL_SRC_FILES_arm += $(CELT_SOURCES_ARM_NEON_INTR) \
                       $(SILK_SOURCES_ARM_NEON_INTR)
LOCAL_CFLAGS_arm += -DOPUS_ARM_MAY_HAVE_NEON -DOPUS_ARM_MAY_HAVE_NEON_INTR \
                    -DOPUS_ARM_PRESUME_NEON -DOPUS_ARM_INLINE_NEON
endif

LIBOPUS_X86_FILES :=
LIBOPUS_X86_CFLAGS :=
ifeq ($(ARCH_X86_HAVE_SSSE3),true)
LIBOPUS_X86_FILES += $(CELT_SOURCES_SSE) $(CELT_SOURCES_SSE2)
LIBOPUS_X86_CFLAGS += -DOPUS_X86_MAY_HAVE_SSE -DOPUS_X86_PRESUME_SSE \
                      -DOPUS_X86_MAY_HAVE_SSE2 -DOPUS_X86_PRESUME_SSE2
endif

ifeq ($(ARCH_X86_HAVE_SSE4_1),true)
LIBOPUS_X86_FILES += $(CELT_SOURCES_SSE4_1) \
                   $(SILK_SOURCES_SSE4_1) $(SILK_SOURCES_FIXED_SSE4_1)
LIBOPUS_X86_CFLAGS += -DOPUS_X86_MAY_HAVE_SSE4_1 -DOPUS_X86_PRESUME_SSE4_1
endif

LOCAL_SRC_FILES_x86 += $(LIBOPUS_X86_FILES)
LOCAL_SRC_FILES_x86_64 += $(LIBOPUS_X86_FILES)
LOCAL_CFLAGS_x86 += $(LIBOPUS_X86_CFLAGS)
LOCAL_CFLAGS_x86_64 += $(LIBOPUS_X86_CFLAGS)

LOCAL_STATIC_LIBRARIES := libogg

include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	speex_resampler/resample.c
LOCAL_MODULE:= libspeex_resampler
LOCAL_MODULE_TAGS := optional

ifeq ($(ARCH_ARM_HAVE_NEON),true)
	LOCAL_CFLAGS += -D_USE_NEON -DANDROID
endif
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/src $(LOCAL_PATH)/silk \
                    $(LOCAL_PATH)/celt $(LOCAL_PATH)/silk/fixed $(OGG_DIR)/include $(LOCAL_PATH)/speex_resampler
include $(BUILD_STATIC_LIBRARY)