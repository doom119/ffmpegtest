LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := FFmpegTest
LOCAL_SRC_FILES := FFmpegTest2.c

LOCAL_CFLAGS := -O0 -DANDROID -Wno-deprecated-declarations
LOCAL_LDLIBS := -llog -lz -lm

LOCAL_SHARED_LIBRARIES := libavformat libavcodec libswscale libavutil libswresample

include $(BUILD_SHARED_LIBRARY)
$(call import-module, ffmpeg-2.5.4/android/arm)
