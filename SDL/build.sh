#!/bin/bash

SDLDIR=`pwd`
SDL1DIR=libsdl1.2-1.2.15
SDL2DIR=libsdl2-2.0.2+dfsg1
SDL1BUILD=sdl1.2_build
SDL2BUILD=sdl2_build

build()
{
	#sdl1.2
	mkdir -p ${SDL1BUILD}
	if [ ! -e ${SDL1DIR} ];then
		apt-get source libsdl1.2-dev
		rm libsdl1.2_* -rf
	fi
	cd ${SDL1DIR}
	patch -R -p0 < ../patch/SDL_x11sym.patch 1>/dev/null
	./configure --prefix=${SDLDIR}/${SDL1BUILD}
	make
	make install

	cd ${SDLDIR}

	#sdl2
	mkdir -p ${SDL2BUILD}
	if [ ! -e ${SDL2DIR} ];then
		apt-get source libsdl2-dev	
		rm libsdl2_2.0.2+dfsg1* -rf
	fi
	cd ${SDL2DIR}
	./configure --prefix=${SDLDIR}/${SDL2BUILD}
	make
	make install
	
	cd ${SDLDIR}
}

clear()
{
	rm ${SDL1DIR} ${SDL2DIR} ${SDL1BUILD} ${SDL2BUILD} -rf
}

if [ "$1" == "clear" ];then
	clear
else
	build
fi
