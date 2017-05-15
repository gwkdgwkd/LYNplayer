FFMPEG_BUILD=../ffmpeg/ffmpeg_build
SDL_BUILD=../SDL/sdl1.2_build
CC=gcc
SRC = LYNplayer.c LYNmain.c LYNvideo2yuv.c LYNplayyuv.c LYNyuv420p2video.c LYNyuv420p2picture.c LYNpcm2audio.c LYNaudio2pcm.c LYNplaypcm.c LYNdemuxer.c LYNmuxer.c LYNremuxer.c
TARGET = LYNplayer
INCLUDES =-I$(FFMPEG_BUILD)/include -I$(SDL_BUILD)/include
LIBS = -D_GNU_SOURCE=1 -D_REENTRANT -pthread -Wl,-rpath,$(SDL_BUILD)/lib -L$(FFMPEG_BUILD)/lib -L$(SDL_BUILD)/lib -lavformat -lavcodec -lxcb -lX11 -lx264 -lpthread -ldl -lvpx -lvorbisenc -lvorbis -ltheoraenc -ltheoradec -logg -lopus -lmp3lame -lfreetype -lfdk-aac -lass -lz -lswresample -lswscale -lavutil -lrt -lm -lSDL -Lusr/lib/x86_64-linux-gnu/ -lva -lva-x11 -lva-drm -lX11 -lvdpau

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
