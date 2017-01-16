#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef enum actionId {
    ACIDPLAY,
    ACIDVEDIO2RGB,
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

typedef struct inFile {
    AVFormatContext *pFormatCtx;
    int videoStream;
    AVCodecContext *pCodecCtxOrig;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVFrame *pFrameTarget;
    AVPacket packet;
    int frameFinished;
    int numBytes;
    uint8_t *buffer;
    struct SwsContext *sws_ctx;
    enum AVPixelFormat format;
} inFile, *inFilePtr;
