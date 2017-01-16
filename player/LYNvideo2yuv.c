#include <stdio.h>

#include "LYNtype.h"

void SaveFrame(AVFrame * pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int y;

    // Open file
    sprintf(szFilename, "frame%d.ppm", iFrame);
    pFile = fopen(szFilename, "wb");
    if (pFile == NULL)
        return;

    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for (y = 0; y < height; y++)
        fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3,
               pFile);

    // Close file
    fclose(pFile);
}

int open_in_file(inFilePtr in, cmdArgsPtr args)
{
    int i;

    if (avformat_open_input(&in->pFormatCtx, args->infile, NULL, NULL) !=
        0)
        return -1;              // Couldn't open file

    // Retrieve stream information
    if (avformat_find_stream_info(in->pFormatCtx, NULL) < 0)
        return -1;              // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(in->pFormatCtx, 0, args->infile, 0);

    // Find the first video stream
    in->videoStream = -1;
    for (i = 0; i < in->pFormatCtx->nb_streams; i++)
        if (in->pFormatCtx->streams[i]->codec->codec_type ==
            AVMEDIA_TYPE_VIDEO) {
            in->videoStream = i;
            break;
        }
    if (in->videoStream == -1)
        return -1;              // Didn't find a video stream

    // Get a pointer to the codec context for the video stream
    in->pCodecCtxOrig = in->pFormatCtx->streams[in->videoStream]->codec;
    // Find the decoder for the video stream
    in->pCodec = avcodec_find_decoder(in->pCodecCtxOrig->codec_id);
    if (in->pCodec == NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;              // Codec not found
    }
    // Copy context
    in->pCodecCtx = avcodec_alloc_context3(in->pCodec);
    if (avcodec_copy_context(in->pCodecCtx, in->pCodecCtxOrig) != 0) {
        fprintf(stderr, "Couldn't copy codec context");
        return -1;              // Error copying codec context
    }
    // Open codec
    if (avcodec_open2(in->pCodecCtx, in->pCodec, NULL) < 0)
        return -1;              // Could not open codec

    // Allocate video frame
    in->pFrame = av_frame_alloc();

    // Allocate an AVFrame structure
    in->pFrameTarget = av_frame_alloc();
    if (in->pFrameTarget == NULL)
        return -1;

    // Determine required buffer size and allocate buffer
    in->numBytes =
        avpicture_get_size(AV_PIX_FMT_RGB24, in->pCodecCtx->width,
                           in->pCodecCtx->height);
    in->buffer = (uint8_t *) av_malloc(in->numBytes * sizeof(uint8_t));

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *) in->pFrameTarget, in->buffer,
                   AV_PIX_FMT_RGB24, in->pCodecCtx->width,
                   in->pCodecCtx->height);

    // initialize SWS context for software scaling
    in->sws_ctx = sws_getContext(in->pCodecCtx->width,
                                 in->pCodecCtx->height,
                                 in->pCodecCtx->pix_fmt,
                                 in->pCodecCtx->width,
                                 in->pCodecCtx->height,
                                 AV_PIX_FMT_RGB24,
                                 SWS_BILINEAR, NULL, NULL, NULL);
    return 0;
}

int do_decode(cmdArgsPtr args,enum AVPixelFormat format)
{
    inFile infile;
    int i, ret;

    // Register all formats and codecs
    av_register_all();

    memset(&infile, 0, sizeof(inFile));
    infile.format = format;
    if ((ret = open_in_file(&infile, args)) < 0) {
        printf("open in file failed!");
        return ret;
    }
    // Read frames and save first five frames to disk
    i = 0;
    while (av_read_frame(infile.pFormatCtx, &infile.packet) >= 0) {
        // Is this a packet from the video stream?
        if (infile.packet.stream_index == infile.videoStream) {
            // Decode video frame
            avcodec_decode_video2(infile.pCodecCtx, infile.pFrame,
                                  &infile.frameFinished, &infile.packet);

            // Did we get a video frame?
            if (infile.frameFinished) {
                // Convert the image from its native format to RGB
                sws_scale(infile.sws_ctx,
                          (uint8_t const *const *) infile.pFrame->data,
                          infile.pFrame->linesize, 0,
                          infile.pCodecCtx->height,
                          infile.pFrameTarget->data,
                          infile.pFrameTarget->linesize);

                // Save the frame to disk
                if (++i <= 5)
                    SaveFrame(infile.pFrameTarget, infile.pCodecCtx->width,
                              infile.pCodecCtx->height, i);
            }
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&infile.packet);
    }

    // Free the RGB image
    av_free(infile.buffer);
    av_frame_free(&infile.pFrameTarget);

    // Free the YUV frame
    av_frame_free(&infile.pFrame);

    // Close the codecs
    avcodec_close(infile.pCodecCtx);
    avcodec_close(infile.pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&infile.pFormatCtx);

    return 0;
}

int video2rgb(cmdArgsPtr args)
{
    return do_decode(args,AV_PIX_FMT_RGB24);
}

int video2yuv(cmdArgsPtr args)
{
    return do_decode(args,AV_PIX_FMT_YUV420P);
}

int video2yuvs(cmdArgsPtr args)
{

    return 0;
}
