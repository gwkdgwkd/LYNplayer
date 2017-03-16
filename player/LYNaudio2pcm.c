#include "LYNtype.h"

//#define USEAVFORMAT

#ifdef USEAVFORMAT
#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
static int audio_decode(cmdArgsPtr args)
{
    AVFormatContext *pFormatCtx;
    int i, audioStream;
    AVCodecContext *pCodecCtx;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodec *pCodec;
    AVPacket *packet;
    uint8_t *out_buffer;
    AVFrame *pFrame;
    int ret;
    uint32_t len = 0;
    int got_picture;
    int index = 0;
    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;

    FILE *pFile = fopen(args->outfile, "wb");
    char *url = args->infile;

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    //Open
    if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    // Dump valid information onto standard error
    av_dump_format(pFormatCtx, 0, url, 0);

    // Find the first audio stream
    audioStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codec->codec_type ==
            AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
            break;
        }

    if (audioStream == -1) {
        printf("Didn't find a audio stream.\n");
        return -1;
    }
    // Get a pointer to the codec context for the audio stream
    pCodecCtxOrig = pFormatCtx->streams[audioStream]->codec;
    //pCodecCtx=pFormatCtx->streams[audioStream]->codec;

    // Find the decoder for the audio stream
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }

    /* Copy context */
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        fprintf(stderr, "Couldn't copy codec context");
        return (-1);            /* Error copying codec context */
    }
    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }
    //return 0;
    packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    av_init_packet(packet);

    //Out Audio Param
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    //nb_samples: AAC-1024 MP3-1152
    int out_nb_samples = pCodecCtx->frame_size;
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int out_channels =
        av_get_channel_layout_nb_channels(out_channel_layout);
    //Out Buffer Size
    int out_buffer_size =
        av_samples_get_buffer_size(NULL, out_channels, out_nb_samples,
                                   out_sample_fmt, 1);
    out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
    pFrame = av_frame_alloc();

    //FIX:Some Codec's Context Information is missing
    in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
    //Swr
    au_convert_ctx = swr_alloc();
    au_convert_ctx =
        swr_alloc_set_opts(au_convert_ctx, out_channel_layout,
                           out_sample_fmt, out_sample_rate,
                           in_channel_layout, pCodecCtx->sample_fmt,
                           pCodecCtx->sample_rate, 0, NULL);
    swr_init(au_convert_ctx);

    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == audioStream) {

            ret =
                avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture,
                                      packet);
            if (ret < 0) {
                printf("Error in decoding audio frame.\n");
                return -1;
            }
            if (got_picture > 0) {
                //swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);
                int len = swr_convert(au_convert_ctx, &out_buffer,
                                      pFrame->nb_samples + 32,
                                      (const uint8_t **) pFrame->data,
                                      pFrame->nb_samples);

                int len2 =
                    len * 2 * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                //printf("index:%5d\t pts:%lld\t packet size:%d\n",index,packet->pts,packet->size);

                //Write PCM
                fwrite(out_buffer, 1, len2, pFile);
                //fwrite(out_buffer, 1, out_buffer_size, pFile);
                index++;
            }
        }
        av_free_packet(packet);
    }

    swr_free(&au_convert_ctx);

    fclose(pFile);

    av_free(out_buffer);
    // Close the codec
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);
    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}


#else
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

static unsigned long long hex2int(unsigned char *buf, int bytenum,
                                  int islittle)
{
    int i;
    int index;
    unsigned long long ret = 0;
    unsigned long long tmp = 0;

    for (i = 0; i < bytenum; i++) {
        index = (islittle == 1) ? bytenum - 1 - i : i;
        tmp = buf[index];
        ret |= tmp << (bytenum - 1 - i) * 8;
    }

    return ret;
}

static unsigned char *find_guid(unsigned char *str, int str_size,
                                unsigned char *sub, int sub_size)
{
    int i, j, m;

    for (i = 0; i < str_size; i++) {
        m = i;
        j = 0;
        while (str[i++] == sub[j++]) {
            if (j == sub_size) {
                return &str[m];
            }
        }
        i = m;
    }

    return NULL;
}

static unsigned long long get_header_len(FILE * f)
{
    unsigned char header_len_buf[8] = { 0 };

    fseek(f, 16, SEEK_SET);
    if (fread(header_len_buf, 1, 8, f) < 0) {
        printf("read wma file header len failed.\n");
        return 0;
    }

    return hex2int(header_len_buf, 8, 1);
}

