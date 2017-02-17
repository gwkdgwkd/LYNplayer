#include "LYNtype.h"

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static int audio_decode(cmdArgsPtr args)
{
    AVCodec *codec;
    AVCodecContext *c = NULL;
    int len;
    FILE *f, *outfile;
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;
    AVFrame *decoded_frame = NULL;

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
            /* if a frame has been decoded, output it */
            int data_size = av_get_bytes_per_sample(c->sample_fmt);
            if (data_size < 0) {
                /* This should not occur, checking just for paranoia */
                fprintf(stderr, "Failed to calculate data size\n");
                exit(1);
            }
            for (i = 0; i < decoded_frame->nb_samples; i++)
                for (ch = 0; ch < c->channels; ch++)
                    fwrite(decoded_frame->data[ch] + data_size * i, 1,
                           data_size, outfile);
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
