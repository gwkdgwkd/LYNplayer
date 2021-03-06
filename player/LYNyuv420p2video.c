#include "LYNtype.h"

#define USELIBAVFORMAT 0

#if USELIBAVFORMAT

#else
int video_encode(cmdArgsPtr args, int codec_id, const char *outfile)
{
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx = NULL;
    int i, ret, got_output;
    FILE *fp_in;
    FILE *fp_out;
    AVFrame *pFrame;
    AVPacket pkt;
    int y_size;
    int framecnt = 0;
    long framenum;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    avcodec_register_all();
    pCodec = avcodec_find_encoder(codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        printf("Could not allocate video codec context\n");
        return -1;
    }
    pCodecCtx->bit_rate = args->framerate * 10000;
    pCodecCtx->width = args->width;
    pCodecCtx->height = args->height;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;
    pCodecCtx->gop_size = 10;
    pCodecCtx->max_b_frames = 1;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec_id == AV_CODEC_ID_H264) {
        av_opt_set(pCodecCtx->priv_data, "preset", "slow", 0);
    }

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    if (!pFrame) {
        printf("Could not allocate video frame\n");
        return -1;
    }
    pFrame->format = pCodecCtx->pix_fmt;
    pFrame->width = pCodecCtx->width;
    pFrame->height = pCodecCtx->height;

    ret =
        av_image_alloc(pFrame->data, pFrame->linesize, pCodecCtx->width,
                       pCodecCtx->height, pCodecCtx->pix_fmt, 16);
    if (ret < 0) {
        printf("Could not allocate raw picture buffer\n");
        return -1;
    }
    //Input raw data
    fp_in = fopen(args->infile, "rb");
    if (!fp_in) {
        printf("Could not open %s\n", args->infile);
        return -1;
    }

    if (args->framenum == 0) {
        fseek(fp_in, 0L, SEEK_END);
        framenum = ftell(fp_in) / ((args->width * args->height) * 3 / 2);
        fseek(fp_in, 0L, SEEK_SET);
    }
    //Output bitstream
    fp_out = fopen(outfile, "wb");
    if (!fp_out) {
        printf("Could not open %s\n", outfile);
        return -1;
    }

    y_size = pCodecCtx->width * pCodecCtx->height;
    //Encode
    for (i = 0; i < framenum; i++) {
        av_init_packet(&pkt);
        pkt.data = NULL;        // packet data will be allocated by the encoder
        pkt.size = 0;
        //Read raw YUV data
        if (fread(pFrame->data[0], 1, y_size, fp_in) <= 0 || // Y
            fread(pFrame->data[1], 1, y_size / 4, fp_in) <= 0 || // U
            fread(pFrame->data[2], 1, y_size / 4, fp_in) <= 0) { // V
            return -1;
        } else if (feof(fp_in)) {
            break;
        }

        pFrame->pts = i;
        /* encode the image */
        ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_output);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }
        if (got_output) {
            printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt,
                   pkt.size);
            framecnt++;
            fwrite(pkt.data, 1, pkt.size, fp_out);
            av_free_packet(&pkt);
        }
    }
    //Flush Encoder
    for (got_output = 1; got_output; i++) {
        ret = avcodec_encode_video2(pCodecCtx, &pkt, NULL, &got_output);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }
        if (got_output) {
            printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",
                   pkt.size);
            fwrite(pkt.data, 1, pkt.size, fp_out);
            av_free_packet(&pkt);
        }
    }

    fwrite(endcode, 1, sizeof(endcode), fp_out);

    fclose(fp_in);
    fclose(fp_out);
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);
    av_freep(&pFrame->data[0]);
    av_frame_free(&pFrame);

    return 0;
}
#endif

int yuv420p2video(cmdArgsPtr args)
{
    char outfilename[128] = { 0 };
    char ext[10] = { 0 };
    int i, len = strlen(args->outfile);
    enum AVCodecID codec_id;

    for (i = len - 1; i >= 0; i--) {
        if ('.' == args->outfile[i]) {
            memcpy(ext, args->outfile + i + 1, len - i - 1);
            if (!strcmp(ext, "h264") || !strcmp(ext, "hevc")
                || !strcmp(ext, "mpg") || !strcmp(ext, "h265")) {
                strcpy(outfilename, args->outfile);
            } else {
                sprintf(outfilename, "%s.h264", args->outfile);
                strcpy(ext, "h264");
            }
            break;
        }
    }
    if (i < 0) {
        sprintf(outfilename, "%s.h264", args->outfile);
        strcpy(ext, "h264");
    }

    if (!strcmp(ext, "h265") || !strcmp(ext, "hevc")) {
        codec_id = AV_CODEC_ID_HEVC;
    } else if (!strcmp(ext, "h264")) {
        codec_id = AV_CODEC_ID_H264;
    } else if (!strcmp(ext, "mpg")) {
        codec_id = AV_CODEC_ID_MPEG1VIDEO;
    }

    video_encode(args, codec_id, outfilename);
}
