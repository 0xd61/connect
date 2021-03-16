#!/usr/bin/env bash

# Stop at errors
set -e

NO_RELEASE=1

CommonCompilerFlags="-O3 -g -ggdb -fdiagnostics-color=always -std=c++11 -fno-rtti -fno-exceptions -ffast-math -msse4.1 -msse2
-Wall -Werror -Wconversion
-Wno-writable-strings -Wno-gnu-anonymous-struct
-Wno-padded -Wno-string-conversion
-Wno-error=sign-conversion -Wno-incompatible-function-pointer-types
-Wno-error=unused-variable -Wno-unused-function
-Wno-error=unused-command-line-argument"

CommonDefines="-DZHC_BIG_ENDIAN=0 -DZHC_DEBUG=${NO_RELEASE} -DZHC_INTERNAL=${NO_RELEASE} -DOS_NAME=${OS_NAME}"

# the goal should be -nostdlib
# TODO(dgl): link sdl2_net statically
CommonLinkerFlags="-Wl,--gc-sections -lm -lSDL2_net"

if [ -z "$1" ]; then
    OS_NAME=$(uname -o 2>/dev/null || uname -s)
else
    OS_NAME="$1"
fi

echo "Building for $OS_NAME..."

curDir=$(pwd)
srcDir="$curDir/src"
buildDir="$curDir/build"
dataDir="$curDir/data"

[ -d $buildDir ] || mkdir -p $buildDir
[ -d $dataDir ] || mkdir -p $dataDir

pushd $buildDir > /dev/null

# NOTE(dgl): currently only x64 code. For other architectures we have to adjust the intrinsics.
if [ "$OS_NAME" == "GNU/Linux" ] || \
   [ "$OS_NAME" == "Linux" ] || \
   [ "$OS_NAME" == "linux" ]; then
    mkdir -p linux

    echo "Building tests"
    clang++ $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/test_sdl2_api_x64 $srcDir/sdl2_api_test.cpp \
    `sdl2-config --static-libs` -pg
    clang++ $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/test_zhc_net_x64 $srcDir/zhc_net_test.cpp \
    `sdl2-config --static-libs` -pg
    clang++ $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/test_zhc_asset_x64 $srcDir/zhc_asset_test.cpp \
    `sdl2-config --static-libs` -pg
    echo "Testing:"
    ./linux/test_sdl2_api_x64
    ./linux/test_zhc_net_x64
    ./linux/test_zhc_asset_x64

    # PIC = Position Independent Code
    # -lm -> we have to link the math library...
    echo "Building application"
    clang++ $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/server_main_x64 $srcDir/server_main.cpp \
    `sdl2-config --static-libs` -pg

    clang++ $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/client_main_x64 $srcDir/client_main.cpp \
    `sdl2-config --static-libs` -pg

    cp -r $dataDir/assets/* linux/
elif [ "$OS_NAME" == "Android" ] || \
     [ "$OS_NAME" == "android" ]; then

    SDLVERSION="SDL2-2.0.14"
    SDLPATH="$curDir/$SDLVERSION"
    SDLNETVERSION="SDL2_net-2.0.1"
    SDLNETPATH="$curDir/$SDLNETVERSION"
    ANDROID_PATH="$curDir/sdl2_android"

    [ -d $SDLPATH ] || curl https://www.libsdl.org/release/$SDLVERSION.tar.gz | tar xzf -
    [ -d $SDLVERSION ] && mv $SDLVERSION $SDLPATH

    [ -d $SDLNETPATH ] || curl https://libsdl.org/projects/SDL_net/release/$SDLNETVERSION.tar.gz | tar xzf -
    [ -d $SDLNETVERSION ] && mv $SDLNETVERSION $SDLNETPATH

    mkdir -p $ANDROID_PATH/app/jni/SDL
    ln -sfn $SDLPATH/Android.mk $ANDROID_PATH/app/jni/SDL/Android.mk
    ln -sfn $SDLPATH/src $ANDROID_PATH/app/jni/SDL/src
    ln -sfn $SDLPATH/include $ANDROID_PATH/app/jni/SDL/include
    ln -sfn $SDLNETPATH $ANDROID_PATH/app/jni/SDLNet
    ln -sfn $SDLPATH/android-project/app/src/main/java/org $ANDROID_PATH/app/src/main/java/org
    mkdir -p $ANDROID_PATH/app/jni/src
    ln -sfn $srcDir/* $ANDROID_PATH/app/jni/src/
    ln -sfn $curDir/Android.mk $ANDROID_PATH/app/jni/src/Android.mk
    ln -sfn $dataDir/assets $ANDROID_PATH/app/src/main/

    echo "Created symlinks to sources. Please use android studio for the build"
else
    echo "$OS_NAME is currently not supported"
fi

popd > /dev/null
