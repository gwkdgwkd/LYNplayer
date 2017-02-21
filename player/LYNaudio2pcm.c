#include "LYNtype.h"

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

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

static int audio_decode(cmdArgsPtr args)
{
    AVCodec *codec;
    AVCodecContext *c = NULL;
    int len;
    FILE *f, *outfile;
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;
    AVFrame *decoded_frame = NULL;
    AVFrame *pcm_frame = NULL;
    uint8_t *pcm_buf;
    int pcm_size;
    struct SwrContext *swr_ctx = NULL;
    int ret;
    int src_nb_samples = 1024, dst_nb_samples, max_dst_nb_samples;
    int src_rate = 44100, dst_rate = 44100;

    av_register_all();
    /* register all the codecs */
    //avcodec_register_all();

    AVOutputFormat *fmt;
    fmt = av_guess_format(NULL, args->infile, NULL);

    av_init_packet(&avpkt);

    printf("Decode audio file %s to %s\n", args->infile, args->outfile);

    /* find the MPEG audio decoder */
    codec = avcodec_find_decoder(fmt->audio_codec);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    f = fopen(args->infile, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", args->infile);
        exit(1);
    }
    outfile = fopen(args->outfile, "wb");
    if (!outfile) {
        av_free(c);
        exit(1);
    }

    if (codec->id == AV_CODEC_ID_AAC) {
        /** Initialize the resampler to be able to convert audio sample formats. */
        if (init_resampler(AV_CH_LAYOUT_STEREO, c->sample_fmt, src_rate,
                           AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                           dst_rate, &swr_ctx)) {
            printf("Failed to init resampler!\n");
            return -1;
        }

        c->frame_size = src_nb_samples;
        pcm_frame = av_frame_alloc();
        pcm_frame->nb_samples = c->frame_size;
        pcm_frame->format = AV_SAMPLE_FMT_S16;
        pcm_size =
            av_samples_get_buffer_size(NULL, AV_CH_LAYOUT_STEREO,
                                       c->frame_size,
                                       AV_SAMPLE_FMT_S16, 1);
        pcm_buf = (uint8_t *) av_malloc(pcm_size);
        avcodec_fill_audio_frame(pcm_frame, AV_CH_LAYOUT_STEREO,
                                 AV_SAMPLE_FMT_S16,
                                 (const uint8_t *) pcm_buf, pcm_size, 1);
    }

    /* decode until eof */
    avpkt.data = inbuf;
    avpkt.size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);
    if (codec->id == AV_CODEC_ID_MP3) {
        int id3len = ff_id3v2_tag_len(inbuf);
        printf("id3len is %d\n", id3len);
        avpkt.size -= id3len;
        avpkt.data = inbuf + id3len;
    }

    while (avpkt.size > 0) {
        int i, ch;
        int got_frame = 0;

        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
                fprintf(stderr, "Could not allocate audio frame\n");
                exit(1);
            }
        }

        len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &avpkt);
        if (len < 0) {
            fprintf(stderr, "Error while decoding\n");
            exit(1);
        }

        if (got_frame) {

            if (codec->id == AV_CODEC_ID_AAC) {
                /* compute destination number of samples */
                dst_nb_samples =
                    av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
                                   src_nb_samples, dst_rate, src_rate,
                                   AV_ROUND_UP);
                /** Convert the samples using the resampler. */
                swr_convert(swr_ctx, pcm_frame->data, dst_nb_samples,
                            (const uint8_t **) decoded_frame->data,
                            src_nb_samples);

                if (ret < 0) {
                    fprintf(stderr, "Error while converting\n");
                    return ret;
                }
            }

            /* if a frame has been decoded, output it */
            int data_size = av_get_bytes_per_sample(c->sample_fmt);
            if (data_size < 0) {
                /* This should not occur, checking just for paranoia */
                fprintf(stderr, "Failed to calculate data size\n");
                exit(1);
            }
            printf("data_size %d\n", data_size);
            printf("av_sample_fmt_is_planar(c->sample_fmt) planar is %d\n",
                   av_sample_fmt_is_planar(c->sample_fmt));
            printf("c->sample_fmt planar is %d\n", c->sample_fmt);

            if (codec->id == AV_CODEC_ID_AAC) {
                for (i = 0; i < pcm_frame->nb_samples; i++) {
                    fwrite(pcm_frame->data[ch] + data_size * i, 1,
                           data_size, outfile);
                }
            } else {
                for (i = 0; i < decoded_frame->nb_samples; i++) {
                    for (ch = 0; ch < c->channels; ch++)
                        fwrite(decoded_frame->data[ch] + data_size * i, 1,
                               data_size, outfile);
                }
            }
        }
        avpkt.size -= len;
        avpkt.data += len;
        avpkt.dts = avpkt.pts = AV_NOPTS_VALUE;
        if (avpkt.size < AUDIO_REFILL_THRESH) {
            /* Refill the input buffer, to avoid trying to decode
             * incomplete frames. Instead of this, one could also use
             * a parser, or use a proper container format through
             * libavformat. */
            memmove(inbuf, avpkt.data, avpkt.size);
            avpkt.data = inbuf;
            len = fread(avpkt.data + avpkt.size, 1,
                        AUDIO_INBUF_SIZE - avpkt.size, f);
            if (len > 0)
                avpkt.size += len;
        }
    }

    if (codec->id == AV_CODEC_ID_AAC) {
        swr_free(&swr_ctx);
        av_frame_free(&pcm_frame);
    }
    fclose(outfile);
    fclose(f);

    avcodec_close(c);
    av_free(c);
    av_frame_free(&decoded_frame);
}

int audio2pcm(cmdArgsPtr args)
{
    int ret;

    ret = audio_decode(args);

    return ret;
}
