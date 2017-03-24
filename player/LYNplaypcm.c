#include <stdio.h>
#include <SDL/SDL.h>
#include "LYNtype.h"

static Uint8 *audio_chunk;
static Uint32 audio_len;
static Uint8 *audio_pos;

void fill_audio(void *udata, Uint8 * stream, int len)
{
    if (audio_len == 0)        /*  Only  play  if  we  have  data  left  */
        return;
    len = (len > audio_len ? audio_len : len); /*  Mix  as  much  data  as  possible  */
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
}

int play_pcm(cmdArgsPtr args)
{
    FILE * fp;
    int pcm_buffer_size = 192000;
    char *pcm_buffer = (char *) malloc(pcm_buffer_size);
    int data_count = 0;
    SDL_Event event;
    size_t readlen;

    fp = fopen(args->infile, "rb");
    if (fp == NULL) {
        printf("open %s failed!\n", args->infile);
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 44100;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024;
    wanted_spec.callback = fill_audio;
    //wanted_spec.userdata = pCodecCtx;

    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        printf("can't open audio.\n");
        return -1;
    }

    //Play
    SDL_PauseAudio(0);
    while (!feof(fp)) {
        if ((readlen = fread(pcm_buffer, 1, pcm_buffer_size, fp)) <= 0) {
            printf("pcm_buffer_size is %d\n", pcm_buffer_size);
            break;
        }
        //printf("Now Playing %10d Bytes data.\n",data_count);
        data_count += readlen;
        //Set audio buffer (PCM data)
        audio_chunk = (Uint8 *) pcm_buffer;
        //Audio buffer length
        audio_len = pcm_buffer_size;
        audio_len = readlen;
        audio_pos = audio_chunk;
        while (audio_len > 0) { //Wait until finish
            SDL_PollEvent(&event);
            switch (event.type) {
                case SDL_QUIT:
                    SDL_Quit();
                    exit(0);
                    break;
                default:
                    break;
            }
            SDL_Delay(1);
        }
    }
    SDL_CloseAudio();        //Close SDL
    SDL_Quit();
    free(pcm_buffer);

    return 0;
}
