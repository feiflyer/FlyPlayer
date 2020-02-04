#!/bin/bash
NDK=/Users/liangchuanfei/Documents/Android/SDK/android-ndk-r20b
API=16
# arm aarch64 i686 x86_64
ARCH=arm
# armv7a aarch64 i686 x86_64
PLATFORM=armv7a
TARGET=$PLATFORM-linux-androideabi
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin
SYSROOT=$NDK/sysroot
PREFIX=./Android_out/$PLATFORM
 
CFLAG="-D__ANDROID_API__=$API -U_FILE_OFFSET_BITS -DBIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD -Os -fPIC -DANDROID -D__thumb__ -mthumb -Wfatal-errors -Wno-deprecated -mfloat-abi=softfp -marm"
 
build_one()
{
./configure \
--ln_s="cp -rf" \
--prefix=$PREFIX \
--cc=$TOOLCHAIN/$TARGET$API-clang \
--cxx=$TOOLCHAIN/$TARGET$API-clang++ \
--ld=$TOOLCHAIN/$TARGET$API-clang \
--target-os=android \
--arch=$ARCH \
--cpu=$PLATFORM \
--cross-prefix=$TOOLCHAIN/$ARCH-linux-androideabi- \
--enable-cross-compile \
--enable-shared \
--disable-static \
--enable-runtime-cpudetect \
--disable-doc \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-doc \
--disable-symver \
--enable-small \
--enable-gpl --enable-nonfree --enable-version3 --disable-iconv --enable-neon --enable-hwaccels \
--enable-jni \
--enable-mediacodec \
--enable-avdevice  \
--disable-decoders --enable-decoder=vp9 --enable-decoder=h264 --enable-decoder=mpeg4 --enable-decoder=aac --enable-decoder=h264_mediacodec \
--disable-postproc \
--extra-cflags="$CFLAG" \
--extra-ldflags="-marm"
}
 
build_one
 
make clean
 
make -j4
 
make install