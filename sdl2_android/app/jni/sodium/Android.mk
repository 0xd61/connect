LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := sodium

JNILIBS_PATH := ../../jniLibs
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/src/lib/sodium
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(JNILIBS_PATH)/$(TARGET_ARCH_ABI)/sodium/libsodium.so

include $(PREBUILT_SHARED_LIBRARY)
