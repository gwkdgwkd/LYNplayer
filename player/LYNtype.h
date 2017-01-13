#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef enum actionId {
    ACIDPLAY,
    ACIDVEDIO2YUV,
    ACIDVEDIO2YUVS,
    ACIDMAXID
} actionId;

typedef struct cmdArgs {
    char *infile;
    char *outfile;
} cmdArgs, *cmdArgsPtr;

typedef int (*doAction) (cmdArgsPtr args);

typedef struct action {
    actionId id;
    char *name;
    doAction fun;
} action, *actionPtr;
