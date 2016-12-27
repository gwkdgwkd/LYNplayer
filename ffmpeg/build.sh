#!/bin/bash

#https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu#RevertingChangesMadebyThisGuideCompile%20FFmpeg%20on%20Ubuntu,%20Debian,%20or%20Mint
#http://blog.csdn.net/leechee_1986/article/details/41891119

FFMPEGDIR=`pwd`

#sudo apt-get update
#sudo apt-get -y install autoconf automake build-essential libass-dev libfreetype6-dev libgpac-dev \
#  libsdl1.2-dev libtheora-dev libtool libva-dev libvdpau-dev libvorbis-dev libx11-dev \
#  libxext-dev libxfixes-dev pkg-config texi2html zlib1g-dev
#mkdir ~/ffmpeg_sources

mkdir -p ffmpeg_build
BUILD=${FFMPEGDIR}/ffmpeg_build

# Yasm
YASM=yasm-1.3.0
YASMTAR=${YASM}.tar.gz
if [ ! -e ${YASMTAR} ];then
	wget http://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz
fi
if [ ! -e ${YASM} ];then
	tar xzvf yasm-1.3.0.tar.gz
fi
cd ${YASM}
./configure --prefix="$BUILD" --bindir="$BUILD/bin"
make
make install
#make disclean

cd ${FFMPEGDIR}

# libx264
LIBX264=x264-snapshot*
LIBX264TAR=last_x264.tar.bz2
if [ ! -e ${LIBX264TAR} ];then
	wget http://download.videolan.org/pub/x264/snapshots/last_x264.tar.bz2
fi
if [ ! -e ${LIBX264} ];then
	tar xjvf last_x264.tar.bz2
fi
cd ${LIBX264}
PATH="$BUILD/bin:$PATH" ./configure --prefix="$BUILD" --bindir="$BUILD/bin" --enable-static
PATH="$BUILD/bin:$PATH" make
make install
#make disclean

cd ${FFMPEGDIR}

# libfdk-aac
LIBFDKAAC=mstorsjo-fdk-aac*
LIBFDKAACTAR=fdk-aac.zip
if [ ! -e ${LIBFDKAACTAR} ];then
	wget -O fdk-aac.zip https://github.com/mstorsjo/fdk-aac/zipball/master
fi
if [ ! -e ${LIBFDKAAC} ];then
	unzip fdk-aac.zip
fi
cd ${LIBFDKAAC}
autoreconf -fiv
./configure --prefix="$BUILD" --disable-shared
make
make install
#make disclean

cd ${FFMPEGDIR}

# libmp3lame
LIBMP3LAME=lame-3.99.5
LIBMP3LAMETAR=lame-3.99.5.tar.gz
if [ ! -e ${LIBMP3LAMETAR} ];then
	wget http://downloads.sourceforge.net/project/lame/lame/3.99/lame-3.99.5.tar.gz --no-check-certificate
fi
if [ ! -e ${LIBMP3LAME} ];then
	tar xzvf lame-3.99.5.tar.gz
fi
cd ${LIBMP3LAME}
./configure --prefix="$BUILD" --enable-nasm --disable-shared
make
make install
#make disclean

cd ${FFMPEGDIR}

# libopus
LIBOPUS=opus-1.1
LIBOPUSTAR=opus-1.1.tar.gz
if [ ! -e ${LIBOPUSTAR} ];then
	wget http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz
fi
if [ ! -e ${LIBOPUS} ];then
	tar xzvf opus-1.1.tar.gz
fi
cd ${LIBOPUS}
./configure --prefix="$BUILD" --disable-shared
make
make install
#make disclean

cd ${FFMPEGDIR}

# libvpx
LIBVPX=libvpx-1.6.0
LIBVPXTAR=libvpx-1.6.0.tar.bz2
if [ ! -e ${LIBVPXTAR} ];then
	wget http://storage.googleapis.com/downloads.webmproject.org/releases/webm/libvpx-1.6.0.tar.bz2
fi
if [ ! -e ${LIBVPX} ];then
	tar xjvf libvpx-1.6.0.tar.bz2
fi
cd ${LIBVPX}
./configure --prefix="$BUILD" --disable-shared
PATH="$BUILD/bin:$PATH" ./configure --prefix="$BUILD" --disable-examples --disable-unit-tests
PATH="$BUILD/bin:$PATH" make
make install
#make clean

cd ${FFMPEGDIR}

# ffmpeg
FFMPEG=ffmpeg
FFMPEGTAR=1ffmpeg-snapshot.tar.bz2
if [ ! -e ${FFMPEGTAR} ];then
	wget http://ffmpeg.org/releases/ffmpeg-snapshot.tar.bz2
fi
if [ ! -e ${FFMPEG} ];then
	tar xjvf ffmpeg-snapshot.tar.bz2
fi
cd ${FFMPEG}
PATH="$BUILD/bin:$PATH" PKG_CONFIG_PATH="$BUILD/lib/pkgconfig" ./configure \
  --prefix="$BUILD" \
  --extra-cflags="-I$BUILD/include" \
  --extra-ldflags="-L$BUILD/lib" \
  --bindir="$BUILD/bin" \
  --enable-gpl \
  --enable-libass \
  --enable-libfdk-aac \
  --enable-libfreetype \
  --enable-libmp3lame \
  --enable-libopus \
  --enable-libtheora \
  --enable-libvorbis \
  --enable-libvpx \
  --enable-libx264 \
  --enable-nonfree \
  --enable-x11grab
PATH="$BUILD/bin:$PATH" make
make install
#make distclean

cd ${FFMPEGDIR}

