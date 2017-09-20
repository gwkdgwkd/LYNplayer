#include <stdio.h>

#include <jni.h>

#define LOG_TAG "android-ffmpeg-tutorial03"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGD(...) __android_log_print(5, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

typedef int (*get_pcm_buffer)(void**, size_t*);

typedef struct audioArgs {
    int rate;
    int channels;
    get_pcm_buffer pcm_callback;
}*audioArgsPtr;
