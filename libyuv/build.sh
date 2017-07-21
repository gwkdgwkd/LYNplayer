#!/bin/bash

LIBYUVDIR=`pwd`
LIBYUV=libyuv-master
LIBYUVTAR=master.zip
NDKDIR=${LIBYUVDIR}/../ffmpeg-android/android-ndk-r14b

clean()
{
	rm  ${LIBYUV} ${LIBYUVTAR} -rf
}

function build_libyuv_for_android
{
	if [ ! -e ${LIBYUVTAR} ];then
		wget https://github.com/lemenkov/libyuv/archive/master.zip
	fi
	if [ ! -e ${LIBYUV} ];then
		unzip master.zip
	fi

	cp Application.mk ${LIBYUV}/Application.mk
	cd ${LIBYUV}
	#diff -uN Android.mk libyuv-master/Android.mk > Android.mk.patch
	patch -t -p1 < ../Android.mk.patch
	#http://blog.csdn.net/lincyang/article/details/45971655
	${NDKDIR}/ndk-build NDK_PROJECT_PATH=. NDK_APPLICATION_MK=Application.mk
	cd ${LIBYUVDIR}
}

if [ "$1" == "clean" ];then
	clean
else
	build_libyuv_for_android
fi

