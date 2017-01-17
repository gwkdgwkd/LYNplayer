#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "LYNtype.h"

extern int play_vedio(cmdArgsPtr args);
extern int video2rgbfiles(cmdArgsPtr args);
extern int vedio2yuv422pfiles(cmdArgsPtr args);
extern int vedio2yuv422pfile(cmdArgsPtr args);
extern int vedio2yuv420pfiles(cmdArgsPtr args);
extern int vedio2yuv420pfile(cmdArgsPtr args);

action act[ACIDMAXID] = {
    {ACIDPLAY, "play", NOOUTPUT, play_vedio},
    {ACIDVEDIO2RGB24FILES, "vedio2rgb24files", HAVEOUTPUT, video2rgbfiles},
    {ACIDVEDIO2YUV422PFILES, "vedio2yuv422pfiles", HAVEOUTPUT,
     vedio2yuv422pfiles},
    {ACIDVEDIO2YUV422PFILE, "vedio2yuv422pfile", HAVEOUTPUT,
     vedio2yuv422pfile},
    {ACIDVEDIO2YUV420PFILES, "vedio2yuv420pfiles", HAVEOUTPUT,
     vedio2yuv420pfiles},
    {ACIDVEDIO2YUV420PFILE, "vedio2yuv420pfile", HAVEOUTPUT,
     vedio2yuv420pfile},
};

int find_action(char *actname)
{
    int i;
    for (i = 0; i < ACIDMAXID; i++) {
        if (!strcmp(actname, act[i].name)) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[])
{
    int ch;
    cmdArgs args;
    int id = ACIDPLAY;

    if (argc < 2) {
        printf("Please provide a movie file\n");
        return (-1);
    }

    args.framenum = -1;
    opterr = 0;
    while ((ch = getopt(argc, argv, "t:n:")) != -1) {
        switch (ch) {
        case 't':
            id = find_action(optarg);
            break;
        case 'n':
            args.framenum = atoi(optarg);
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
        if (argv[optind + 1] != NULL && act[id].ishaveoutput == HAVEOUTPUT) {
            args.outfile = argv[optind + 1];
        } else if (argv[optind + 1] == NULL
                   && act[id].ishaveoutput == HAVEOUTPUT) {
            args.outfile = DEFAULTOUTPUTFILE;
        } else if (argv[optind + 1] != NULL
                   && act[id].ishaveoutput == NOOUTPUT) {
            printf("Invalid arguments format\n");
            return -4;
        }

        if (id == ACIDPLAY && args.framenum >= 0) {
            printf("Invalid arguments format\n");
            return -5;
        }

        if ((id == ACIDVEDIO2RGB24FILES ||
             id == ACIDVEDIO2YUV422PFILES ||
             id == ACIDVEDIO2YUV422PFILE ||
             id == ACIDVEDIO2YUV420PFILES || id == ACIDVEDIO2YUV420PFILE)
            && args.framenum < 0) {
            args.framenum = DEFAULTFRAMENUM;
        }
        act[id].fun(&args);
    } else {
        printf("no input file for play\n");
        return -3;
    }

    return 0;
}
