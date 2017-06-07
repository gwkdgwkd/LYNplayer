#install libxcb-shm
#sudo apt-get install libx11-xcb-dev libxcb-shm0-dev libxcb-xv0

FFMPEG_BUILD=../ffmpeg/ffmpeg_build
SDL2_BUILD=../SDL/sdl2_build
SDL1.2_BUILD=../SDL/sdl1.2_build
CC=gcc
SRC = LYNplayer.c LYNmain.c LYNvideo2yuv.c LYNplayyuv.c LYNyuv420p2video.c LYNyuv420p2picture.c LYNpcm2audio.c LYNaudio2pcm.c LYNplaypcm.c LYNdemuxer.c LYNmuxer.c LYNremuxer.c LYNpush.c LYNreceive.c LYNgrabdesktop.c
TARGET = LYNplayer
INCLUDES =-I$(FFMPEG_BUILD)/include -I$(SDL2_BUILD)/include -I$(SDL1.2_BUILD)/include
LIBS = -D_GNU_SOURCE=1 -D_REENTRANT -pthread -Wl,-rpath,$(SDL2_BUILD)/lib -L$(FFMPEG_BUILD)/lib -lavformat -lavdevice -lavfilter -lavcodec -lavutil -lswresample -lswscale -lpostproc -L$(SDL2_BUILD)/lib -lSDL2 -L$(SDL1.2_BUILD)/lib -lSDL -lxcb -lX11 -lx264 -lpthread -ldl -lvpx -lvorbisenc -lvorbis -ltheoraenc -ltheoradec -logg -lopus -lmp3lame -lfreetype -lfdk-aac -lass -lz -lrt -lm  -Lusr/lib/x86_64-linux-gnu/ -lva -lva-x11 -lva-drm -lX11 -lvdpau -lxcb -lxcb-shm -lxcb-shape -lxcb-xfixes -lasound

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
