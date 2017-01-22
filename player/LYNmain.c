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

action act[ACIDMAXID] = {
    {ACIDPLAY, "play", NOOUTPUT, NULL, play_vedio},
    {ACIDPLAYYUV420P, "playyuv420p", NOOUTPUT, NULL, play_yuv420p},
    {ACIDVIDEO2RGB24FILES, "video2rgb24files", HAVEOUTPUT, "frame",
     video2rgbfiles},
    {ACIDVIDEO2YUV422PFILES, "video2yuv422pfiles", HAVEOUTPUT, "frame",
     video2yuv422pfiles},
    {ACIDVIDEO2YUV422PFILE, "video2yuv422pfile", HAVEOUTPUT, "frame",
     video2yuv422pfile},
    {ACIDVIDEO2YUV420PFILES, "video2yuv420pfiles", HAVEOUTPUT, "frame",
     video2yuv420pfiles},
    {ACIDVIDEO2YUV420PFILE, "video2yuv420pfile", HAVEOUTPUT, "frame",
     video2yuv420pfile},
    {ACIDYUV420P2VIDEO, "yuv420p2video", HAVEOUTPUT, "video",
     yuv420p2video},
};

static int find_action(const char *actname)
{
    int i;
    for (i = 0; i < ACIDMAXID; i++) {
        if (!strcmp(actname, act[i].name)) {
            return i;
        }
    }
    return -1;
}

static void adjust_id(const char *filename, int *actid)
{
    int i;
    char ext[10] = { 0 };
    int len = strlen(filename);

    for (i = len - 1; i >= 0; i--) {
        if ('.' == filename[i]) {
            memcpy(ext, filename + i + 1, len - i - 1);
            break;
        }
    }
    if (!strcmp(ext, "yuv")) {
        *actid = ACIDPLAYYUV420P;
    }
}

static void set_default_arg(cmdArgsPtr args, int actid)
{
    if ((actid == ACIDVIDEO2RGB24FILES ||
         actid == ACIDVIDEO2YUV422PFILES ||
         actid == ACIDVIDEO2YUV422PFILE ||
         actid == ACIDVIDEO2YUV420PFILES || actid == ACIDVIDEO2YUV420PFILE
         || actid == ACIDYUV420P2VIDEO)
        && args->framenum < 0) {
        args->framenum = DEFAULTFRAMENUM;
    }

    if (actid == ACIDPLAYYUV420P && args->width < 0) {
        args->width = DEFAULTWIDTH;
    }
    if (actid == ACIDPLAYYUV420P && args->height < 0) {
        args->height = DEFAULTHEIGHT;
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
    if ((id == ACIDPLAY && args->framenum >= 0) ||
        (id == ACIDYUV420P2VIDEO && args->width < 0) ||
        (id == ACIDYUV420P2VIDEO && args->height < 0)) {
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int ch;
    cmdArgs args;
    int id = ACIDPLAY;
    int needadjust = 1;
    int ret;

    if (argc < 2) {
        printf("Please provide a movie file\n");
        return (-1);
    }

    init_args(&args);

    opterr = 0;
    while ((ch = getopt(argc, argv, "t:n:w:h:f:")) != -1) {
        switch (ch) {
        case 't':
            id = find_action(optarg);
            needadjust = 0;
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

    if (id < 0) {
        printf("no matching parameters of -t\n");
        return -2;
    }

    if (argv[optind] != NULL) {
        args.infile = argv[optind];
        if ((ACIDPLAY == id) && needadjust) {
            adjust_id(args.infile, &id);
        }

        if (argv[optind + 1] != NULL && act[id].ishaveoutput == HAVEOUTPUT) {
            args.outfile = argv[optind + 1];
        } else if (argv[optind + 1] == NULL
                   && act[id].ishaveoutput == HAVEOUTPUT) {
            args.outfile = act[id].defaultoutput;
        } else if (argv[optind + 1] != NULL
                   && act[id].ishaveoutput == NOOUTPUT) {
            printf("Invalid arguments format\n");
            return -4;
        }

        if ((ret = check_args_format(id, &args)) < 0) {
            printf("Invalid arguments format\n");
            return -5;
        }

        set_default_arg(&args, id);

        act[id].fun(&args);
    } else {
        printf("no input file!\n");
        return -3;
    }

    return 0;
}
