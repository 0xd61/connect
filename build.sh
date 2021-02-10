#!/usr/bin/env bash

# Stop at errors
set -e

CommonCompilerFlags="-O0 -g -ggdb -fdiagnostics-color=always -std=c++11 -fno-rtti -fno-exceptions -ffast-math -msse4.1
-Wall -Werror -Wconversion
-Wno-writable-strings -Wno-gnu-anonymous-struct
-Wno-padded -Wno-string-conversion
-Wno-error=sign-conversion -Wno-incompatible-function-pointer-types
-Wno-error=unused-variable -Wno-unused-function
-Wno-error=unused-command-line-argument"

CommonDefines="-DZHC_DEBUG=1 -DZHC_INTERNAL=1 -DOS_NAME=${OS_NAME}"

# the goal should be -nostdlib
CommonLinkerFlags="-Wl,--gc-sections -lm"

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
    # PIC = Position Independent Code
    # -lm -> we have to link the math library...
    clang++ $CommonCompilerFlags $CommonDefines $CommonLinkerFlags -o main_x64 $srcDir/main.cpp \
    `sdl2-config --static-libs`
elif [ "$OS_NAME" == "Android" ] || \
     [ "$OS_NAME" == "android" ]; then
    # TODO(dgl): Android build not yet tested!
    $curDir/_androidbuild.sh "co.degit.connect"
else
    echo "$OS_NAME is currently not supported"
fi

popd > /dev/null
