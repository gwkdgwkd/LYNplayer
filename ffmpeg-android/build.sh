#!/bin/bash

#http://www.roman10.net/2013/08/18/how-to-build-ffmpeg-with-ndk-r9/
#https://github.com/dxjia/ffmpeg-compile-shared-library-for-android
#http://blog.csdn.net/xiaoru5127/article/details/51524795

FFMPEGDIR=`pwd`
FFMPEG=ffmpeg
FFMPEGTAR=ffmpeg-snapshot.tar.bz2
TMP=tmpdir
export TMPDIR=${FFMPEGDIR}/tmpdir
NDK=${FFMPEGDIR}/android-ndk-r14b

if [ ! -e ${NDK} ];then
	echo "wrong NDK dir ("${NDK}")"
	echo "download:"
        echo "wget https://dl.google.com/android/repository/android-ndk-r14b-linux-x86_64.zip"
        echo "unzip android-ndk-r14b-linux-x86_64.zip"
	return
fi


if [ "$1" == "64" ];then
	ARCH=aarch64
        DIR=android
        EXT=arm64
else
	ARCH=arm
	DIR=androideabi
	EXT=arm
fi

BUILD=${FFMPEGDIR}/ffmpeg_build_${EXT}
SYSROOT=${NDK}/platforms/android-24/arch-${EXT}/
TOOLCHAIN=${NDK}/toolchains/${ARCH}-linux-${DIR}-4.9/prebuilt/linux-x86_64
CROSSPREFIX=${TOOLCHAIN}/bin/${ARCH}-linux-${DIR}-

clean()
{
	rm  ${FFMPEG} ${FFMPEGTAR} ffmpeg_build* ${TMP} -rf
}

function build_ffmpeg_for_android
{
	mkdir -p ${BUILD}        

	if [ ! -e ${FFMPEGTAR} ];then
		wget http://ffmpeg.org/releases/ffmpeg-snapshot.tar.bz2
	fi
	if [ ! -e ${FFMPEG} ];then
		tar xjvf ${FFMPEGTAR}
        	cp configure.android ${FFMPEG}/configure
	fi
        if [ ! -e ${TMP} ];then
		mkdir -p ${TMP}
        fi
	cd ${FFMPEG}
	./configure \
	--prefix=$BUILD \
	--enable-shared \
	--disable-static \
	--disable-doc \
	--disable-ffmpeg \
	--disable-ffplay \
	--disable-ffprobe \
	--disable-ffserver \
	--disable-doc \
	--disable-symver \
	--enable-small \
	--cross-prefix=${CROSSPREFIX} \
	--target-os=linux \
	--arch=$ARCH \
	--enable-cross-compile \
	--sysroot=$SYSROOT \
	--extra-cflags="-Os -fpic -DANDROID" \
	--extra-ldflags="$ADDI_LDFLAGS" \
	$ADDITIONAL_CONFIGURE_FLAG
	make clean
	make
	make install

	cd ${FFMPEGDIR}
}

if [ "$1" == "clean" ];then
	clean
else
	build_ffmpeg_for_android
fi

