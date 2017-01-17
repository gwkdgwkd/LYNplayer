#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define HAVEOUTPUT 1
#define NOOUTPUT 0

#define DEFAULTOUTPUTFILE "frame"
#define DEFAULTFRAMENUM 5

typedef enum actionId {
    ACIDPLAY,
    ACIDVEDIO2RGB24FILES,
    ACIDVEDIO2YUV422PFILES,
    ACIDVEDIO2YUV422PFILE,
    ACIDVEDIO2YUV420PFILES,
    ACIDVEDIO2YUV420PFILE,
    ACIDMAXID
} actionId;

typedef struct cmdArgs {
    char *infile;
    char *outfile;
    int framenum;
} cmdArgs, *cmdArgsPtr;

typedef int (*doAction) (cmdArgsPtr args);

typedef struct action {
    actionId id;
    char *name;
    int ishaveoutput;
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