static int get_wma_file_audio_stream_properties(AVCodecContext * ctx,
                                                FILE * f)
{
    unsigned long long header_len;
    unsigned char *header;
    const unsigned char audio_info_guid[16] =
        { 0x50, 0xCD, 0xC3, 0xBF, 0x8F, 0x61, 0xCF, 0x11, 0x8B, 0xB2, 0x00,
        0xAA, 0x00, 0xB4, 0xE2, 0x20
    };
    unsigned char *audio_info;

    if (f == NULL || ctx == NULL) {
        printf("f or ctx is NULL\n");
        return -1;
    }

    header_len = get_header_len(f);

    if ((header = malloc(header_len)) == NULL) {
        printf("malloc header buffer failed.\n");
        return -1;
    }

    if (fread(header, 1, header_len, f) < header_len) {
        printf("read wma file header failed.\n");
        free(header);
        return -1;
    }

    if ((audio_info =
         find_guid(header, header_len, audio_info_guid, 16)) == NULL) {
        printf("find audio info guid failed.\n");
        free(header);
        return -1;
    }

    audio_info += 16 + 8 + 4 + 4 + 2 + 4 + 2;
    ctx->channels = hex2int(audio_info, 2, 1);
    ctx->sample_rate = hex2int(audio_info + 2, 4, 1);
    ctx->bit_rate = hex2int(audio_info + 6, 4, 1) * 8LL;
    ctx->block_align = hex2int(audio_info + 10, 2, 1);
    audio_info += 2 + 4 + 4 + 2 + 2;
    ctx->extradata_size = hex2int(audio_info, 2, 1);
    if ((ctx->extradata = malloc(ctx->extradata_size)) == NULL) {
        printf("malloc extradata buffer failed.\n");
        free(header);
        return -1;
    }
    memcpy(ctx->extradata, audio_info + 2, ctx->extradata_size);

    free(header);
    return 0;
}

static int get_len(int c, int bit)
{
    switch ((c >> bit) & 3) {
    case 3:
        return 4;
    case 2:
        return 2;
    case 1:
        return 1;
      defulat:
        return 0;
    }
}

static void jump_to_audio_data(FILE * f)
{
    const unsigned char data_guid[16] =
        { 0x36, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00,
        0xaa, 0x00, 0x62, 0xce, 0x6c
    };
    unsigned char guid[16];
    int seeknum;
    int c, d;
    unsigned long long header_len = get_header_len(f);

    fseek(f, header_len, SEEK_SET);
    fread(guid, 1, 16, f);
    if (memcmp(guid, data_guid, 16)) {
        printf("wrong data header\n");
        return NULL;
    }

    seeknum = 8 + 16 + 8 + 1 + 1;
    fseek(f, seeknum, SEEK_CUR);
    c = fgetc(f);
    seeknum = c & 0x0F;
    fseek(f, seeknum, SEEK_CUR);
    c = fgetc(f);
    d = fgetc(f);
    seeknum = get_len(c, 5) + get_len(c, 1) + get_len(c, 3) + 4 + 2;
    fseek(f, seeknum, SEEK_CUR);
    if (c & 0x01) {
        c = fgetc(f);
    }
    seeknum =
        1 + get_len(d, 4) + get_len(d, 2) + get_len(d,
                                                    0) + 4 + 4 + get_len(c,
                                                                         6);
    fseek(f, seeknum, SEEK_CUR);
}

static int get_packet_size(FILE * f)
{
    unsigned long long header_len;
    unsigned char *header;
    const unsigned char file_header_guid[16] =
        { 0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00,
        0xC0, 0x0C, 0x20, 0x53, 0x65
    };
    unsigned char *file_properties;
    int seeknum;

    if (f == NULL) {
        printf("f is NULL\n");
        return -1;
    }

    header_len = get_header_len(f);

    if ((header = malloc(header_len)) == NULL) {
        printf("malloc header buffer failed.\n");
        return -1;
    }

    if (fread(header, 1, header_len, f) < header_len) {
        printf("read wma file header failed.\n");
        free(header);
        return -1;
    }

    if ((file_properties =
         find_guid(header, header_len, file_header_guid, 16)) == NULL) {
        printf("find audio info guid failed.\n");
        free(header);
        return -1;
    }

    seeknum = 16 + 8 + 16 + 8 + 8 + 8 + 8 + 8 + 4 + 4 + 4 + 4;

    return hex2int(file_properties + seeknum, 4, 1);
}

