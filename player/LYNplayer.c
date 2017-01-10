/* http://dranger.com/ffmpeg/tutorial01.c */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

/* compatibility with newer API */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT( 55, 28, 1 )
#define av_frame_alloc    avcodec_alloc_frame
#define av_frame_free    avcodec_free_frame
#endif

int main(int argc, char *argv[])
{
    /* Initalizing these to NULL prevents segfaults! */
    AVFormatContext *pFormatCtx = NULL;
    int i, videoStream;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVPacket packet;
    int frameFinished;
    struct SwsContext *sws_ctx = NULL;
    float aspect_ratio;
    SDL_Overlay *bmp;
    SDL_Surface *screen;
    SDL_Rect rect;
    SDL_Event event;

    if (argc < 2) {
	printf("Please provide a movie file\n");
	return (-1);
    }
    /* Register all formats and codecs */
    av_register_all();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
	fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
	exit(1);
    }

    /* Open video file */
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
	return (-1);		/* Couldn't open file */

    /* Retrieve stream information */
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	return (-1);		/* Couldn't find stream information */

    /* Dump information about file onto standard error */
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    /* Find the first video stream */
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type ==
	    AVMEDIA_TYPE_VIDEO) {
	    videoStream = i;
	    break;
	}
    if (videoStream == -1)
	return (-1);		/* Didn't find a video stream */

    /* Get a pointer to the codec context for the video stream */
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    /* Find the decoder for the video stream */
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if (pCodec == NULL) {
	fprintf(stderr, "Unsupported codec!\n");
	return (-1);		/* Codec not found */
    }
    /* Copy context */
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
	fprintf(stderr, "Couldn't copy codec context");
	return (-1);		/* Error copying codec context */
    }

    /* Open codec */
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	return (-1);		/* Could not open codec */

    /* Allocate video frame */
    pFrame = av_frame_alloc();

    // Make a screen to put our video
#ifndef __DARWIN__
    screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
    screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
    if (!screen) {
	fprintf(stderr, "SDL: could not set video mode - exiting\n");
	exit(1);
    }
    // Allocate a place to put our YUV image on that screen
    bmp = SDL_CreateYUVOverlay(pCodecCtx->width,
			       pCodecCtx->height,
			       SDL_YV12_OVERLAY, screen);

    /* initialize SWS context for software scaling */
    sws_ctx = sws_getContext(pCodecCtx->width,
			     pCodecCtx->height,
			     pCodecCtx->pix_fmt,
			     pCodecCtx->width,
			     pCodecCtx->height,
			     AV_PIX_FMT_YUV420P,
			     SWS_BILINEAR, NULL, NULL, NULL);

    /* Read frames and save first five frames to disk */
    i = 0;
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
	/* Is this a packet from the video stream? */
	if (packet.stream_index == videoStream) {
	    /* Decode video frame */
	    avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
				  &packet);

	    /* Did we get a video frame? */
	    if (frameFinished) {
		SDL_LockYUVOverlay(bmp);

		AVPicture pict;
		pict.data[0] = bmp->pixels[0];
		pict.data[1] = bmp->pixels[2];
		pict.data[2] = bmp->pixels[1];

		pict.linesize[0] = bmp->pitches[0];
		pict.linesize[1] = bmp->pitches[2];
		pict.linesize[2] = bmp->pitches[1];

		/* Convert the image from its native format to RGB */
		sws_scale(sws_ctx, (uint8_t const *const *) pFrame->data,
			  pFrame->linesize, 0, pCodecCtx->height,
			  pict.data, pict.linesize);

		SDL_UnlockYUVOverlay(bmp);

		rect.x = 0;
		rect.y = 0;
		rect.w = pCodecCtx->width;
		rect.h = pCodecCtx->height;
		SDL_DisplayYUVOverlay(bmp, &rect);
	    }
	}

	/* Free the packet that was allocated by av_read_frame */
	av_free_packet(&packet);
	SDL_PollEvent(&event);
	switch (event.type) {
	case SDL_QUIT:
	    SDL_Quit();
	    exit(0);
	    break;
	default:
	    break;
	}
    }

    /* Free the YUV frame */
    av_frame_free(&pFrame);

    /* Close the codecs */
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    /* Close the video file */
    avformat_close_input(&pFormatCtx);

    return (0);
}
