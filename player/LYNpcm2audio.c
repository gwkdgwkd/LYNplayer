#include "LYNtype.h"
int flush_encoder(AVFormatContext * fmt_ctx, unsigned int stream_index)
{
    int ret;
    int got_frame;

    AVPacket enc_pkt;
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
          CODEC_CAP_DELAY))
        return 0;
    while (1) {
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret =
            avcodec_encode_audio2(fmt_ctx->streams[stream_index]->codec,
                                  &enc_pkt, NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame) {
            ret = 0;
            break;
        }
        printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",
               enc_pkt.size);
        /* mux encoded frame */
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}

static int init_resampler(int src_ch_layout,
                          enum AVSampleFormat src_sample_fmt, int src_rate,
                          int dst_ch_layout,
                          enum AVSampleFormat dst_sample_fmt, int dst_rate,
                          struct swrContext **resample_context)
{
    int ret;
#if 0
        /**
         * Create a resampler context for the conversion.
         * Set the conversion parameters.
         * Default channel layouts based on the number of channels
         * are assumed for simplicity (they are sometimes not detected
         * properly by the demuxer and/or decoder).
         */
    *resample_context = swr_alloc_set_opts(NULL,
                                           src_ch_layout,
                                           src_sample_fmt,
                                           src_rate,
                                           dst_ch_layout,
                                           dst_sample_fmt,
                                           dst_rate, 0, NULL);
#else
    /* create resampler context */
    *resample_context = swr_alloc();
    if (!*resample_context) {
        fprintf(stderr, "Could not allocate resampler context\n");
        return -1;
    }

    /* set options */
    av_opt_set_int(*resample_context, "in_channel_layout", src_ch_layout,
                   0);
    av_opt_set_int(*resample_context, "in_sample_rate", src_rate, 0);
    av_opt_set_sample_fmt(*resample_context, "in_sample_fmt",
                          src_sample_fmt, 0);

    av_opt_set_int(*resample_context, "out_channel_layout", dst_ch_layout,
                   0);
    av_opt_set_int(*resample_context, "out_sample_rate", dst_rate, 0);
    av_opt_set_sample_fmt(*resample_context, "out_sample_fmt",
                          dst_sample_fmt, 0);
#endif
    if (!*resample_context) {
        fprintf(stderr, "Could not allocate resample context\n");
        return -1;
    }

        /** Open the resampler with the specified parameters. */
    if ((ret = swr_init(*resample_context)) < 0) {
        fprintf(stderr, "Could not open resample context\n");
        swr_free(resample_context);
        return ret;
    }
    return 0;
}

int audio_encode(cmdArgsPtr args, enum AVSampleFormat targetformat)
{
    AVFormatContext *pFormatCtx;
    AVOutputFormat *fmt;
    AVStream *audio_st;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    uint8_t *frame_buf;
    AVFrame *pFrame, *audioTarget;
    AVPacket pkt;
    struct SwrContext *swr_ctx = NULL;

    int got_frame = 0;
    int ret = 0;
    int size = 0;

    FILE *in_file = NULL;       //Raw PCM data
    int framenum = 1000;        //Audio frame number
    int i;

    in_file = fopen(args->infile, "rb");

    av_register_all();

    //Method 1.
    pFormatCtx = avformat_alloc_context();
    fmt = av_guess_format(NULL, args->outfile, NULL);
    pFormatCtx->oformat = fmt;

    //Method 2.
    //avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, args->outfile);
    //fmt = pFormatCtx->oformat;

    //Open output URL
    if (avio_open(&pFormatCtx->pb, args->outfile, AVIO_FLAG_READ_WRITE) <
        0) {
        printf("Failed to open output file!\n");
        return -1;
    }
    audio_st = avformat_new_stream(pFormatCtx, 0);
    if (audio_st == NULL) {
        return -1;
    }
    pCodecCtx = audio_st->codec;
    pCodecCtx->codec_id = fmt->audio_codec;
    pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    pCodecCtx->sample_fmt = targetformat;
    pCodecCtx->sample_rate = 44100;
    pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    pCodecCtx->channels =
        av_get_channel_layout_nb_channels(pCodecCtx->channel_layout);
    //pCodecCtx->bit_rate = 64000;
    //Show some information
    av_dump_format(pFormatCtx, 0, args->outfile, 1);

    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Can not find encoder!\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Failed to open encoder!\n");
        return -1;
    }

    /** Initialize the resampler to be able to convert audio sample formats. */
    if (init_resampler(AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
                       pCodecCtx->channel_layout, pCodecCtx->sample_fmt,
                       pCodecCtx->sample_rate, &swr_ctx)) {
        printf("Failed to init resampler!\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    audioTarget = av_frame_alloc();
    audioTarget->nb_samples = pCodecCtx->frame_size;
    audioTarget->format = pCodecCtx->sample_fmt;
    size =
        av_samples_get_buffer_size(NULL, pCodecCtx->channels,
                                   pCodecCtx->frame_size,
                                   pCodecCtx->sample_fmt, 1);
    frame_buf = (uint8_t *) av_malloc(size);
    avcodec_fill_audio_frame(audioTarget, pCodecCtx->channels,
                             pCodecCtx->sample_fmt,
                             (const uint8_t *) frame_buf, size, 1);
    //Write Header
    avformat_write_header(pFormatCtx, NULL);
    av_new_packet(&pkt, size);
    for (i = 0; i < framenum; i++) {
        //Read PCM
        if (fread(frame_buf, 1, size, in_file) <= 0) {
            printf("Failed to read raw data! \n");
            return -1;
        } else if (feof(in_file)) {
            break;
        }
        pFrame->data[0] = frame_buf; //PCM Data
        pFrame->pts = i * 100;
        ret =
            swr_convert(swr_ctx, audioTarget->data, 1024,
                        (const uint8_t **) pFrame->data, 1024);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            return ret;
        }
        got_frame = 0;
        //Encode
        ret =
            avcodec_encode_audio2(pCodecCtx, &pkt, audioTarget,
                                  &got_frame);
        if (ret < 0) {
            printf("Failed to encode!\n");
            return -1;
        }
        if (got_frame == 1) {
            printf("Succeed to encode 1 frame! \tsize:%5d\n", pkt.size);
            pkt.stream_index = audio_st->index;
            ret = av_write_frame(pFormatCtx, &pkt);
            av_free_packet(&pkt);
        }
    }
    //Flush Encoder
    ret = flush_encoder(pFormatCtx, 0);
    if (ret < 0) {
        printf("Flushing encoder failed\n");
        return -1;
    }
    //Write Trailer
    av_write_trailer(pFormatCtx);
    //Clean
    if (audio_st) {
        avcodec_close(audio_st->codec);
        av_free(pFrame);
        av_free(audioTarget);
        av_free(frame_buf);
    }
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
    fclose(in_file);
    return 0;
}

int pcm2audio(cmdArgsPtr args)
{
    audio_encode(args, AV_SAMPLE_FMT_FLTP);
}