static int audio_decode(cmdArgsPtr args)
{
    AVCodec *codec;
    AVCodecContext *c = NULL;
    int len;
    FILE *f, *outfile, *ac3_pcm[6] = { 0 };
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
    int packet_size = AUDIO_INBUF_SIZE;
    int last_nb_samples = 0;
    int codec_id;

    /* register all the codecs */
    avcodec_register_all();

    av_init_packet(&avpkt);

    printf("Decode audio file %s to %s\n", args->infile, args->outfile);

    if (!strcmp(strrchr(args->infile, '.') + 1, "mp2")) {
        codec_id = AV_CODEC_ID_MP2;
    } else if (!strcmp(strrchr(args->infile, '.') + 1, "mp3")) {
        codec_id = AV_CODEC_ID_MP3;
    } else if (!strcmp(strrchr(args->infile, '.') + 1, "aac")) {
        codec_id = AV_CODEC_ID_AAC;
    } else if (!strcmp(strrchr(args->infile, '.') + 1, "wma")) {
        codec_id = AV_CODEC_ID_WMAV2;
    } else if (!strcmp(strrchr(args->infile, '.') + 1, "ac3")) {
        codec_id = AV_CODEC_ID_AC3;
    } else {
        printf("unknown file ext name.");
        return -1;
    }

    /* find the MPEG audio decoder */
    codec = avcodec_find_decoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }

    f = fopen(args->infile, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", args->infile);
        exit(1);
    }

    if (codec->id == AV_CODEC_ID_WMAV2) {
        src_nb_samples = 2048;
        if ((ret = get_wma_file_audio_stream_properties(c, f)) < 0) {
            printf("get_wma_file_audio_stream_properties failed.\n");
            return ret;
        }

        if ((packet_size = get_packet_size(f)) < 0) {
            printf("get_packet_size failed.\n");
            return -1;
        }

        jump_to_audio_data(f);
    }

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    if (codec->id != AV_CODEC_ID_AC3) {
        outfile = fopen(args->outfile, "wb");
        if (!outfile) {
            av_free(c);
            exit(1);
        }
    }

    if (codec->id != AV_CODEC_ID_AC3) {
        /** Initialize the resampler to be able to convert audio sample formats. */
        if (init_resampler(AV_CH_LAYOUT_STEREO, c->sample_fmt, src_rate,
                           AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                           dst_rate, &swr_ctx)) {
            printf("Failed to init resampler!\n");
            return -1;
        }
    }

    /* decode until eof */
    avpkt.data = inbuf;
    avpkt.size = fread(inbuf, 1, packet_size, f);

    if (codec->id == AV_CODEC_ID_MP3) {
        int id3len = ff_id3v2_tag_len(inbuf);
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
            int swr_len;
            if (codec->id != AV_CODEC_ID_AC3) {
                if (!pcm_frame) {
                    if (!(pcm_frame = av_frame_alloc())) {
                        fprintf(stderr,
                                "Could not allocate audio frame\n");
                        exit(1);
                    }
                }
                /* compute destination number of samples */
                //dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx,
                //src_rate) + src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
                if (last_nb_samples != decoded_frame->nb_samples) {
                    if (pcm_buf != NULL) {
                        free(pcm_buf);
                    }
                    pcm_frame->nb_samples = last_nb_samples =
                        decoded_frame->nb_samples;
                    pcm_frame->format = AV_SAMPLE_FMT_S16;
                    pcm_size =
                        av_samples_get_buffer_size(NULL, 2,
                                                   pcm_frame->nb_samples,
                                                   AV_SAMPLE_FMT_S16, 1);
                    pcm_buf = (uint8_t *) av_malloc(pcm_size);
                    avcodec_fill_audio_frame(pcm_frame, 2,
                                             AV_SAMPLE_FMT_S16,
                                             (const uint8_t *) pcm_buf,
                                             pcm_size, 1);
                }
                /** Convert the samples using the resampler. */
                swr_len =
                    swr_convert(swr_ctx, pcm_frame->data,
                                decoded_frame->nb_samples + 32,
                                (const uint8_t **) decoded_frame->data,
                                decoded_frame->nb_samples);
                if (swr_len < 0) {
                    fprintf(stderr, "Error while converting\n");
                    return ret;
                }
            } else {
                pcm_frame = decoded_frame;
            }

            int data_size = av_get_bytes_per_sample(pcm_frame->format);
            if (data_size < 0) {
                fprintf(stderr, "Failed to calculate data size\n");
                exit(1);
            }
            int planar = av_sample_fmt_is_planar(pcm_frame->format);
            if (!planar) {
                data_size *= c->channels;
            }

            for (i = 0; i < decoded_frame->nb_samples; i++) {
                for (ch = 0; ch < (planar ? c->channels : 1); ch++) {
                    if (codec->id == AV_CODEC_ID_AC3) {
                        if (ac3_pcm[ch] == NULL) {
                            char pcm_name[64];
                            sprintf(pcm_name, "%d_channel_%d_%s",
                                    c->sample_rate, ch + 1, args->outfile);
                            ac3_pcm[ch] = fopen(pcm_name, "wb");
                            if (!ac3_pcm[ch]) {
                                av_free(c);
                                exit(1);
                            }
                        }
                        outfile = ac3_pcm[ch];
                    }
                    fwrite(pcm_frame->data[ch] + data_size * i, 1,
                           data_size, outfile);
                }
            }
        }

        if (codec->id == AV_CODEC_ID_WMAV2) {
            if (fread(inbuf, 1, packet_size, f) > 0) {
                avpkt.size = packet_size;
                avpkt.data = inbuf;
            } else {
                break;
            }
        } else {
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

    }

    if (codec->id != AV_CODEC_ID_AC3) {
        swr_free(&swr_ctx);
        av_frame_free(&pcm_frame);
        free(pcm_buf);
    }
    if (codec->id != AV_CODEC_ID_AC3) {
        fclose(outfile);
    } else {
        int i;
        for (i = 0; i < 6; i++) {
            fclose(ac3_pcm[i]);
        }
    }
    fclose(f);

    if (codec->id == AV_CODEC_ID_WMAV2) {
        free(c->extradata);
    }
    avcodec_close(c);
    av_free(c);
    av_frame_free(&decoded_frame);
}
#endif
int audio2pcm(cmdArgsPtr args)
{
    int ret;

    ret = audio_decode(args);

    return ret;
}
