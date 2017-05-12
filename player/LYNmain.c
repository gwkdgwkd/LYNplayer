#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "LYNtype.h"

extern int play_vedio(cmdArgsPtr args);
extern int play_yuv420p(cmdArgsPtr args);
extern int video2rgbfiles(cmdArgsPtr args);
extern int video2yuv422pfiles(cmdArgsPtr args);
extern int video2yuv422pfile(cmdArgsPtr args);
extern int video2yuv420pfiles(cmdArgsPtr args);
extern int video2yuv420pfile(cmdArgsPtr args);
extern int yuv420p2video(cmdArgsPtr args);
extern int yuv420p2picture(cmdArgsPtr args);
extern int pcm2audio(cmdArgsPtr args);
extern int audio2pcm(cmdArgsPtr args);
extern int play_pcm(cmdArgsPtr args);
extern int demuxer(cmdArgsPtr args);
extern int muxer(cmdArgsPtr args);

action act[ACIDMAXID] = {
    {ACIDPLAY, "play", NOOUTPUT, NULL, play_vedio},
    {ACIDPLAYYUV420P, "playyuv420p", NOOUTPUT, NULL, play_yuv420p},
    {ACIDVIDEO2RGB24FILES, "video2rgb24files", HAVEOUTPUT, "rgb24",
     video2rgbfiles},
    {ACIDVIDEO2YUV422PFILES, "video2yuv422pfiles", HAVEOUTPUT, "yuv422p",
     video2yuv422pfiles},
    {ACIDVIDEO2YUV422PFILE, "video2yuv422pfile", HAVEOUTPUT, "yuv422p",
     video2yuv422pfile},
    {ACIDVIDEO2YUV420PFILES, "video2yuv420pfiles", HAVEOUTPUT, "yuv420p",
     video2yuv420pfiles},
    {ACIDVIDEO2YUV420PFILE, "video2yuv420pfile", HAVEOUTPUT, "yuv420p",
     video2yuv420pfile},
    {ACIDYUV420P2VIDEO, "yuv420p2video", HAVEOUTPUT, "video",
     yuv420p2video},
    {ACIDYUV420P2PICTURE, "yuv420p2picture", HAVEOUTPUT, "picture",
     yuv420p2picture},
    {ACIDPCM2AUDIO, "pcm2audio", HAVEOUTPUT, "audio", pcm2audio},
    {ACIDAUDIO2PCM, "audio2pcm", HAVEOUTPUT, "pcm", audio2pcm},
    {ACIDPLAYPCM, "playpcm", NOOUTPUT, NULL, play_pcm},
    {ACIDDEMUXER, "demuxer", HAVEOUTPUT, "demuxer", demuxer},
    {ACIDDEMUXER, "muxer", HAVEOUTPUT, "muxer.mp4", muxer},
};

static int find_action(const char *actname)
{
    int i;
    for (i = 0; i < ACIDMAXID; i++) {
        if (!strcmp(actname, act[i].name)) {
            return i;
        }
    }
    return -2;
}

static int guess_id(const char *infile, const char *outfile, int *actid)
{
    int i;
    char inext[10] = { 0 };
    char outext[10] = { 0 };
    int inlen = (infile == NULL ? 0 : strlen(infile));
    int outlen = (outfile == NULL ? 0 : strlen(outfile));

    if (inlen == 0) {
        return -1;
    }
    for (i = inlen - 1; i >= 0; i--) {
        if ('.' == infile[i]) {
            memcpy(inext, infile + i + 1, inlen - i - 1);
            break;
        }
    }

    if (outlen > 0) {
        for (i = outlen - 1; i >= 0; i--) {
            if ('.' == outfile[i]) {
                memcpy(outext, outfile + i + 1, outlen - i - 1);
                break;
            }
        }
    }

    if (strcmp(inext, "yuv") && strcmp(inext, "pcm") && 0 == outlen) {
        *actid = ACIDPLAY;
    } else if (!strcmp(inext, "yuv") && 0 == outlen) {
        *actid = ACIDPLAYYUV420P;
    } else if (!strcmp(inext, "pcm") && 0 == outlen) {
        *actid = ACIDPLAYPCM;
    } else if (!strcmp(outext, "yuv")) {
        *actid = ACIDVIDEO2YUV420PFILE;
    } else if (!strcmp(outext, "ppm")) {
        *actid = ACIDVIDEO2RGB24FILES;
    } else if ((!strcmp(outext, "h264") || !strcmp(outext, "h265")
                || !strcmp(outext, "hevc") || !strcmp(outext, "mpg"))
               && !strcmp(inext, "yuv")) {
        *actid = ACIDYUV420P2VIDEO;
    } else if (!strcmp(outext, "jpg") || !strcmp(outext, "png")
               || !strcmp(outext, "gif")) {
        *actid = ACIDYUV420P2PICTURE;
    } else if (!strcmp(inext, "pcm")
               && (!strcmp(outext, "aac") || !strcmp(outext, "mp3")
                   || !strcmp(outext, "mp2"))) {
        *actid = ACIDPCM2AUDIO;
    } else if ((!strcmp(inext, "mp2") || !strcmp(inext, "mp3")
                || !strcmp(inext, "aac") || !strcmp(inext, "ac3")
                || !strcmp(inext, "wma")) && !strcmp(outext, "pcm")) {
        *actid = ACIDAUDIO2PCM;
    } else if ((!strcmp(inext, "flv") || !strcmp(inext, "ts")
                || !strcmp(inext, "mp4") || !strcmp(inext, "mkv")
                || !strcmp(inext, "avi") || !strcmp(inext, "rmvb"))
               && outlen > 0) {
        *actid = ACIDDEMUXER;
    } else if ((!strcmp(outext, "mp4") || !strcmp(outext, "flv")
                || !strcmp(outext, "ts") || !strcmp(outext, "mkv")
                || !strcmp(outext, "avi")) && inlen > 0) {
        *actid = ACIDMUXER;
    } else {
        return -1;
    }

    printf("based on the input and output determine the id,%d\n", *actid);
    return 0;
}

