// tutorial02.c
// A pedagogical video player that will stream through every video frame as fast as it can.
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard,
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
//
// The code is modified so that it can be compiled to a shared library and run on Android
//
// The code play the video stream on your screen
//
// Feipeng Liu (http://www.roman10.net/)
// Aug 2013


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include "libyuv.h"

#include <stdio.h>
#include <pthread.h>

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <sys/time.h>

#define LOG_TAG "android-ffmpeg-tutorial02"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);
#define MAX_AUDIO_FRAME_SIZE 192000

ANativeWindow* 		window;
char 				*videoFileName;
AVFormatContext 	*formatCtx = NULL;
int 				videoStream,audioStream;
AVCodecContext  	*vCodecCtx = NULL;
AVCodecContext  	*aCodecCtx = NULL;
AVFrame         	*decodedFrame = NULL;
AVFrame         	*scaleFrame = NULL;
AVFrame         	*frameRGBA = NULL;
jobject				bitmap;
void*				buffer;
void*				scalebuffer;
struct SwsContext   *sws_ctx = NULL;
int 				width;
int 				height;
int					stop = -1;
int 				scalebuffersize;
pthread_mutex_t     mutex;

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} PacketQueue;

PacketQueue audioq;
int quit = 0;

static void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    pthread_mutex_init(q->mutex, NULL);
    pthread_cond_init(q->cond, NULL);
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

    pthread_mutex_lock(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    pthread_cond_signal(q->cond);

    pthread_mutex_unlock(q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    pthread_mutex_lock(q->mutex);

    for(;;) {
        if(quit) {
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
            pthread_cond_wait(q->cond, q->mutex);
        }
    }
    pthread_mutex_unlock(q->mutex);
    return ret;
}

jint naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
    AVCodec         *pVideoCodec = NULL;
    AVCodec         *pAudioCodec = NULL;
    int 			i;
    AVDictionary    *videoOptionsDict = NULL;
    AVDictionary    *audioOptionsDict = NULL;

    videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    LOGI("video file name is %s", videoFileName);
    // Register all formats and codecs
    av_register_all();
    // Open video file
    if(avformat_open_input(&formatCtx, videoFileName, NULL, NULL)!=0)
        return -1; // Couldn't open file
    // Retrieve stream information
    if(avformat_find_stream_info(formatCtx, NULL)<0)
        return -1; // Couldn't find stream information
    // Dump information about file onto standard error
    av_dump_format(formatCtx, 0, videoFileName, 0);
    // Find the first video stream
    videoStream=-1;
    audioStream=-1;
    for(i=0; i<formatCtx->nb_streams; i++) {
        if(formatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO && videoStream < 0) {
            videoStream=i;
        }
        if(formatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO && audioStream < 0) {
            audioStream=i;
        }
    }
    if(videoStream==-1)
        return -1; // Didn't find a video stream
    if(audioStream==-1)
        return -1;

    aCodecCtx=formatCtx->streams[audioStream]->codec;
    pAudioCodec = avcodec_find_decoder(aCodecCtx->codec_id);
    if(!pAudioCodec) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
    avcodec_open2(aCodecCtx, pAudioCodec, &audioOptionsDict);
    packet_queue_init(&audioq);

    // Get a pointer to the codec context for the video stream
    vCodecCtx=formatCtx->streams[videoStream]->codec;
    // Find the decoder for the video stream
    //pCodec=avcodec_find_decoder(vCodecCtx->codec_id);
    pVideoCodec=avcodec_find_decoder_by_name("h264_mediacodec");
    if(pVideoCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
    // Open codec
    if(avcodec_open2(vCodecCtx, pVideoCodec, &videoOptionsDict)<0)
        return -1; // Could not open codec
    // Allocate video frame
    decodedFrame=av_frame_alloc();
    scaleFrame=av_frame_alloc();
    // Allocate an AVFrame structure
    frameRGBA=av_frame_alloc();
    if(frameRGBA==NULL)
        return -1;
    pthread_mutex_init(&mutex, NULL);
    return 0;
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
	if (NULL == vCodecCtx) {
		return NULL;
	}
	lRes = (*pEnv)->NewIntArray(pEnv, 2);
	if (lRes == NULL) {
		LOGI(1, "cannot allocate memory for video size");
		return NULL;
	}
	jint lVideoRes[2];
	lVideoRes[0] = vCodecCtx->width;
	lVideoRes[1] = vCodecCtx->height;
	(*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
	return lRes;
}

jintArray naGetAudioInfo(JNIEnv *pEnv, jobject pObj) {
    jintArray lInfo;
	if (NULL == aCodecCtx) {
		return NULL;
	}
	lInfo = (*pEnv)->NewIntArray(pEnv, 3);
	if (lInfo == NULL) {
		LOGI(1, "cannot allocate memory for audio info");
		return NULL;
	}
	jint lAudioInfo[3];
	lAudioInfo[0] = aCodecCtx->sample_rate;
	lAudioInfo[1] = aCodecCtx->channels;
	lAudioInfo[2] = aCodecCtx->sample_fmt;
	(*pEnv)->SetIntArrayRegion(pEnv, lInfo, 0, 3, lAudioInfo);
	return lInfo;
}

void naSetSurface(JNIEnv *pEnv, jobject pObj, jobject pSurface) {
	pthread_mutex_lock(&mutex);
	if (0 != pSurface) {
        if(stop == 0){
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
}

jint naSetup(JNIEnv *pEnv, jobject pObj, int pWidth, int pHeight) {
	width = pWidth;
	height = pHeight;

	//create a bitmap as the buffer for frameRGBA
	bitmap = createBitmap(pEnv, pWidth, pHeight);
	if (AndroidBitmap_lockPixels(pEnv, bitmap, &buffer) < 0)
		return -1;
	//get the scaling context
	sws_ctx = sws_getContext (
	        vCodecCtx->width,
	        vCodecCtx->height,
	        vCodecCtx->pix_fmt,
	        pWidth,
	        pHeight,
	        AV_PIX_FMT_RGBA,
	        SWS_BILINEAR,
	        NULL,
	        NULL,
	        NULL
	);
	// Assign appropriate parts of bitmap to image planes in pFrameRGBA
	// Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)frameRGBA, buffer, AV_PIX_FMT_RGBA,
			pWidth, pHeight);

	scalebuffersize = avpicture_get_size(AV_PIX_FMT_YUV420P, pWidth, pHeight);
	if(scalebuffer != NULL){
	    free(scalebuffer);
	}
	scalebuffer = (uint8_t *) av_malloc(scalebuffersize * sizeof(uint8_t));
	avpicture_fill((AVPicture *)scaleFrame, scalebuffer, AV_PIX_FMT_YUV420P, pWidth, pHeight);
	pthread_mutex_unlock(&mutex);
	return 0;
}

void finish(JNIEnv *pEnv) {
	pthread_mutex_destroy(&mutex);
	//unlock the bitmap
	AndroidBitmap_unlockPixels(pEnv, bitmap);
	av_free(buffer);
	av_free(scalebuffer);
	// Free the RGB image
	av_free(frameRGBA);
	// Free the YUV frame
	av_free(decodedFrame);
	av_free(scaleFrame);
	// Close the codec
	avcodec_close(vCodecCtx);
	avcodec_close(aCodecCtx);
	// Close the video file
	avformat_close_input(&formatCtx);
}

void decodeAndRender(JNIEnv *pEnv) {
	ANativeWindow_Buffer 	windowBuffer;
	AVPacket        		packet;
	int 					i=0;
	int            			frameFinished;
	int 					lineCnt;
	struct timeval start;
	struct timeval end;
	float time_use=0;
	while(av_read_frame(formatCtx, &packet)>=0 && !stop) {
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) {
			//gettimeofday(&start,NULL); //gettimeofday(&start,&tz);结果一样
			// Decode video frame
			avcodec_decode_video2(vCodecCtx, decodedFrame, &frameFinished,
			   &packet);
			//gettimeofday(&end,NULL);
			//time_use=(end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);//微秒
			//LOGI("avcodec_decode_video2 time_use is %.10f\n",time_use);
			// Did we get a video frame?
			if(frameFinished) {
				pthread_mutex_lock(&mutex);
                /* save frame after decode to yuv file
                if(i < 20){
                    FILE *fp;
                    int j;
                    fp = fopen("/storage/emulated/0/android-ffmpeg-tutorial02/1.yuv","ab");
                    for (j = 0; j<vCodecCtx->height; j++) {
                        fwrite(decodedFrame->data[0] + j*decodedFrame->linesize[0], vCodecCtx->width, 1, fp);
                    }
                    for (j = 0; j<vCodecCtx->height/2; j++) {
                        fwrite(decodedFrame->data[1] + j*decodedFrame->linesize[1], vCodecCtx->width/2, 1, fp);
                    }
                    for (j = 0; j<vCodecCtx->height/2; j++){
                        fwrite(decodedFrame->data[2] + j*decodedFrame->linesize[2], vCodecCtx->width/2, 1, fp);
                    }
                    fclose(fp);
                }//*/
				// Convert the image from its native format to RGBA
				gettimeofday(&start,NULL);
				#if 0
				sws_scale
				(
					sws_ctx,
					(uint8_t const * const *)decodedFrame->data,
					decodedFrame->linesize,
					0,
					vCodecCtx->height,
					frameRGBA->data,
					frameRGBA->linesize
				);
				#else
				I420Scale(decodedFrame->data[0],decodedFrame->linesize[0],
						  decodedFrame->data[1],decodedFrame->linesize[1],
						  decodedFrame->data[2],decodedFrame->linesize[2],
						  vCodecCtx->width,vCodecCtx->height,
						  scaleFrame->data[0],scaleFrame->linesize[0],
						  scaleFrame->data[1],scaleFrame->linesize[1],
						  scaleFrame->data[2],scaleFrame->linesize[2],
						  width,height,kFilterNone);

				//why not I420ToRGBA？ ijkplayer uses I420ToABGR
				I420ToABGR(scaleFrame->data[0],scaleFrame->linesize[0],
						  scaleFrame->data[1],scaleFrame->linesize[1],
						  scaleFrame->data[2],scaleFrame->linesize[2],
						  frameRGBA->data[0],frameRGBA->linesize[0],
						  width,height);
				#endif
				gettimeofday(&end,NULL);
				time_use=(end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);//微秒
				LOGI("I420Scale and I420ToABGR use time : %.10f\n",time_use);
                /* save frame after scale to rgba file
                if(i < 20){
                    FILE *fp;
                    fp = fopen("/storage/emulated/0/android-ffmpeg-tutorial02/1.rgba","ab");
                    for (int j = 0; j<height; j++) {
                        fwrite(frameRGBA->data[0] + j*frameRGBA->linesize[0], width*4, 1, fp);
                    }
                    fclose(fp);
                }//*/
				// lock the window buffer
				if (ANativeWindow_lock(window, &windowBuffer, NULL) < 0) {
					LOGE("cannot lock window");
				} else {
					// draw the frame on buffer
					LOGI("copy buffer %d:%d:%d", width, height, width*height*4);
					LOGI("window buffer: %d:%d:%d", windowBuffer.width,
							windowBuffer.height, windowBuffer.stride);
					if(width == windowBuffer.width && height == windowBuffer.height){
						//memcpy(windowBuffer.bits, buffer,  width * height * 4);
						for (int h = 0; h < height; h++){
							memcpy(windowBuffer.bits + h * windowBuffer.stride *4,
								buffer + h * frameRGBA->linesize[0],
								width*4);
						}
					}
					// unlock the window buffer and post it to display
					ANativeWindow_unlockAndPost(window);
					// count number of frames
					++i;
				}
				pthread_mutex_unlock(&mutex);
			}
		}else if(packet.stream_index==audioStream) {
              packet_queue_put(&audioq, &packet);
        }
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
	LOGI("total No. of frames decoded and rendered %d", i);
	finish(pEnv);
}

static int audio_decode_frame(AVCodecContext *codecCtx, uint8_t *audio_buf, int buf_size) {
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;

    int len1, data_size = 0;

    for(;;) {
        while(audio_pkt_size > 0) {
            int got_frame = 0;
            len1 = avcodec_decode_audio4(codecCtx, &frame, &got_frame, &pkt);
            if(len1 < 0) {
                /* if error, skip frame */
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            if (got_frame) {
                data_size =
                    av_samples_get_buffer_size
                    (
                        NULL,
                        codecCtx->channels,
                        frame.nb_samples,
                        codecCtx->sample_fmt,
                        1
                    );
                memcpy(audio_buf, frame.data[0], data_size);
            }
            if(data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if(pkt.data) {
            av_free_packet(&pkt);
        }
        if(quit) {
            return -1;
        }
        if(packet_queue_get(&audioq, &pkt, 1) < 0) {
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }
}

jint naGetPcmBuffer(JNIEnv *pEnv, jobject pObj, jbyteArray buffer, jint len) {
    jbyte *pcm = (*pEnv)->GetByteArrayElements(pEnv, buffer, NULL);
    jsize pcmsize = (*pEnv)->GetArrayLength(pEnv, buffer);
    jbyte *pcmindex = pcm;

    int len1, audio_size;

    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while(len > 0) {
        if(audio_buf_index >= audio_buf_size) {
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(aCodecCtx, audio_buf, audio_buf_size);
            if(audio_size < 0) {
	            /* If error, output silence */
	            audio_buf_size = 1024; // arbitrary?
	            memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if(len1 > len)
            len1 = len;
        memcpy(pcmindex, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        pcmindex += len1;
        audio_buf_index += len1;
    }
    (*pEnv)->ReleaseByteArrayElements(pEnv, buffer, pcm, 0);
    return audio_buf_index;
}

/**
 * start the video playback
 */
void naPlay(JNIEnv *pEnv, jobject pObj) {
	//create a new thread for video decode and render
	pthread_t decodeThread;
	stop = 0;
	pthread_create(&decodeThread, NULL, decodeAndRender, NULL);
}

/**
 * stop the video playback
 */
void naStop(JNIEnv *pEnv, jobject pObj) {
	stop = 1;
}

jint JNI_OnLoad(JavaVM* pVm, void* reserved) {
	JNIEnv* env;
	if ((*pVm)->GetEnv(pVm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		 return -1;
	}
	JNINativeMethod nm[8];
	nm[0].name = "naInit";
	nm[0].signature = "(Ljava/lang/String;)I";
	nm[0].fnPtr = (void*)naInit;

	nm[1].name = "naSetSurface";
	nm[1].signature = "(Landroid/view/Surface;)V";
	nm[1].fnPtr = (void*)naSetSurface;

	nm[2].name = "naGetVideoRes";
	nm[2].signature = "()[I";
	nm[2].fnPtr = (void*)naGetVideoRes;

	nm[3].name = "naGetAudioInfo";
	nm[3].signature = "()[I";
	nm[3].fnPtr = (void*)naGetAudioInfo;

	nm[4].name = "naSetup";
	nm[4].signature = "(II)I";
	nm[4].fnPtr = (void*)naSetup;

	nm[5].name = "naPlay";
	nm[5].signature = "()V";
	nm[5].fnPtr = (void*)naPlay;

	nm[6].name = "naStop";
	nm[6].signature = "()V";
	nm[6].fnPtr = (void*)naStop;

	nm[7].name = "naGetPcmBuffer";
    nm[7].signature = "([BI)I";
    nm[7].fnPtr = (void*)naGetPcmBuffer;

	jclass cls = (*env)->FindClass(env, "lyn/android_ffmpeg/tutorial/MainActivity");
	//Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, cls, nm, 8);
	av_jni_set_java_vm(pVm, NULL);
	return JNI_VERSION_1_6;
}

