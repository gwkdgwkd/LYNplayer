#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define HAVEOUTPUT 1
#define NOOUTPUT 0

#define DEFAULTFRAMENUM 0       //means all frame
#define DEFAULTFRAMERATE 40

typedef enum actionId {
    ACIDPLAY,
    ACIDPLAYYUV420P,
    ACIDVIDEO2RGB24FILES,
    ACIDVIDEO2RGB24FILE,
    ACIDVIDEO2YUV422PFILES,
    ACIDVIDEO2YUV422PFILE,
    ACIDVIDEO2YUV420PFILES,
    ACIDVIDEO2YUV420PFILE,
    ACIDYUV420P2VIDEO,
    ACIDYUV420P2PICTURE,
    ACIDPCM2AUDIO,
    ACIDAUDIO2PCM,
    ACIDPLAYPCM,
    ACIDDEMUXER,
    ACIDMUXER,
    ACIDREMUXER,
    ACIDPUSH,
    ACIDRECEIVE,
    ACIDGRABDESKTOP,
    ACIDMAXID
} actionId;

typedef struct cmdArgs {
    char *infile;
    char *outfile;
    int framenum;
    int width;
    int height;
    int framerate;
} cmdArgs, *cmdArgsPtr;

typedef int (*doAction) (cmdArgsPtr args);

typedef struct action {
    actionId id;
    char *name;
    int ishaveoutput;
    char *defaultoutput;
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
