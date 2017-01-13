#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "LYNtype.h"

extern int play_vedio(cmdArgsPtr args);
extern int video2yuv(cmdArgsPtr args);

action act[ACIDMAXID] = {
    {ACIDPLAY, "play", play_vedio},
    {ACIDVEDIO2YUV, "vedio2yuv", video2yuv},
    {ACIDVEDIO2YUVS, "vedio2yuvs", NULL},
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

    opterr = 0;
    while ((ch = getopt(argc, argv, "t:")) != -1) {
        switch (ch) {
        case 't':
            id = find_action(optarg);
            break;
        default:
            printf("other option :%c {%s}\n", ch, optarg);
        }
    }

    if (id < 0) {
        printf("no matching parameters of -t\n");
        return -1;
    }

    if (argv[optind] != NULL) {
        args.infile = argv[optind];
        act[id].fun(&args);
    } else {
        printf("no input file for play\n");
        return -2;
    }

    return 0;
}
