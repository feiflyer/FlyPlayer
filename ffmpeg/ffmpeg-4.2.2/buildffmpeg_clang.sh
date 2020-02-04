#!/bin/bash

ADDI_CFLAGS="-marm"
API=21
PLATFORM=arm-linux-androideabi
CPU=armv7-a

#自己本地的ndk路径。
NDK_ROOT=/Users/liangchuanfei/Documents/Android/SDK/android-ndk-r16b

#指定库文件的查找路径
SYSROOT=$NDK_ROOT/platforms/android-$API/arch-arm/

ISYSROOT=$NDK_ROOT/sysroot

ASM=$ISYSROOT/usr/include/$PLATFORM

TOOLCHAIN=$NDK_ROOT/toolchains/$PLATFORM-4.9/prebuilt/darwin-x86_64
#自己指定一个输出目录，用来放生成的文件的。
OUTPUT=`pwd`/android_out

function build
{
./configure \
--prefix=$OUTPUT \
--enable-shared \
--disable-static \
--disable-doc \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-avdevice \
--disable-doc \
--disable-symver \
--cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
--target-os=android \
--arch=arm \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-I$ASM -isysroot $ISYSROOT -Os -fpic -marm" \
--extra-ldflags="-marm" \
$ADDITIONAL_CONFIGURE_FLAG
  make clean
  make
  make install
}
build
