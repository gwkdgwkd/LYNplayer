#include "tutorial.h"

ANativeWindow* 		window;
jobject				bitmap;

static void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;
    if(av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    pthread_mutex_lock(&q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    pthread_mutex_lock(&q->mutex);

    for(;;) {
        if(global_video_state->quit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

int queue_picture(VideoState *is, AVFrame *pFrame, int frameWidth, int frameHeight, int frameId) {
    VideoPicture *vp;
    /* wait until we have space for a new pic */
    pthread_mutex_lock(&is->pictq_mutex);
    while(is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&	!is->quit) {
        pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
    }
    pthread_mutex_unlock(&is->pictq_mutex);
    if(is->quit)
        return -1;
    // windex is set to 0 initially
    vp = &is->pictq[is->pictq_windex];
    vp->bmp = pFrame;
    vp->width = frameWidth;
    vp->height = frameHeight;
    vp->id = frameId;
    /* now we inform our display thread that we have a pic ready */
    if(++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
        is->pictq_windex = 0;
    }
    pthread_mutex_lock(&is->pictq_mutex);
    is->pictq_size++;
    pthread_cond_signal(&is->pictq_cond);
    pthread_mutex_unlock(&is->pictq_mutex);
    return 0;
}

void videoDecodeThread(void *arg) {
	AVPacket        		packet;
	AVFrame         	    *decodedFrame = NULL;
	int 					i=0;
	int            			frameFinished;
    //struct timeval start;
    //struct timeval end;
    //float time_use=0;
    VideoState *is = (VideoState *)arg;
    decodedFrame = av_frame_alloc();

    while(!is->quit) {
        if(packet_queue_get(&is->videoq, &packet, 1) < 0) {
            // means we quit getting packets
            break;
        }
        //gettimeofday(&start,NULL); //gettimeofday(&start,&tz);结果一样
        // Decode video frame
        avcodec_decode_video2(is->vCodecCtx, decodedFrame, &frameFinished, &packet);
        //gettimeofday(&end,NULL);
        //time_use=(end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);//微秒
        //LOGI("avcodec_decode_video2 time_use is %.10f\n",time_use);
        // Did we get a video frame?
        if(frameFinished && !is->quit) {
            /* save frame after decode to yuv file
            if(i < 20){
                FILE *fp;
                int j;
                fp = fopen("/storage/emulated/0/android-ffmpeg-tutorial02/1.yuv","ab");
                for (j = 0; j<is->vCodecCtx->height; j++) {
                    fwrite(decodedFrame->data[0] + j*decodedFrame->linesize[0], is->vCodecCtx->width, 1, fp);
                }
                for (j = 0; j<is->vCodecCtx->height/2; j++) {
                    fwrite(decodedFrame->data[1] + j*decodedFrame->linesize[1], is->vCodecCtx->width/2, 1, fp);
                }
                for (j = 0; j<is->vCodecCtx->height/2; j++){
                    fwrite(decodedFrame->data[2] + j*decodedFrame->linesize[2], is->vCodecCtx->width/2, 1, fp);
                }
                fclose(fp);
            }//*/
            // Convert the image from its native format to RGBA
            //gettimeofday(&start,NULL);
            pthread_mutex_lock(&is->decode_mutex);
#if USE_SWS_CTX
            sws_scale
				(
					is->sws_ctx,
					(uint8_t const * const *)decodedFrame->data,
					decodedFrame->linesize,
					0,
					is->vCodecCtx->height,
					is->frameRGBA->data,
					is->frameRGBA->linesize
				);
#else
            I420Scale(decodedFrame->data[0],decodedFrame->linesize[0],
						  decodedFrame->data[1],decodedFrame->linesize[1],
						  decodedFrame->data[2],decodedFrame->linesize[2],
						  is->vCodecCtx->width,is->vCodecCtx->height,
						  is->scaleFrame->data[0],is->scaleFrame->linesize[0],
						  is->scaleFrame->data[1],is->scaleFrame->linesize[1],
						  is->scaleFrame->data[2],is->scaleFrame->linesize[2],
						  is->width,is->height,kFilterNone);
            //why not I420ToRGBA？ ijkplayer uses I420ToABGR
            I420ToABGR(is->scaleFrame->data[0],is->scaleFrame->linesize[0],
						  is->scaleFrame->data[1],is->scaleFrame->linesize[1],
						  is->scaleFrame->data[2],is->scaleFrame->linesize[2],
						  is->frameRGBA->data[0],is->frameRGBA->linesize[0],
						  is->width,is->height);
#endif
            //gettimeofday(&end,NULL);
            //time_use=(end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);//微秒
            //LOGI("I420Scale and I420ToABGR use time : %.10f\n",time_use);
            /* save frame after scale to rgba file
            if(i < 20){
                FILE *fp;
                fp = fopen("/storage/emulated/0/android-ffmpeg-tutorial02/1.rgba","ab");
                for (int j = 0; j < is->height; j++) {
                    fwrite(is->frameRGBA->data[0] + j*is->frameRGBA->linesize[0], is->width*4, 1, fp);
                }
                fclose(fp);
            }//*/
            if(queue_picture(is, is->frameRGBA,is->width,is->height,i) < 0) {
                break;
            }
            pthread_mutex_unlock(&is->decode_mutex);
            i++;
            av_free_packet(&packet);
        }
	}
	av_free(decodedFrame);
	LOGI("total No. of frames decoded %d", i);
}

void finish(JNIEnv *pEnv,VideoState *is) {
	pthread_mutex_destroy(&is->mutex);
	pthread_cond_destroy(&is->cond);
	if(is->is_play) {
	    //unlock the bitmap
	    AndroidBitmap_unlockPixels(pEnv, bitmap);
	}
	//av_free(buffer);  //why ?
	av_free(is->scalebuffer);
	// Free the RGB image
	av_free(is->frameRGBA);
	// Free the YUV frame
	av_free(is->scaleFrame);
	// Close the codec
	avcodec_close(is->vCodecCtx);
	avcodec_close(is->aCodecCtx);
	// Close the video file
	avformat_close_input(&is->pFormatCtx);
	// shut down the native audio system
	shutdown();
}

void videoDisplayThread(JNIEnv *pEnv) {
	ANativeWindow_Buffer 	windowBuffer;
	VideoPicture *vp;
    int i = 0;
    VideoState *is = global_video_state;

    while(!is->quit) {
        /* wait until we have a pic */
        pthread_mutex_lock(&is->pictq_mutex);
        while(is->pictq_size < 1 && !is->quit) {
            pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
        }
        pthread_mutex_unlock(&is->pictq_mutex);
        vp = &is->pictq[is->pictq_rindex];
        if(!vp->bmp) {
            LOGE("picture is NULL");
            break;
        }
        pthread_mutex_lock(&is->display_mutex);
        if(is->quit){
            break;
        }
        // lock the window buffer
        if (ANativeWindow_lock(window, &windowBuffer, NULL) < 0) {
            LOGE("cannot lock window");
        } else {
            float tmp = (float)windowBuffer.width/(float)windowBuffer.height;
            if(tmp - is->ratio > 0.009){
                if(++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
                    is->pictq_rindex = 0;
                }
                LOGD("throw frame %d,because windowBuffer's ratio unreasonable",vp->id);
                pthread_mutex_lock(&is->pictq_mutex);
                is->pictq_size--;
                pthread_cond_signal(&is->pictq_cond);
                pthread_mutex_unlock(&is->pictq_mutex);
                ANativeWindow_unlockAndPost(window);
                pthread_mutex_unlock(&is->display_mutex);
                continue;
            }
            // draw the frame on buffer
            LOGI("copy buffer %d:%d:%d", is->width, is->height, is->width*is->height*4);
            LOGI("window buffer: %d:%d:%d", windowBuffer.width,
                windowBuffer.height, windowBuffer.stride);
            if(is->width == windowBuffer.width && is->height == windowBuffer.height
               && is->width == vp->width && is->height == vp->height){
                //memcpy(windowBuffer.bits, buffer,  is->width * is->height * 4);
                for (int h = 0; h < is->height; h++){
                    memcpy(windowBuffer.bits + h * windowBuffer.stride *4,
                        vp->bmp->data[0] + h * vp->bmp->linesize[0],is->width*4);
                }
            } else {
                LOGD("throw frame %d,because vp's width and height unreasonable",vp->id);
            }
            // unlock the window buffer and post it to display
            ANativeWindow_unlockAndPost(window);
            /* update queue for next picture! */
            if(++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
                is->pictq_rindex = 0;
            }
            pthread_mutex_lock(&is->pictq_mutex);
            is->pictq_size--;
            pthread_cond_signal(&is->pictq_cond);
            pthread_mutex_unlock(&is->pictq_mutex);
            // count number of frames
            ++i;
        }
        pthread_mutex_unlock(&is->display_mutex);
	}
	LOGI("total No. of frames rendered %d", i);
	finish(pEnv,is);
}

int readPacketThread(void *arg) {
    AVCodec         *pVideoCodec = NULL;
    AVCodec         *pAudioCodec = NULL;
    int 			i;
    AVDictionary    *videoOptionsDict = NULL;
    AVDictionary    *audioOptionsDict = NULL;
    AVPacket        packet;

    AVFormatContext *pFormatCtx = NULL;

    VideoState *is = (VideoState *)arg;
    global_video_state = is;
    is->init = 0;
    is->videoStream = -1;

    // Register all formats and codecs
    av_register_all();
    // Open video file
    if(avformat_open_input(&pFormatCtx, is->filename, NULL, NULL)!=0)
        return -1; // Couldn't open file
    is->pFormatCtx = pFormatCtx;
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return -1; // Couldn't find stream information
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, is->filename, 0);
    // Find the first video stream

    for(i = 0; i < pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && is->videoStream < 0) {
            is->videoStream = i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && is->audioStream < 0) {
            is->audioStream = i;
        }
    }
    if(is->videoStream == -1)
        return -1; // Didn't find a video stream
    if(is->audioStream == -1)
        return -1;

    is->aCodecCtx=pFormatCtx->streams[is->audioStream]->codec;
    pAudioCodec = avcodec_find_decoder(is->aCodecCtx->codec_id);
    if(!pAudioCodec) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
    avcodec_open2(is->aCodecCtx, pAudioCodec, &audioOptionsDict);
    packet_queue_init(&is->audioq);
    is->audio_buf_size = 0;
    is->audio_buf_index = 0;
    memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));

    // Get a pointer to the codec context for the video stream
    is->vCodecCtx=pFormatCtx->streams[is->videoStream]->codec;
    // Find the decoder for the video stream
    if(is->vCodecCtx->codec_id == AV_CODEC_ID_H264 ||
       is->vCodecCtx->codec_id == AV_CODEC_ID_HEVC ||
       is->vCodecCtx->codec_id == AV_CODEC_ID_MPEG4 ||
       is->vCodecCtx->codec_id == AV_CODEC_ID_VP8 ||
       is->vCodecCtx->codec_id == AV_CODEC_ID_VP9){
        char codec_name[64] = {0};
        sprintf(codec_name,"%s_mediacodec",avcodec_get_name(is->vCodecCtx->codec_id));
        pVideoCodec=avcodec_find_decoder_by_name(codec_name);
    }else{
        pVideoCodec=avcodec_find_decoder(is->vCodecCtx->codec_id);
    }
    if(pVideoCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
    is->ratio = (float)is->vCodecCtx->width / (float)is->vCodecCtx->height;
    is->init = 1;
    // Open codec
    if(avcodec_open2(is->vCodecCtx, pVideoCodec, &videoOptionsDict)<0)
        return -1; // Could not open codec
    packet_queue_init(&is->videoq);
    is->scaleFrame=av_frame_alloc();
    // Allocate an AVFrame structure
    is->frameRGBA=av_frame_alloc();
    if(is->frameRGBA==NULL)
        return -1;

    is->init = 2;
    pthread_mutex_lock(&is->mutex);
    while(!is->is_play){
        pthread_cond_wait(&is->cond, &is->mutex);
    }
    pthread_mutex_unlock(&is->mutex);
    pthread_create(&is->video_decode_tid, NULL, videoDecodeThread, is);
    pthread_create(&is->video_display_tid, NULL, videoDisplayThread, NULL);

    while(av_read_frame(pFormatCtx, &packet)>=0 && !is->quit) {
        // Is this a packet from the video stream?
        if(packet.stream_index == is->videoStream) {
            packet_queue_put(&is->videoq, &packet);
        } else if (packet.stream_index == is->audioStream) {
            packet_queue_put(&is->audioq, &packet);
        } else {
            // Free the packet that was allocated by av_read_frame
            av_free_packet(&packet);
        }
    }
    return 0;
}

jint naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
    VideoState      *is;
    char *videoFileName;
    is = av_mallocz(sizeof(VideoState));
    videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    LOGI("video file name is %s", videoFileName);
    av_strlcpy(is->filename, videoFileName, 1024);
    pthread_mutex_init(&is->pictq_mutex, NULL);
    pthread_cond_init(&is->pictq_cond, NULL);
    pthread_mutex_init(&is->display_mutex, NULL);
    pthread_mutex_init(&is->decode_mutex, NULL);
    pthread_mutex_init(&is->mutex, NULL);
    pthread_cond_init(&is->cond, NULL);
    pthread_create(&is->read_packet_tid, NULL, readPacketThread, is);
}

