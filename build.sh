#!/usr/bin/env bash

# Stop at errors
set -e

NO_RELEASE=1

CommonCompilerFlags="-O0 -g -ggdb -fdiagnostics-color=always -std=c++11 -fno-rtti -fno-exceptions -ffast-math -msse4.1 -msse2
-Wall -Werror -Wconversion
-Wno-writable-strings -Wno-gnu-anonymous-struct
-Wno-padded -Wno-string-conversion
-Wno-error=sign-conversion -Wno-incompatible-function-pointer-types
-Wno-error=unused-variable -Wno-unused-function
-Wno-error=unused-command-line-argument"

CommonDefines="-DZHC_BIG_ENDIAN=0 -DZHC_DEBUG=${NO_RELEASE} -DZHC_INTERNAL=${NO_RELEASE} -DOS_NAME=${OS_NAME}"

# the goal should be -nostdlib
# TODO(dgl): link sdl2_net statically
CommonLinkerFlags="$(sdl2-config --libs) -Wl,--gc-sections -lm  -lSDL2_net -lsodium"

CommonIncludeFlags="$(sdl2-config --cflags) -L linux/sodium"

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

SDLVERSION="SDL2-2.0.14"
SDLNETVERSION="SDL2_net-2.0.1"
LIBSODIUMVERSION="libsodium-1.0.18"

[ -d $buildDir ] || mkdir -p $buildDir
[ -d $dataDir ] || mkdir -p $dataDir

pushd $buildDir > /dev/null

