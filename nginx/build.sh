#!/bin/bash

#http://lib.csdn.net/article/57/37915?knId=1549
#http://www.cnblogs.com/cocoajin/p/4353767.html
#build nginx

#install SSH
#apt-get install openssl
#apt-get install libssl-dev

NGINXDIR=`pwd`
BUILD=${NGINXDIR}/nginx_build

NGINX=nginx-1.7.5
NGINXTAR=${NGINX}.tar.gz
RTMP=nginx-rtmp-module-master
RTMPZIP=master.zip

clear()
{
	rm ${NGINX} ${NGINXTAR} ${RTMP} ${RTMPZIP} ${BUILD} -rf
}

build()
{
	#sudo apt-get update
	#sudo apt-get install libpcre3 libpcre3-dev
	#sudo apt-get install openssl libssl-dev
	mkdir -p nginx_build

	# nginx
	if [ ! -e ${NGINXTAR} ];then
		wget http://nginx.org/download/nginx-1.7.5.tar.gz
	fi
	if [ ! -e ${NGINX} ];then
		tar -zxvf ${NGINXTAR}
	fi

  cd ${NGINXDIR}

	# nginx rtmp module
	if [ ! -e ${RTMPZIP} ];then
		wget https://github.com/arut/nginx-rtmp-module/archive/master.zip
	fi
	if [ ! -e ${RTMP} ];then
		unzip ${RTMPZIP}
	fi

	cd ${NGINXDIR}

	# ffmpeg
	build_nginx

	cd ${NGINXDIR}
}

build_nginx()
{
	cd ${NGINX}
	./configure --with-http_ssl_module --add-module=../nginx-rtmp-module-master
	make
	make install DESTDIR=${BUILD}

	cd ${NGINXDIR}
	cp nginx.conf nginx_build/usr/local/nginx/conf/nginx.conf

	#run nginx
	#sudo nginx_build/usr/local/nginx/sbin/nginx -p nginx_build/usr/local/nginx
}

if [ "$1" == "clear" ];then
	clear
elif [ "$1" == "nginx" ];then
	build_nginx
else
	build
fi