static void set_default_arg(cmdArgsPtr args, int actid)
{
    if ((actid == ACIDVIDEO2RGB24FILES ||
         actid == ACIDVIDEO2YUV422PFILES ||
         actid == ACIDVIDEO2YUV422PFILE ||
         actid == ACIDVIDEO2YUV420PFILES || actid == ACIDVIDEO2YUV420PFILE
         || actid == ACIDYUV420P2VIDEO || actid == ACIDYUV420P2PICTURE)
        && args->framenum < 0) {
        args->framenum = DEFAULTFRAMENUM;
    }

    if ((actid == ACIDPLAYYUV420P || actid == ACIDYUV420P2VIDEO)
        && args->framerate < 0) {
        args->framerate = DEFAULTFRAMERATE;
    }
}

static void init_args(cmdArgsPtr args)
{
    args->infile = NULL;
    args->outfile = NULL;
    args->framenum = -1;
    args->width = -1;
    args->height = -1;
    args->framerate = -1;
}

static int check_args_format(int id, cmdArgsPtr args)
{
    if (id == ACIDPLAY
        && (args->framenum >= 0 || args->width >= 0 || args->height >= 0
            || args->framerate >= 0)) {
        return -1;
    }
    if ((id == ACIDPLAYYUV420P)
        && (args->width < 0 || args->height < 0 || args->framenum >= 0
            || args->framerate < 0)) {
        return -1;
    }
    if ((id == ACIDPCM2AUDIO)
        && (args->width >= 0 || args->height >= 0 || args->framenum >= 0
            || args->framerate >= 0)) {
        return -1;
    }
    if ((id == ACIDYUV420P2PICTURE)
        && (args->framerate >= 0)) {
        return -1;
    }
    if ((id == ACIDVIDEO2RGB24FILES ||
         id == ACIDVIDEO2YUV422PFILES ||
         id == ACIDVIDEO2YUV422PFILE ||
         id == ACIDVIDEO2YUV420PFILES ||
         id == ACIDVIDEO2YUV420PFILE || id == ACIDYUV420P2VIDEO
         || id == ACIDYUV420P2PICTURE)
        && (args->width < 0 || args->height < 0)) {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ch;
    cmdArgs args;
    int id = -1;
    int ret;

    init_args(&args);

    opterr = 0;
    while ((ch = getopt(argc, argv, "t:n:w:h:f:")) != -1) {
        switch (ch) {
        case 't':
            id = find_action(optarg);
            break;
        case 'n':
            args.framenum = atoi(optarg);
            break;
        case 'w':
            args.width = atoi(optarg);
            break;
        case 'h':
            args.height = atoi(optarg);
            break;
        case 'f':
            args.framerate = atoi(optarg);
            break;
        default:
            printf("other option :%c {%s}\n", ch, optarg);
        }
    }

    args.infile = argv[optind];
    args.outfile = argv[optind + 1];

    if (id == -2) {
        printf("no matching parameters of -t\n");
        return -2;
    } else if (id == -1) {
        if ((ret = guess_id(args.infile, args.outfile, &id)) < 0) {
            printf("can't guess id,please provide a movie file\n");
            return -2;
        }
    } else {
        if (args.outfile == NULL && act[id].ishaveoutput == HAVEOUTPUT) {
            args.outfile = act[id].defaultoutput;
        }
    }

    set_default_arg(&args, id);

    if ((ret = check_args_format(id, &args)) < 0) {
        printf("Invalid arguments format\n");
        return -3;
    }

    act[id].fun(&args);

    return 0;
}
