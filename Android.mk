LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
SDLnet_PATH := ../SDLNet

# LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/$(SDLnet_PATH)
# LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDLnet_PATH)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src/lib
LOCAL_CFLAGS := -DZHC_DEBUG=0 -DZHC_INTERNAL=0

# Add your application source files here...
LOCAL_SRC_FILES :=  client_main.cpp

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_net sodium
# LOCAL_STATIC_LIBRARIES := sodium

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog -landroid

include $(BUILD_SHARED_LIBRARY)

