LOCAL_PATH := $(call my-dir)

# libvorbis
include $(CLEAR_VARS)
LOCAL_MODULE := vorbis
LOCAL_C_INCLUDES := ../../../include \
					../../../lib \
					../../../../../Ogg/libogg-1.2.2/include
LOCAL_SRC_FILES :=  ../../../lib/analysis.c \
					../../../lib/bitrate.c \
					../../../lib/block.c \
					../../../lib/codebook.c \
					../../../lib/envelope.c \
					../../../lib/floor0.c \
					../../../lib/floor1.c \
					../../../lib/info.c \
					../../../lib/lookup.c \
					../../../lib/lpc.c \
					../../../lib/lsp.c \
					../../../lib/mapping0.c \
					../../../lib/mdct.c \
					../../../lib/psy.c \
					../../../lib/registry.c \
					../../../lib/res0.c \
					../../../lib/sharedbook.c \
					../../../lib/smallft.c \
					../../../lib/synthesis.c \
					../../../lib/vorbisenc.c \
					../../../lib/window.c 

# Optional compiler flags.
LOCAL_CFLAGS   = -DLIBVORBIS_EXPORTS -fno-stack-protector

include $(BUILD_STATIC_LIBRARY)

# libvorbisfile
include $(CLEAR_VARS)
LOCAL_MODULE := vorbisfile
LOCAL_C_INCLUDES := ../../../include \
					../../../lib \
					../../../../../Ogg/libogg-1.2.2/include
LOCAL_SRC_FILES :=  ../../../lib/vorbisfile.c

# Optional compiler flags.
LOCAL_CFLAGS   = -DLIBVORBIS_EXPORTS -fno-stack-protector

include $(BUILD_STATIC_LIBRARY)

# libvorbisenc
#include $(CLEAR_VARS)
#LOCAL_MODULE := vorbisenc
#LOCAL_C_INCLUDES := ../../include \
#					../../lib \
#					../../../../Ogg/libogg-1.2.2/include
#
#LOCAL_SRC_FILES :=  ../../../examples/encoder_example.c
#
## Optional compiler flags.
#LOCAL_CFLAGS   = -DLIBVORBIS_EXPORTS -fno-stack-protector
#
#include $(BUILD_STATIC_LIBRARY)
