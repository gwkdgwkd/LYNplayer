#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "libyuv.h"

#include "type.h"
#include "audiodevice.h"
#include "timer.h"

#define VIDEO_PICTURE_QUEUE_SIZE 1
#define MAX_AUDIO_FRAME_SIZE 192000
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0
//#define USE_SWS_CTX 1
//#define USE_AUDIO_TRACK 1

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} PacketQueue;

typedef struct VideoPicture {
    AVFrame *bmp;
    int width, height; /* source height & width */
    int allocated;
    int id;
    double pts;
} VideoPicture;

typedef struct VideoState {
    AVFormatContext *pFormatCtx;
    int             videoStream, audioStream;

    AVStream        *audio_st;
    AVCodecContext  	*aCodecCtx;
    PacketQueue     audioq;
    uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    unsigned int    audio_buf_size;
    unsigned int    audio_buf_index;
    AVFrame         audio_frame;
    AVPacket        audio_pkt;
    uint8_t         *audio_pkt_data;
    int             audio_pkt_size;
    double         audio_clock;

    AVStream        *video_st;
    AVCodecContext  	*vCodecCtx;
    PacketQueue     videoq;
    AVFrame         *scaleFrame;
    AVFrame         *frameRGBA;
    void*			buffer;
    void*			scalebuffer;
    VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int             pictq_size, pictq_rindex, pictq_windex;
    double          frame_timer;
    double          frame_last_pts;
    double          frame_last_delay;
    double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame

    pthread_mutex_t       mutex;
    pthread_cond_t        cond;
    pthread_mutex_t       pictq_mutex;
    pthread_cond_t        pictq_cond;
    pthread_mutex_t       display_mutex;
    pthread_mutex_t       decode_mutex;

    pthread_t      read_packet_tid;
    pthread_t      video_decode_tid;
    pthread_t      video_display_tid;
    char            filename[1024];
    int             quit;
    int             is_play;
    int             init;
    float           ratio;
    int 			width;
    int 			height;

    AVIOContext     *io_context;
    struct SwsContext *sws_ctx;
} VideoState;

VideoState *global_video_state;

int getPcm(void **pcm, size_t *pcmSize);
