#m3u8-segmenter.c and libav-compat.h copy from git clone https://github.com/johnf/m3u8-segmenter.git

#use segmenter see http://blog.csdn.net/cjsafty/article/details/7922849#

#use ffmpeg see http://www.zhoubo15.cn/zone/shuishui/p/56.html
#ffmpeg -y -i test.mp4 -vcodec copy -acodec copy -vbsf h264_mp4toannexb test.ts
#ffmpeg -i test.ts -c copy -map 0 -f segment -segment_list test.m3u8 -segment_time 5 test%03d.ts

FFMPEG_BUILD=../ffmpeg/ffmpeg_build
CC=gcc
SRC = m3u8-segmenter.c
TARGET = segmenter
INCLUDES =-I$(FFMPEG_BUILD)/include
LIBS = -L$(FFMPEG_BUILD)/lib -lavformat -lavcodec -lavutil -lswresample -lx264 -lpthread  -ldl -lvpx -lvorbisenc -lvorbis -ltheoraenc -ltheoradec -lopus -lmp3lame -lfreetype -lfdk-aac -lz -lm -Lusr/lib/x86_64-linux-gnu/ -lva -lva-x11 -lva-drm -lX11 -lvdpau

CFLAGS:=$(CFLAGS) $(INCLUDES)

LDFLAGS:=$(LDFLAGS) $(LIBS)

OBJ := $(SRC:.c=.o)

all:$(OBJ)
	$(CC) -DDEBUG -Wall -g $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(TARGET)

%.o:%.c
	$(CC) -DDEBUG -Wall -g $(CFLAGS) -o $@ -c $<

.PHONY:clean
clean:
	-rm -f $(OBJ) $(TARGET)
