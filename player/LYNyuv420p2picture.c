#include "LYNtype.h"

int do_encode(cmdArgsPtr args, enum AVPixelFormat targetformat)
{
    AVFormatContext *pFormatCtx;
    AVOutputFormat *fmt;
    AVStream *video_st;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    struct SwsContext *sws_ctx;

    uint8_t *picture_buf;
    AVFrame *picture, *pictureTarget;
    AVPacket pkt;
    int y_size;
    int got_picture = 0;
    int size;
    int framenum, framecnt = 0;
    int i, ret = 0;

    FILE *in_file = NULL;       //YUV source
    in_file = fopen(args->infile, "rb");
    if (!in_file) {
        printf("Could not open %s\n", args->infile);
        return -1;
    }

    if (args->framenum == 0 && targetformat == AV_PIX_FMT_RGB8) {
        fseek(in_file, 0L, SEEK_END);
        framenum = ftell(in_file) / ((args->width * args->height) * 3 / 2);
        fseek(in_file, 0L, SEEK_SET);
    } else if (args->framenum == 0 || targetformat != AV_PIX_FMT_RGB8) {
        printf("png and jpg just encode first yuv picture!\n");
        framenum = 1;
    } else {
        framenum = args->framenum;
    }

    av_register_all();

    //Method 1
    //pFormatCtx = avformat_alloc_context();
    //Guess format
    //fmt = av_guess_format("mjpeg", NULL, NULL);
    //pFormatCtx->oformat = fmt;
    //Output URL
    //if (avio_open(&pFormatCtx->pb, args->outfile, AVIO_FLAG_READ_WRITE) < 0) {
    //    printf("Couldn't open output file.");
    //    return -1;
    //}
    //Method 2. More simple
    avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, args->outfile);
    fmt = pFormatCtx->oformat;

    video_st = avformat_new_stream(pFormatCtx, 0);
    if (video_st == NULL) {
        return -1;
    }
    pCodecCtx = video_st->codec;
    pCodecCtx->codec_id = fmt->video_codec;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = targetformat;

    pCodecCtx->width = args->width;
    pCodecCtx->height = args->height;

    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;
    if (targetformat == AV_PIX_FMT_RGB8) {
        pFormatCtx->flags = AVFMT_FLAG_AUTO_BSF | AVFMT_FLAG_FLUSH_PACKETS;
        avio_open(&pFormatCtx->pb, args->outfile, "w");
    }
    //Output some information
    av_dump_format(pFormatCtx, 0, args->outfile, 1);

    if (targetformat == AV_PIX_FMT_RGB24) {
        //pCodecCtx->codec_id = av_guess_codec(fmt, NULL, args->outfile,NULL,AVMEDIA_TYPE_VIDEO);
        pCodecCtx->codec_id = ff_guess_image2_codec(args->outfile);
    }
    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Codec not found.");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.");
        return -1;
    }

    picture = av_frame_alloc();
    if (targetformat != AV_PIX_FMT_YUVJ420P) {
        pictureTarget = av_frame_alloc();
    } else {
        pictureTarget = picture;
    }
    size =
        avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width,
                           pCodecCtx->height);
    picture_buf = (uint8_t *) av_malloc(size);
    if (!picture_buf) {
        return -1;
    }

    avpicture_fill((AVPicture *) pictureTarget, picture_buf,
                   pCodecCtx->pix_fmt, pCodecCtx->width,
                   pCodecCtx->height);
    if (targetformat != AV_PIX_FMT_YUVJ420P) {
        sws_ctx =
            sws_getContext(pCodecCtx->width, pCodecCtx->height,
                           AV_PIX_FMT_YUV420P, pCodecCtx->width,
                           pCodecCtx->height, pCodecCtx->pix_fmt,
                           SWS_BILINEAR, NULL, NULL, NULL);
        if (sws_ctx == NULL) {
            printf("sws_getContext failed!\n");
            return -1;
        }
    }
    //Write Header
    avformat_write_header(pFormatCtx, NULL);

    y_size = pCodecCtx->width * pCodecCtx->height;
    char *buf = (uint8_t *) malloc(y_size * 3 / 2 + 1);

    if (targetformat != AV_PIX_FMT_YUVJ420P) {
        pictureTarget->width = args->width;
        pictureTarget->height = args->height;
    }
    if (targetformat == AV_PIX_FMT_RGB8) {
        pictureTarget->format = AV_PIX_FMT_RGB8;
    }
    //Encode
    for (i = 0; i < framenum; i++) {
        av_init_packet(&pkt);
        pkt.data = NULL;        // packet data will be allocated by the encoder
        pkt.size = 0;
        //Read raw YUV data
        if (fread(buf, 1, y_size * 3 / 2, in_file) <= 0) {
            printf("Could not read input file.");
            return -1;
        }
        picture->data[0] = buf; // Y
        picture->data[1] = buf + y_size; // U
        picture->data[2] = buf + y_size * 5 / 4; // V

        if (targetformat != AV_PIX_FMT_YUVJ420P) {
            picture->linesize[0] = pCodecCtx->width;
            picture->linesize[1] = pCodecCtx->width / 2;
            picture->linesize[2] = pCodecCtx->width / 2;

            sws_scale(sws_ctx,
                      (uint8_t const *const *) picture->data,
                      picture->linesize, 0,
                      pCodecCtx->height,
                      pictureTarget->data, pictureTarget->linesize);
        }

        pictureTarget->pts = i;
        /* encode the image */
        ret =
            avcodec_encode_video2(pCodecCtx, &pkt, pictureTarget,
                                  &got_picture);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }

        if (got_picture) {
            printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt,
                   pkt.size);
            framecnt++;
            pkt.stream_index = video_st->index;
            ret = av_write_frame(pFormatCtx, &pkt);
            av_free_packet(&pkt);
        }
    }

    //Write Trailer
    av_write_trailer(pFormatCtx);

    printf("Encode Successful.\n");

    if (video_st) {
        avcodec_close(video_st->codec);
        av_free(picture);
        av_free(picture_buf);
        av_free(buf);
    }
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);

    fclose(in_file);

    return 0;
}

int yuv420p2picture(cmdArgsPtr args)
{
    const char *ext;

    ext = strrchr(args->outfile, '.');
    if (!strcmp(ext + 1, "png")) {
        do_encode(args, AV_PIX_FMT_RGB24);
    } else if (!strcmp(ext + 1, "jpg")) {
        do_encode(args, AV_PIX_FMT_YUVJ420P);
    } else if (!strcmp(ext + 1, "gif")) {
        do_encode(args, AV_PIX_FMT_RGB8);
    }
}
