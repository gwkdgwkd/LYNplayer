#include "LYNtype.h"

#if 1
//http://blog.csdn.net/leixiaohua1020/article/details/39759623
#include <libavdevice/avdevice.h>

FILE *fp_write;

//Write File
int write_buffer(void *opaque, uint8_t * buf, int buf_size)
{
    if (!feof(fp_write)) {
        int true_size = fwrite(buf, 1, buf_size, fp_write);
        return true_size;
    } else {
        return -1;
    }
}

int flush_encoder1(AVFormatContext * fmt_ctx, unsigned int stream_index)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
          CODEC_CAP_DELAY))
        return 0;
    while (1) {
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n",
               stream_index);
        //ret = encode_write_frame(NULL, stream_index, &got_frame);
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret =
            avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec,
                                  &enc_pkt, NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame) {
            ret = 0;
            break;
        }
        /* prepare packet for muxing */
        enc_pkt.stream_index = stream_index;
        enc_pkt.dts = av_rescale_q_rnd(enc_pkt.dts,
                                       fmt_ctx->streams[stream_index]->
                                       codec->time_base,
                                       fmt_ctx->streams[stream_index]->
                                       time_base,
                                       (enum AVRounding) (AV_ROUND_NEAR_INF
                                                          |
                                                          AV_ROUND_PASS_MINMAX));
        enc_pkt.pts =
            av_rescale_q_rnd(enc_pkt.pts,
                             fmt_ctx->streams[stream_index]->codec->
                             time_base,
                             fmt_ctx->streams[stream_index]->time_base,
                             (enum AVRounding) (AV_ROUND_NEAR_INF |
                                                AV_ROUND_PASS_MINMAX));
        enc_pkt.duration =
            av_rescale_q(enc_pkt.duration,
                         fmt_ctx->streams[stream_index]->codec->time_base,
                         fmt_ctx->streams[stream_index]->time_base);
        av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
        /* mux encoded frame */
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}

