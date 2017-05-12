#include "LYNtype.h"
#include <libavutil/mathematics.h>

//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 1

#if 1

char *get_extname(const char *name)
{
    if (!strcmp("mpeg4", name)) {
        return "mp4";           //avi
    } else if (!strcmp("flv1", name)) {
        return "flv";
    } else if (!strcmp("rv40", name)) {
        return "rm";
    } else if (!strcmp("cook", name)) {
        return "ra";
    } else {
        return name;
    }
}

//flv,ts works well.mp3 and aac all good
int demuxer(cmdArgsPtr args)
{
    AVOutputFormat *ofmt_a = NULL, *ofmt_v = NULL;
    //(input AVFormatContext and Output AVFormatContext)
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx_a = NULL, *ofmt_ctx_v =
        NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex = -1, audioindex = -1;
    int frame_index = 0;

    const char *in_filename = args->infile; //Input file URL
    char out_filename_v[64];    //Output file URL
    char out_filename_a[64];

    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVFormatContext *ofmt_ctx;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = NULL;
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            sprintf(out_filename_v, "%s.%s", args->outfile,
                    get_extname(avcodec_descriptor_get
                                (ifmt_ctx->streams[i]->codec->
                                 codec_id)->name));
            avformat_alloc_output_context2(&ofmt_ctx_v, NULL, NULL,
                                           out_filename_v);
            if (!ofmt_ctx_v) {
                printf("Could not create output context\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            ofmt_v = ofmt_ctx_v->oformat;
            out_stream =
                avformat_new_stream(ofmt_ctx_v, in_stream->codec->codec);
            ofmt_ctx = ofmt_ctx_v;
        } else if (ifmt_ctx->streams[i]->codec->codec_type ==
                   AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            sprintf(out_filename_a, "%s.%s", args->outfile,
                    get_extname(avcodec_descriptor_get
                                (ifmt_ctx->streams[i]->codec->
                                 codec_id)->name));
            avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL,
                                           out_filename_a);
            if (!ofmt_ctx_a) {
                printf("Could not create output context\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            ofmt_a = ofmt_ctx_a->oformat;
            out_stream =
                avformat_new_stream(ofmt_ctx_a, in_stream->codec->codec);
            ofmt_ctx = ofmt_ctx_a;
        } else {
            break;
        }

        if (!out_stream) {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        //Copy the settings of AVCodecContext
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            printf
                ("Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    //Dump Format------------------
    printf("\n==============Input Video=============\n");
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    printf("\n==============Output Video============\n");
    av_dump_format(ofmt_ctx_v, 0, out_filename_v, 1);
    printf("\n==============Output Audio============\n");
    av_dump_format(ofmt_ctx_a, 0, out_filename_a, 1);
    printf("\n======================================\n");
    //Open output file
    if (!(ofmt_v->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_v->pb, out_filename_v, AVIO_FLAG_WRITE) <
            0) {
            printf("Could not open output file '%s'", out_filename_v);
            goto end;
        }
    }

    if (!(ofmt_a->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_a->pb, out_filename_a, AVIO_FLAG_WRITE) <
            0) {
            printf("Could not open output file '%s'", out_filename_a);
            goto end;
        }
    }
    //Write file header
    if (avformat_write_header(ofmt_ctx_v, NULL) < 0) {
        printf("Error occurred when opening video output file\n");
        goto end;
    }
    if (avformat_write_header(ofmt_ctx_a, NULL) < 0) {
        printf("Error occurred when opening audio output file\n");
        goto end;
    }
#if USE_H264BSF
    AVBitStreamFilterContext *h264bsfc =
        av_bitstream_filter_init("h264_mp4toannexb");
#endif

    while (1) {
        AVFormatContext *ofmt_ctx;
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        if (av_read_frame(ifmt_ctx, &pkt) < 0)
            break;
        in_stream = ifmt_ctx->streams[pkt.stream_index];


        if (pkt.stream_index == videoindex) {
            out_stream = ofmt_ctx_v->streams[0];
            ofmt_ctx = ofmt_ctx_v;
            printf("Write Video Packet. size:%d\tpts:%lld\n", pkt.size,
                   pkt.pts);
#if USE_H264BSF
            av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL,
                                       &pkt.data, &pkt.size, pkt.data,
                                       pkt.size, 0);
#endif
        } else if (pkt.stream_index == audioindex) {
            out_stream = ofmt_ctx_a->streams[0];
            ofmt_ctx = ofmt_ctx_a;
            printf("Write Audio Packet. size:%d\tpts:%lld\n", pkt.size,
                   pkt.pts);
        } else {
            continue;
        }


        //Convert PTS/DTS
        pkt.pts =
            av_rescale_q_rnd(pkt.pts, in_stream->time_base,
                             out_stream->time_base,
                             (enum AVRounding) (AV_ROUND_NEAR_INF |
                                                AV_ROUND_PASS_MINMAX));
        pkt.dts =
            av_rescale_q_rnd(pkt.dts, in_stream->time_base,
                             out_stream->time_base,
                             (enum AVRounding) (AV_ROUND_NEAR_INF |
                                                AV_ROUND_PASS_MINMAX));
        pkt.duration =
            av_rescale_q(pkt.duration, in_stream->time_base,
                         out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = 0;
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            printf("Error muxing packet\n");
            break;
        }
        //printf("Write %8d frames to output file\n",frame_index);
        av_free_packet(&pkt);
        frame_index++;
    }

#if USE_H264BSF
    av_bitstream_filter_close(h264bsfc);
#endif

    //Write file trailer
    av_write_trailer(ofmt_ctx_a);
    av_write_trailer(ofmt_ctx_v);
  end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx_a && !(ofmt_a->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_a->pb);

    if (ofmt_ctx_v && !(ofmt_v->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_v->pb);

    avformat_free_context(ofmt_ctx_a);
    avformat_free_context(ofmt_ctx_v);


    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}
#else
//add adts for aac audio,but zhe aac file can't play,why?
void write_adts(int psize, FILE * fp)
{
    char *padts = (char *) malloc(sizeof(char) * 7);
    int profile = 2;            //AAC LC
    int freqIdx = 4;            //44.1KHz
    int chanCfg = 2;            //MPEG-4 Audio Channel Configuration. 1 Channel front-center
    padts[0] = (char) 0xFF;     // 11111111     = syncword
    padts[1] = (char) 0xF1;     // 1111 1 00 1  = syncword MPEG-2 Layer CRC
    padts[2] =
        (char) (((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));
    padts[6] = (char) 0xFC;

    padts[3] = (char) (((chanCfg & 3) << 6) + ((7 + psize) >> 11));
    padts[4] = (char) (((7 + psize) & 0x7FF) >> 3);
    padts[5] = (char) ((((7 + psize) & 7) << 5) + 0x1F);

    fwrite(padts, 7, 1, fp);
}

//flv works well. if audio is aac,can't play
int demuxer(cmdArgsPtr args)
{
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex = -1, audioindex = -1;
    const char *in_filename = args->infile; //Input file URL
    char out_filename_v[64];    //Output file URL
    char out_filename_a[64];

    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.\n");
        return -1;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        return -1;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            sprintf(out_filename_v, "%s.%s", args->outfile,
                    avcodec_descriptor_get(ifmt_ctx->streams[i]->
                                           codec->codec_id)->name);
        } else if (ifmt_ctx->streams[i]->codec->codec_type ==
                   AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            sprintf(out_filename_a, "%s.%s", args->outfile,
                    avcodec_descriptor_get(ifmt_ctx->streams[i]->
                                           codec->codec_id)->name);
        }
    }
    //Dump Format------------------
    printf("\nInput Video===========================\n");
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    printf("\n======================================\n");

    FILE *fp_audio = fopen(out_filename_a, "wb+");
    FILE *fp_video = fopen(out_filename_v, "wb+");

    /*
       FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
       "h264_mp4toannexb" bitstream filter (BSF)
       *Add SPS,PPS in front of IDR frame
       *Add start code ("0,0,0,1") in front of NALU
       H.264 in some container (MPEG2TS) don't need this BSF.
     */
#if USE_H264BSF
    AVBitStreamFilterContext *h264bsfc =
        av_bitstream_filter_init("h264_mp4toannexb");
#endif

    while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == videoindex) {
#if USE_H264BSF
            av_bitstream_filter_filter(h264bsfc,
                                       ifmt_ctx->
                                       streams[videoindex]->codec, NULL,
                                       &pkt.data, &pkt.size, pkt.data,
                                       pkt.size, 0);
#endif
            printf("Write Video Packet. size:%d\tpts:%lld\n", pkt.size,
                   pkt.pts);
            fwrite(pkt.data, 1, pkt.size, fp_video);
        } else if (pkt.stream_index == audioindex) {
            /*
               AAC in some container format (FLV, MP4, MKV etc.) need to add 7 Bytes
               ADTS Header in front of AVPacket data manually.
               Other Audio Codec (MP3...) works well.
             */
            printf("Write Audio Packet. size:%d\tpts:%lld\n", pkt.size,
                   pkt.pts);
            //write_adts(pkt.size,fp_audio);
            fwrite(pkt.data, 1, pkt.size, fp_audio);
        }
        av_free_packet(&pkt);
    }

#if USE_H264BSF
    av_bitstream_filter_close(h264bsfc);
#endif

    fclose(fp_video);
    fclose(fp_audio);

    avformat_close_input(&ifmt_ctx);

    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}
#endif