# NOTE(dgl): currently only x64 code. For other architectures we have to adjust the intrinsics.
if [ "$OS_NAME" == "GNU/Linux" ] || \
   [ "$OS_NAME" == "Linux" ] || \
   [ "$OS_NAME" == "linux" ]; then
    mkdir -p linux

    [ -d "$curDir/$LIBSODIUMVERSION" ] || curl https://download.libsodium.org/libsodium/releases/$LIBSODIUMVERSION.tar.gz | tar -C $curDir -xzf -
    pushd $curDir/$LIBSODIUMVERSION > /dev/null
        if [ ! -f $buildDir/linux/sodium/libsodium.a ]; then
            echo "Compiling libsodium"
            ./configure \
                --enable-minimal \
                --prefix=$buildDir/linux --includedir=$srcDir/lib --libdir=$buildDir/linux/sodium \
                --enable-shared=no --enable-static=yes || exit 1

            make && make check
            make install
            make clean
        fi
    popd > /dev/null

    echo "Building tests"
    clang++ $CommonIncludeFlags $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/test_sdl2_api_x64 $srcDir/sdl2_api_test.cpp \
    -pg $CommonLinkerFlags
    clang++ $CommonIncludeFlags $CommonCompilerFlags $CommonDefines -o linux/test_zhc_net_x64 $srcDir/zhc_net_test.cpp \
    -pg $CommonLinkerFlags
    clang++ $CommonIncludeFlags $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/test_zhc_asset_x64 $srcDir/zhc_asset_test.cpp \
    -pg $CommonLinkerFlags
    clang++ $CommonIncludeFlags $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o linux/test_zhc_renderer_x64 $srcDir/zhc_renderer_test.cpp \
    -pg $CommonLinkerFlags

    echo "Testing:"
    ./linux/test_sdl2_api_x64
    ./linux/test_zhc_net_x64
    ./linux/test_zhc_asset_x64
    ./linux/test_zhc_renderer_x64

    # PIC = Position Independent Code
    # -lm -> we have to link the math library...
    echo "Building application"
    clang++ $CommonIncludeFlags $CommonCompilerFlags $CommonDefines -o linux/server_main_x64 $srcDir/server_main.cpp \
    -pg $CommonLinkerFlags

    clang++ $CommonIncludeFlags $CommonCompilerFlags $CommonDefines -o linux/client_main_x64 $srcDir/client_main.cpp \
    -pg $CommonLinkerFlags

    cp -r $dataDir/assets/* linux/
elif [ "$OS_NAME" == "Android" ] || \
     [ "$OS_NAME" == "android" ]; then

    ANDROID_PATH="$curDir/sdl2_android"

    [ -d $curDir/$SDLVERSION ] || curl https://www.libsdl.org/release/$SDLVERSION.tar.gz | tar -C $curDir -xzf -
    [ -d $curDir/$SDLNETVERSION ] || curl https://libsdl.org/projects/SDL_net/release/$SDLNETVERSION.tar.gz | tar -C $curDir -xzf -

    # NOTE(dgl): setup libsodium for android
    mkdir -p $ANDROID_PATH/app/jniLibs
    [ -d "$curDir/$LIBSODIUMVERSION" ] || curl https://download.libsodium.org/libsodium/releases/$LIBSODIUMVERSION.tar.gz | tar -C $curDir -xzf -
    pushd $curDir/$LIBSODIUMVERSION > /dev/null
        echo "Compiling libsodium"
        if [ -z "${ANDROID_NDK_HOME}" ]; then
            echo "You should probably set ANDROID_NDK_HOME to the directory containing"
            echo "the Android NDK"
            popd > /dev/null
            popd > /dev/null
            exit 1
        fi

        TOOLCHAIN_DIR=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64

        export CC=$TOOLCHAIN_DIR/bin/armv7a-linux-androideabi21-clang
        export AR=$TOOLCHAIN_DIR/bin/llvm-ar
        export LDFLAGS='--specs=nosys.specs'
        export TARGET="armv7a-linux-androideabi"

        ./configure \
            --disable-soname-versions \
            --enable-minimal \
            --host="$TARGET" \
            --prefix=$ANDROID_PATH/app/jniLibs --includedir=$srcDir/lib --libdir=$ANDROID_PATH/app/jniLibs/armeabi-v7a/sodium \
            --with-sysroot="${TOOLCHAIN_DIR}/sysroot" --enable-shared=yes --enable-static=yes || exit 1

        make
        make install && make clean

        export CC=$TOOLCHAIN_DIR/bin/aarch64-linux-android21-clang
        export AR=$TOOLCHAIN_DIR/bin/llvm-ar
        export LDFLAGS='--specs=nosys.specs'
        export TARGET="aarch64-linux-android"

        ./configure \
            --disable-soname-versions \
            --enable-minimal \
            --host="$TARGET" \
            --prefix=$ANDROID_PATH/app/jniLibs --includedir=$srcDir/lib --libdir=$ANDROID_PATH/app/jniLibs/arm64-v8a/sodium \
            --with-sysroot="${TOOLCHAIN_DIR}/sysroot" --enable-shared=yes --enable-static=yes || exit 1

        make
        make install && make clean

        export CC=$TOOLCHAIN_DIR/bin/i686-linux-android21-clang
        export AR=$TOOLCHAIN_DIR/bin/llvm-ar
        export LDFLAGS='--specs=nosys.specs'
        export TARGET="i686-linux-android"

        ./configure \
            --disable-soname-versions \
            --enable-minimal \
            --host="$TARGET" \
            --prefix=$ANDROID_PATH/app/jniLibs --includedir=$srcDir/lib --libdir=$ANDROID_PATH/app/jniLibs/x86/sodium \
            --with-sysroot="${TOOLCHAIN_DIR}/sysroot" --enable-shared=yes --enable-static=yes || exit 1

        make
        make install && make clean

        export CC=$TOOLCHAIN_DIR/bin/x86_64-linux-android21-clang
        export AR=$TOOLCHAIN_DIR/bin/llvm-ar
        export LDFLAGS='--specs=nosys.specs'
        export TARGET="x86_64-linux-android"

        ./configure \
            --disable-soname-versions \
            --enable-minimal \
            --host="$TARGET" \
            --prefix=$ANDROID_PATH/app/jniLibs --includedir=$srcDir/lib --libdir=$ANDROID_PATH/app/jniLibs/x86_64/sodium \
            --with-sysroot="${TOOLCHAIN_DIR}/sysroot" --enable-shared=yes --enable-static=yes || exit 1

        make
        make install && make clean
    popd > /dev/null

    mkdir -p $ANDROID_PATH/app/jni/SDL
    ln -sfn $curDir/$SDLVERSION/Android.mk $ANDROID_PATH/app/jni/SDL/Android.mk
    ln -sfn $curDir/$SDLVERSION/src $ANDROID_PATH/app/jni/SDL/src
    ln -sfn $curDir/$SDLVERSION/include $ANDROID_PATH/app/jni/SDL/include
    ln -sfn $curDir/$SDLNETVERSION $ANDROID_PATH/app/jni/SDLNet
    ln -sfn $curDir/$SDLVERSION/android-project/app/src/main/java/org $ANDROID_PATH/app/src/main/java/org
    mkdir -p $ANDROID_PATH/app/jni/src
    ln -sfn $srcDir/* $ANDROID_PATH/app/jni/src/
    ln -sfn $curDir/Android.mk $ANDROID_PATH/app/jni/src/Android.mk
    ln -sfn $dataDir/assets $ANDROID_PATH/app/src/main/
    ln -sfn $curDir/AndroidManifest.xml $ANDROID_PATH/app/src/main/

    echo "Created symlinks to sources. Please use android studio for the build"
else
    echo "$OS_NAME is currently not supported"
fi

popd > /dev/null