int push(cmdArgsPtr args)
{
    int ret;
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVFormatContext *rtmp_ctx = NULL;
    int64_t start_time;
    AVPacket packet, enc_pkt;
    AVFrame *pFrameYUV, *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i = 0;
    int got_frame, enc_got_frame;
    int isusedesktop = 0;
    AVOutputFormat *ofmt = NULL;

    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int videoindex;
    AVIOContext *avio_out;
    struct SwsContext *img_convert_ctx;
    int frame_index = 0;
    unsigned char *outbuffer = NULL;

    av_register_all();

    if (!strcmp(args->infile, "grabdesktop")) {
        isusedesktop = 1;
    }

    if (1 == isusedesktop) {
        avdevice_register_all();

        //Linux
        AVDictionary *options = NULL;
        //Set some options
        //grabbing frame rate
        //av_dict_set(&options,"framerate","5",0);
        //Make the grabbed area follow the mouse
        //av_dict_set(&options,"follow_mouse","centered",0);
        //Video frame size. The default is to capture the full screen
        //av_dict_set(&options,"video_size","640x480",0);
        av_dict_set(&options, "video_size", "1920x1080", 0);
        AVInputFormat *ifmt = av_find_input_format("x11grab");
        //Grab at position 10,20
        //if(avformat_open_input(&pFormatCtx,":0.0+10,20",ifmt,&options)!=0){
        if (avformat_open_input(&ifmt_ctx, ":0.0+0,0", ifmt, &options) !=
            0) {
            printf("Couldn't open input stream.\n");
            return -1;
        }
        //save to file                                                
        //fp_write=fopen("cuc60anniversary_start.flv","wb+");         
        avformat_alloc_output_context2(&ofmt_ctx, NULL, "h264", NULL);

        outbuffer = (unsigned char *) av_malloc(32768);
    } else {
        avformat_network_init();
        if ((ret = avformat_open_input(&ifmt_ctx, args->infile, 0, 0)) < 0) {
            printf("Could not open input file.");
            goto end;
        }
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        videoindex = i;
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id),
                                NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }

    av_dump_format(ifmt_ctx, 0, "whatever", 0);

    if (1 == isusedesktop) {
        /*open output file */
        avio_out =
            avio_alloc_context(outbuffer, 32768, 1, NULL, NULL,
                               write_buffer, NULL);
        if (avio_out == NULL)
            goto end;
        //avio_out->write_packet=write_packet;
        ofmt_ctx->pb = avio_out;
        ofmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
        for (i = 0; i < 1; i++) {
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                av_log(NULL, AV_LOG_ERROR,
                       "Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }
            in_stream = ifmt_ctx->streams[i];
            dec_ctx = in_stream->codec;
            enc_ctx = out_stream->codec;
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
                enc_ctx->height = dec_ctx->height;
                enc_ctx->width = dec_ctx->width;
                printf("enc_ctx->height %d\n", enc_ctx->height);
                printf("enc_ctx->width %d\n", enc_ctx->width);
                enc_ctx->sample_aspect_ratio =
                    dec_ctx->sample_aspect_ratio;
                enc_ctx->pix_fmt = encoder->pix_fmts[0];
                printf("enc_ctx->pix_fmt %d\n", enc_ctx->pix_fmt);
                printf("dec_ctx->pix_fmt %d\n", dec_ctx->pix_fmt);
                printf("AV_PIX_FMT_NB %d,%d\n", AV_PIX_FMT_NB,
                       AV_PIX_FMT_BGR0);
                enc_ctx->time_base = dec_ctx->time_base;
                //enc_ctx->time_base.num = 1;
                //enc_ctx->time_base.den = 25;
                //H264
                enc_ctx->me_range = 16;
                enc_ctx->max_qdiff = 4;
                enc_ctx->qmin = 10;
                enc_ctx->qmax = 51;
                enc_ctx->qcompress = 0.6;
                enc_ctx->refs = 3;
                enc_ctx->bit_rate = 500000;

                ret = avcodec_open2(enc_ctx, encoder, NULL);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR,
                           "Cannot open video encoder for stream #%u\n",
                           i);
                    return ret;
                }
            } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
                av_log(NULL, AV_LOG_FATAL,
                       "Elementary stream #%d is of unknown type, cannot proceed\n",
                       i);
                return AVERROR_INVALIDDATA;
            } else {
                /* if this stream must be remuxed */
                ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
                                           ifmt_ctx->streams[i]->codec);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR,
                           "Copying stream context failed\n");
                    return ret;
                }
            }
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        int size = avpicture_get_size(enc_ctx->pix_fmt, enc_ctx->width,
                                      enc_ctx->height);
        uint8_t *picture_buf = (uint8_t *) av_malloc(size);
        if (!picture_buf) {
            return -1;
        }

        pFrameYUV = av_frame_alloc();
        avpicture_fill((AVPicture *) pFrameYUV, picture_buf,
                       enc_ctx->pix_fmt, enc_ctx->width, enc_ctx->height);
        img_convert_ctx =
            sws_getContext(dec_ctx->width, dec_ctx->height,
                           dec_ctx->pix_fmt, enc_ctx->width,
                           enc_ctx->height, enc_ctx->pix_fmt, SWS_BILINEAR,
                           NULL, NULL, NULL);

        avformat_alloc_output_context2(&rtmp_ctx, NULL, "flv", args->outfile); //RTMP
        //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP

        if (!ofmt_ctx) {
            printf("Could not create output context\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        ofmt = rtmp_ctx->oformat;
        for (i = 0; i < ofmt_ctx->nb_streams; i++) {
            AVStream *in_stream = ofmt_ctx->streams[i];
            AVStream *out_stream =
                avformat_new_stream(rtmp_ctx, in_stream->codec->codec);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            ret =
                avcodec_copy_context(out_stream->codec, in_stream->codec);
            if (ret < 0) {
                printf
                    ("Failed to copy context from input to output stream codec context\n");
                goto end;
            }
            out_stream->codec->codec_tag = 0;
            if (rtmp_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
        //Dump Format------------------
        av_dump_format(rtmp_ctx, 0, args->outfile, 1);
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            ret = avio_open(&rtmp_ctx->pb, args->outfile, AVIO_FLAG_WRITE);
            if (ret < 0) {
                printf("Could not open output URL '%s'", args->outfile);
                goto end;
            }
        }
        ret = avformat_write_header(rtmp_ctx, NULL);
        if (ret < 0) {
            printf("Error occurred when opening output URL\n");
            goto end;
        }
    } else {
        avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", args->outfile); //RTMP
        //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP

        if (!ofmt_ctx) {
            printf("Could not create output context\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        ofmt = ofmt_ctx->oformat;
        for (i = 0; i < ifmt_ctx->nb_streams; i++) {
            AVStream *in_stream = ifmt_ctx->streams[i];
            AVStream *out_stream =
                avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            ret =
                avcodec_copy_context(out_stream->codec, in_stream->codec);
            if (ret < 0) {
                printf
                    ("Failed to copy context from input to output stream codec context\n");
                goto end;
            }
            out_stream->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
        //Dump Format------------------
        av_dump_format(ofmt_ctx, 0, args->outfile, 1);
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            ret = avio_open(&ofmt_ctx->pb, args->outfile, AVIO_FLAG_WRITE);
            if (ret < 0) {
                printf("Could not open output URL '%s'", args->outfile);
                goto end;
            }
        }
    }

    start_time = av_gettime();

    //av_dump_format(ofmt_ctx, 0, "whatever", 1);
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Error occurred when opening output file\n");
        return ret;
    }

    i = 0;
    /* read all packets */
    while (1) {
        i++;
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        if (1 == isusedesktop) {
            stream_index = packet.stream_index;
            if (stream_index != 0)
                continue;
            type =
                ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
            av_log(NULL, AV_LOG_DEBUG,
                   "Demuxer gave frame of stream_index %u\n",
                   stream_index);

            av_log(NULL, AV_LOG_DEBUG, "Going to reencode the frame\n");
            frame = av_frame_alloc();

            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            packet.dts = av_rescale_q_rnd(packet.dts,
                                          ifmt_ctx->streams[stream_index]->
                                          time_base,
                                          ifmt_ctx->streams[stream_index]->
                                          codec->time_base,
                                          (enum
                                           AVRounding) (AV_ROUND_NEAR_INF |
                                                        AV_ROUND_PASS_MINMAX));
            packet.pts =
                av_rescale_q_rnd(packet.pts,
                                 ifmt_ctx->streams[stream_index]->
                                 time_base,
                                 ifmt_ctx->streams[stream_index]->codec->
                                 time_base,
                                 (enum AVRounding) (AV_ROUND_NEAR_INF |
                                                    AV_ROUND_PASS_MINMAX));
            ret =
                avcodec_decode_video2(ifmt_ctx->streams[stream_index]->
                                      codec, frame, &got_frame, &packet);
            printf("Decode 1 Packet\tsize:%d\tpts:%d\n", packet.size,
                   packet.pts);

            if (ret < 0) {
                av_frame_free(&frame);
                av_frame_free(&pFrameYUV);
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }


            sws_scale(img_convert_ctx,
                      (const unsigned char *const *) frame->data,
                      frame->linesize, 0, dec_ctx->height, pFrameYUV->data,
                      pFrameYUV->linesize);
        } else {
            got_frame = 1;
            enc_pkt = packet;
            rtmp_ctx = ofmt_ctx;
            stream_index = videoindex;
        }
        if (got_frame) {
            if (1 == isusedesktop) {
                pFrameYUV->pts =
                    av_frame_get_best_effort_timestamp(pFrameYUV);
                pFrameYUV->pict_type = AV_PICTURE_TYPE_NONE;

                enc_pkt.data = NULL;
                enc_pkt.size = 0;
                av_init_packet(&enc_pkt);
                ret =
                    avcodec_encode_video2(ofmt_ctx->streams[stream_index]->
                                          codec, &enc_pkt, pFrameYUV,
                                          &enc_got_frame);
                printf("Encode 1 Packet\tsize:%d\tpts:%d\n", enc_pkt.size,
                       enc_pkt.pts);

                av_frame_free(&frame);
                //av_frame_free(&pFrameYUV);
                if (ret < 0)
                    goto end;
                if (!enc_got_frame)
                    continue;
            }
            //
            //Simple Write PTS
            if (enc_pkt.pts == AV_NOPTS_VALUE) {
                //Write PTS
                AVRational time_base1 =
                    ifmt_ctx->streams[stream_index]->time_base;
                //Duration between 2 frames (us)
                int64_t calc_duration =
                    (double) AV_TIME_BASE /
                    av_q2d(ifmt_ctx->streams[stream_index]->r_frame_rate);
                //Parameters
                enc_pkt.pts =
                    (double) (frame_index * calc_duration) /
                    (double) (av_q2d(time_base1) * AV_TIME_BASE);
                enc_pkt.dts = enc_pkt.pts;
                enc_pkt.duration =
                    (double) calc_duration / (double) (av_q2d(time_base1) *
                                                       AV_TIME_BASE);
            }
            //Important:Delay
            if (enc_pkt.stream_index == stream_index) {
                AVRational time_base =
                    ifmt_ctx->streams[stream_index]->time_base;
                AVRational time_base_q = { 1, AV_TIME_BASE };
                int64_t pts_time =
                    av_rescale_q(enc_pkt.dts, time_base, time_base_q);
                int64_t now_time = av_gettime() - start_time;
                if (pts_time > now_time)
                    av_usleep(pts_time - now_time);

            }
            AVStream *in_stream = ifmt_ctx->streams[stream_index];
            AVStream *ostream = ofmt_ctx->streams[stream_index];
            /* copy packet */
            enc_pkt.dts =
                av_rescale_q_rnd(enc_pkt.dts, in_stream->codec->time_base,
                                 ostream->time_base,
                                 (enum AVRounding) (AV_ROUND_NEAR_INF |
                                                    AV_ROUND_PASS_MINMAX));
            enc_pkt.pts =
                av_rescale_q_rnd(enc_pkt.pts, in_stream->codec->time_base,
                                 ostream->time_base,
                                 (enum AVRounding) (AV_ROUND_NEAR_INF |
                                                    AV_ROUND_PASS_MINMAX));
            enc_pkt.duration =
                av_rescale_q(enc_pkt.duration, in_stream->codec->time_base,
                             ostream->time_base);

            enc_pkt.pos = -1;
            //Print to Screen
            if (enc_pkt.stream_index == stream_index) {
                printf("Send %8d video frames to output URL\n",
                       frame_index);
                frame_index++;
            }
            //ret = av_write_frame(ofmt_ctx, &enc_pkt);
            ret = av_interleaved_write_frame(rtmp_ctx, &enc_pkt);
            if (ret < 0) {
                printf("Error muxing packet\n");
                break;
            }
            //av_free_packet(&enc_pkt);

            if (ret < 0)
                goto end;
        } else {
            if (1 == isusedesktop) {
                av_frame_free(&frame);
            }
            //av_frame_free(&pFrameYUV);
        }
        if (1 == isusedesktop) {
            av_free_packet(&packet);
        } else {
            av_free_packet(&enc_pkt);
        }
    }

    /* flush encoders */
    for (i = 0; i < 1; i++) {
        /* flush encoder */
        ret = flush_encoder1(ofmt_ctx, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }
    av_write_trailer(ofmt_ctx);
  end:
    if (1 == isusedesktop) {
        av_freep(avio_out);
        av_free(outbuffer);
        av_frame_free(&frame);
        av_frame_free(&pFrameYUV);
    }
    av_free_packet(&packet);
    avformat_close_input(&ifmt_ctx);
    avformat_free_context(ofmt_ctx);

    fcloseall();

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred\n");
    return (ret ? 1 : 0);
}

#else
int push(cmdArgsPtr args)
{
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int videoindex = -1;
    int frame_index = 0;
    int64_t start_time = 0;

    in_filename = args->infile;
    out_filename = args->outfile;

    av_register_all();
    //Network
    avformat_network_init();
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename); //RTMP
    //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP

    if (!ofmt_ctx) {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream =
            avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        if (!out_stream) {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            printf
                ("Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    //Dump Format------------------
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("Could not open output URL '%s'", out_filename);
            goto end;
        }
    }
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        printf("Error occurred when opening output URL\n");
        goto end;
    }

    start_time = av_gettime();
    while (1) {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        //Simple Write PTS
        if (pkt.pts == AV_NOPTS_VALUE) {
            //Write PTS
            AVRational time_base1 =
                ifmt_ctx->streams[videoindex]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration =
                (double) AV_TIME_BASE /
                av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
            //Parameters
            pkt.pts =
                (double) (frame_index * calc_duration) /
                (double) (av_q2d(time_base1) * AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration =
                (double) calc_duration / (double) (av_q2d(time_base1) *
                                                   AV_TIME_BASE);
        }
        //Important:Delay
        if (pkt.stream_index == videoindex) {
            AVRational time_base =
                ifmt_ctx->streams[videoindex]->time_base;
            AVRational time_base_q = { 1, AV_TIME_BASE };
            int64_t pts_time =
                av_rescale_q(pkt.dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

        }

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        /* copy packet */
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
        //Print to Screen
        if (pkt.stream_index == videoindex) {
            printf("Send %8d video frames to output URL\n", frame_index);
            frame_index++;
        }
        //ret = av_write_frame(ofmt_ctx, &pkt);
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

        if (ret < 0) {
            printf("Error muxing packet\n");
            break;
        }

        av_free_packet(&pkt);

    }
    av_write_trailer(ofmt_ctx);
  end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}
#endif
