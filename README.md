# our(lwl and mlk) player for lyn
 - use ffmpeg + sdl

# add some dir

 - player, player source 
 - SDL, lib sdl
 - ffmpeg, lib ffmpeg

# build ffmpeg
 - cd ffmpeg;source build.sh

# clear ffmpeg
 - cd ffmpeg;./build.sh clear

# build and clear SDL
 - build: cd SDL;source build.sh
 - clear: cd SDL;./build.sh clear

# add build ffmpeg only
 - cd ffmpeg;source build.sh ffmpeg

# add Makefile to build player
 - cd player;make

# code format
 - use cmd "indent -kr -i4 -ts0 LYNplayer.c"

# video to RGB24
 - ./LYNplayer file -t vedio2rgb

# add video to yuv
 - ./LYNplayer infile outfile -n15 -t video2yuv422pfile
 - arg -t :
 - play (if no -t, play is default)
 - video2rgb24files
 - video2yuv422pfiles
 - video2yuv422pfile
 - video2yuv420pfiles
 - video2yuv420pfile
 - arg -n (frame num):
 - 0 (all frame)
 - num 
 - no arg -n (default 5)
 - if no outfile,default outfile name is frame

# add video
 - ./LYNplayer *.yuv -w640 -h360
 - default 176 144 if no w and h