jobject createBitmap(JNIEnv *pEnv, int pWidth, int pHeight) {
	int i;
	//get Bitmap class and createBitmap method ID
	jclass javaBitmapClass = (jclass)(*pEnv)->FindClass(pEnv, "android/graphics/Bitmap");
	jmethodID mid = (*pEnv)->GetStaticMethodID(pEnv, javaBitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
	//create Bitmap.Config
	//reference: https://forums.oracle.com/thread/1548728
	const wchar_t* configName = L"ARGB_8888";
	int len = wcslen(configName);
	jstring jConfigName;
	if (sizeof(wchar_t) != sizeof(jchar)) {
		//wchar_t is defined as different length than jchar(2 bytes)
		jchar* str = (jchar*)malloc((len+1)*sizeof(jchar));
		for (i = 0; i < len; ++i) {
			str[i] = (jchar)configName[i];
		}
		str[len] = 0;
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)str, len);
	} else {
		//wchar_t is defined same length as jchar(2 bytes)
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)configName, len);
	}
	jclass bitmapConfigClass = (*pEnv)->FindClass(pEnv, "android/graphics/Bitmap$Config");
	jobject javaBitmapConfig = (*pEnv)->CallStaticObjectMethod(pEnv, bitmapConfigClass,
			(*pEnv)->GetStaticMethodID(pEnv, bitmapConfigClass, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;"), jConfigName);
	//create the bitmap
	return (*pEnv)->CallStaticObjectMethod(pEnv, javaBitmapClass, mid, pWidth, pHeight, javaBitmapConfig);
}

jintArray naGetVideoRes(JNIEnv *pEnv, jobject pObj) {
    jintArray lRes;
    VideoState *is = global_video_state;
    while(is->init < 1){
        usleep(100);
    }
	if (NULL == is->vCodecCtx) {
		return NULL;
	}
	lRes = (*pEnv)->NewIntArray(pEnv, 2);
	if (lRes == NULL) {
		LOGI(1, "cannot allocate memory for video size");
		return NULL;
	}
	jint lVideoRes[2];
	lVideoRes[0] = is->vCodecCtx->width;
	lVideoRes[1] = is->vCodecCtx->height;
	(*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
	return lRes;
}

jintArray naGetAudioInfo(JNIEnv *pEnv, jobject pObj) {
    jintArray lInfo;
    VideoState *is = global_video_state;
    while(is->init < 1){
        usleep(100);
    }
	if (NULL == is->aCodecCtx) {
		return NULL;
	}
	lInfo = (*pEnv)->NewIntArray(pEnv, 3);
	if (lInfo == NULL) {
		LOGI(1, "cannot allocate memory for audio info");
		return NULL;
	}
	jint lAudioInfo[3];
	lAudioInfo[0] = is->aCodecCtx->sample_rate;
	lAudioInfo[1] = is->aCodecCtx->channels;
	lAudioInfo[2] = is->aCodecCtx->sample_fmt;
	(*pEnv)->SetIntArrayRegion(pEnv, lInfo, 0, 3, lAudioInfo);
	return lInfo;
}

int naSetup(JNIEnv *pEnv, jobject pObj, jobject pSurface,int pWidth,int pHeight) {
    int scalebuffersize;
    VideoState *is = global_video_state;

    while(is->init < 2){
        usleep(100);
    }
	pthread_mutex_lock(&is->display_mutex);
	pthread_mutex_lock(&is->decode_mutex);

	if (0 != pSurface) {
        if(is->is_play){
            ANativeWindow_release(window);
        }
		// get the native window reference
		window = ANativeWindow_fromSurface(pEnv, pSurface);
		// set format and size of window buffer
		ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
	} else {
		// release the native window
		ANativeWindow_release(window);
	}
    if(pWidth == 0 || pHeight == 0) {
        if(!is->is_play) {
            finish(pEnv,is);
        } else {
            is->quit = 1;
        };
        pthread_mutex_unlock(&is->decode_mutex);
        pthread_mutex_unlock(&is->display_mutex);
        return -1;
    }
	is->width = pWidth;
    is->height = pHeight;

    //create a bitmap as the buffer for frameRGBA
    bitmap = createBitmap(pEnv, pWidth, pHeight);
    if (AndroidBitmap_lockPixels(pEnv, bitmap, &is->buffer) < 0){
        pthread_mutex_unlock(&is->decode_mutex);
        pthread_mutex_unlock(&is->display_mutex);
        return -1;
    }
    #if USE_SWS_CTX //use libyuv
    //get the scaling context
    is->sws_ctx = sws_getContext (
            is->vCodecCtx->width,
            is->vCodecCtx->height,
            is->vCodecCtx->pix_fmt,
            pWidth,
            pHeight,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
    );
    #endif
    // Assign appropriate parts of bitmap to image planes in pFrameRGBA
    // Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)is->frameRGBA, is->buffer, AV_PIX_FMT_RGBA,
            pWidth, pHeight);
    scalebuffersize = avpicture_get_size(AV_PIX_FMT_YUV420P, pWidth, pHeight);
    if(is->scalebuffer != NULL){
        free(is->scalebuffer);
    }
    is->scalebuffer = (uint8_t *) av_malloc(scalebuffersize * sizeof(uint8_t));
    avpicture_fill((AVPicture *)is->scaleFrame, is->scalebuffer, AV_PIX_FMT_YUV420P, pWidth, pHeight);

    pthread_mutex_unlock(&is->decode_mutex);
    pthread_mutex_unlock(&is->display_mutex);
    return 0;
}

static int audio_decode_frame(VideoState *is) {
    AVPacket *pkt = &is->audio_pkt;
    AVFrame *frame = &is->audio_frame;
    uint8_t *buf = is->audio_buf;

    int len1, data_size = 0;
    struct SwrContext *swr_ctx = NULL;
    int resampled_data_size;

    for(;;) {
        while(is->audio_pkt_size > 0) {
            int got_frame = 0;
            len1 = avcodec_decode_audio4(is->aCodecCtx, frame, &got_frame, pkt);
            if(len1 < 0) {
                /* if error, skip frame */
                is->audio_pkt_size = 0;
                break;
            }
            is->audio_pkt_data += len1;
            is->audio_pkt_size -= len1;
            if (got_frame) {
                data_size =
                    av_samples_get_buffer_size
                    (
                        NULL,
                        is->aCodecCtx->channels,
                        frame->nb_samples,
                        is->aCodecCtx->sample_fmt,
                        1
                    );
                if (frame->channels > 0 && frame->channel_layout == 0){
                    frame->channel_layout = av_get_default_channel_layout(frame->channels);
                } else if (frame->channels == 0 && frame->channel_layout > 0) {
                    frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
                }
                /*
                LOGI("frame->sample_rate = %d \n", frame->sample_rate);
                LOGI("frame->format = %d \n", frame->format);
                LOGI("frame->format bits = %d \n", av_get_bytes_per_sample(frame->format));
                LOGI("frame->channels = %d \n", frame->channels);
                LOGI("frame->channel_layout = %d \n", frame->channel_layout);
                LOGI("frame->nb_samples = %d \n", frame->nb_samples);
                //*/
                if(frame->format != AV_SAMPLE_FMT_S16
                    || frame->channel_layout != is->aCodecCtx->channel_layout
                    || frame->sample_rate != is->aCodecCtx->sample_rate
                    || frame->nb_samples != 1024) {
                    if (swr_ctx != NULL) {
                        swr_free(&swr_ctx);
                        swr_ctx = NULL;
                    }
                    swr_ctx = swr_alloc_set_opts(NULL, frame->channel_layout,
                        AV_SAMPLE_FMT_S16, frame->sample_rate,frame->channel_layout,
                        (enum AVSampleFormat)frame->format, frame->sample_rate, 0, NULL);
                    if (swr_ctx == NULL || swr_init(swr_ctx) < 0) {
                        LOGE("swr_init failed!!!" );
                        break;
                    }
                }
                if(swr_ctx) {
                    int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                                                        frame->sample_rate, AV_SAMPLE_FMT_S16, AV_ROUND_INF);
                    //LOGI("swr convert ! \n");
                    //LOGI("dst_nb_samples : %d \n", dst_nb_samples);
                    //LOGI("data_size : %d \n", data_size);
                    int len2 = swr_convert(swr_ctx, &buf, dst_nb_samples,(const uint8_t**)frame->data, frame->nb_samples);
                    if (len2 < 0) {
                        LOGE("swr_convert failed \n" );
                        break;
                    }
                    resampled_data_size = frame->channels * len2 * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                }else{
                    resampled_data_size = data_size;
                }
            }
            if(data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return resampled_data_size;
        }
        if(pkt->data) {
            av_free_packet(pkt);
        }
        if(is->quit) {
            return -1;
        }
        if(packet_queue_get(&is->audioq, pkt, 1) < 0) {
            return -1;
        }
        is->audio_pkt_data = pkt->data;
        is->audio_pkt_size = pkt->size;
    }
}

jint naGetPcmBuffer(JNIEnv *pEnv, jobject pObj, jbyteArray buffer, jint len) {
    jbyte *pcm = (*pEnv)->GetByteArrayElements(pEnv, buffer, NULL);
    jsize pcmsize = (*pEnv)->GetArrayLength(pEnv, buffer);
    jbyte *pcmindex = pcm;

    int len1, audio_size;
    VideoState *is = global_video_state;

    while(len > 0) {
        if(is->audio_buf_index >= is->audio_buf_size) {
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(is);
            if(audio_size < 0) {
	            /* If error, output silence */
	            is->audio_buf_size = 1024; // arbitrary?
	            memset(is->audio_buf, 0, is->audio_buf_size);
            } else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if(len1 > len)
            len1 = len;
        memcpy(pcmindex, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        pcmindex += len1;
        is->audio_buf_index += len1;
    }
    (*pEnv)->ReleaseByteArrayElements(pEnv, buffer, pcm, 0);
    return pcmindex - pcm;
}

int getPcm(void **pcm, size_t *pcmSize) {
    VideoState *is = global_video_state;
    *pcmSize = audio_decode_frame(is);
    *pcm = is->audio_buf;
}

/**
 * start the video playback
 */
void naPlay(JNIEnv *pEnv, jobject pObj) {
    VideoState *is = global_video_state;

    pthread_mutex_lock(&is->mutex);
    is->is_play = 1;
    pthread_cond_signal(&is->cond);
    pthread_mutex_unlock(&is->mutex);

	#ifndef USE_AUDIO_TRACK
	pthread_t audioPlayThread;
	static struct audioArgs audio_args;
	memset(&audio_args,0,sizeof(struct audioArgs));
    audio_args.rate = is->aCodecCtx->sample_rate;
    audio_args.channels = is->aCodecCtx->channels;
    audio_args.pcm_callback = getPcm;
	pthread_create(&audioPlayThread, NULL, audioplay, (void *)&audio_args);
	#endif
}

/**
 * stop the video playback
 */
void naStop(JNIEnv *pEnv, jobject pObj) {
	global_video_state->quit = 1;
}

jint JNI_OnLoad(JavaVM* pVm, void* reserved) {
	JNIEnv* env;
	if ((*pVm)->GetEnv(pVm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		 return -1;
	}

	int index = 0;
	int nmMax = 8;
	JNINativeMethod nm[nmMax];
	nm[index].name = "naInit";
	nm[index].signature = "(Ljava/lang/String;)I";
	nm[index++].fnPtr = (void*)naInit;

	nm[index].name = "naSetup";
	nm[index].signature = "(Landroid/view/Surface;II)I";
	nm[index++].fnPtr = (void*)naSetup;

	nm[index].name = "naGetVideoRes";
	nm[index].signature = "()[I";
	nm[index++].fnPtr = (void*)naGetVideoRes;

	nm[index].name = "naGetAudioInfo";
	nm[index].signature = "()[I";
	nm[index++].fnPtr = (void*)naGetAudioInfo;

	nm[index].name = "naPlay";
	nm[index].signature = "()V";
	nm[index++].fnPtr = (void*)naPlay;

	nm[index].name = "naStop";
	nm[index].signature = "()V";
	nm[index++].fnPtr = (void*)naStop;

	nm[index].name = "naGetPcmBuffer";
	nm[index].signature = "([BI)I";
	nm[index++].fnPtr = (void*)naGetPcmBuffer;

	jclass cls = (*env)->FindClass(env, "lyn/android_ffmpeg/tutorial/MainActivity");
	//Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, cls, nm, index);
	av_jni_set_java_vm(pVm, NULL);
	return JNI_VERSION_1_6;
}

