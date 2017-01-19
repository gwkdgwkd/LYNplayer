#include <stdio.h>
#include <SDL/SDL.h>

#include "LYNtype.h"

int play_yuv420p(cmdArgsPtr args)
{
    SDL_Overlay *bmp;
    SDL_Surface *screen;
    SDL_Rect rect;
    SDL_Event event;
    FILE *fp;
    long filesize;
    int framesize, framenum, frame = 1;
    char caption[256];
    char seekto[10];
    int isplay = 0, isdown = 0, seek = 0, isgo = 0;

    fp = fopen(args->infile, "rb");
    if (fp == NULL) {
        printf("open %s failed!\n", args->infile);
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    framesize = (args->width * args->height) * 3 / 2;
    framenum = filesize / framesize;

    printf("file size : %ld\n", filesize);
    printf("frame size : %d\n", framesize);
    printf("frame num : %d\n", framenum);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

    screen = SDL_SetVideoMode(args->width, args->height, 0, 0);

    if (!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }
    // Allocate a place to put our YUV image on that screen
    bmp = SDL_CreateYUVOverlay(args->width,
                               args->height, SDL_YV12_OVERLAY, screen);
    while (!feof(fp)) {
        if (1 == isplay || 1 == frame || seek != 0) {
            if (seek != 0) {
                frame += seek - 1;
                if (frame < 1) {
                    frame = 1;
                }
                if (frame > framenum) {
                    frame = framenum;
                }
                if (0 != seek - 1) {
                    fseek(fp, (frame - 1) * framesize, SEEK_SET);
                }
                seek = 0;
            }
            memset(caption, 0, sizeof(caption));
            sprintf(caption, "%s,%d/%d,%s", args->infile, frame++,
                    framenum, isplay == 1 ? "play" : "pause");
            SDL_WM_SetCaption(caption, NULL);

            SDL_LockYUVOverlay(bmp);

            fread(bmp->pixels[0], args->width * args->height, 1, fp);
            fread(bmp->pixels[2], args->width * args->height / 4, 1, fp);
            fread(bmp->pixels[1], args->width * args->height / 4, 1, fp);

            SDL_UnlockYUVOverlay(bmp);

            rect.x = 0;
            rect.y = 0;
            rect.w = args->width;
            rect.h = args->height;
            SDL_DisplayYUVOverlay(bmp, &rect);
        } else if (0 == isplay) {
            memset(caption, 0, sizeof(caption));
            sprintf(caption, "%s,%d/%d,%s", args->infile, frame - 1,
                    framenum, isplay == 1 ? "play" : "pause");
            if (isgo == 1) {
                sprintf(caption + strlen(caption), ",seekto: %s", seekto);
            }
            SDL_WM_SetCaption(caption, NULL);
        }
        SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            SDL_Quit();
            exit(0);
            break;
        case SDL_KEYDOWN:
            if ((event.key.keysym.sym >= '0')
                && (event.key.keysym.sym <= '9') && 1 == isgo) {
                isdown = 1;
            }
            switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_DOWN:
            case SDLK_LEFT:
            case SDLK_RIGHT:
            case SDLK_RETURN:
            case SDLK_BACKSPACE:
            case SDLK_ESCAPE:
            case 'g':
            case 'p':
                isdown = 1;
                break;
            default:
                break;
            }
            break;
        case SDL_KEYUP:
            if ((event.key.keysym.sym >= '0')
                && (event.key.keysym.sym <= '9') && 1 == isgo) {
                if (isdown == 1) {
                    seekto[strlen(seekto)] = event.key.keysym.sym;
                    isdown = 0;
                }
            }
            switch (event.key.keysym.sym) {
            case 'q':
                SDL_Quit();
                exit(0);
                break;
            case SDLK_ESCAPE:
                if (isdown == 1 && isgo == 1) {
                    isdown = 0;
                    isgo = 0;
                }
                break;
            case SDLK_BACKSPACE:
                if (isdown == 1 && isgo == 1) {
                    isdown = 0;
                    seekto[strlen(seekto) - 1] = 0;
                }
                break;
            case SDLK_RETURN:
                if (isdown == 1 && isgo == 1) {
                    isgo = 0;
                    isdown = 0;
                    frame = atoi(seekto) + 2;
                    seek = -1;
                }
                break;
            case 'g':
                if (isdown == 1) {
                    isgo = 1;
                    memset(seekto, 0, sizeof(seekto));
                    isdown = 0;
                }
                break;
            case 'p':
                if (isdown == 1) {
                    isplay = ((isplay == 1) ? 0 : 1);
                    isdown = 0;
                }
                break;
            case SDLK_UP:
                if (isdown == 1) {
                    seek = 5;
                    isdown = 0;
                }
                break;
            case SDLK_DOWN:
                if (isdown == 1) {
                    seek = -5;
                    isdown = 0;
                }
                break;
            case SDLK_LEFT:
                if (isdown == 1) {
                    seek = -1;
                    isdown = 0;
                }
                break;
            case SDLK_RIGHT:
                if (isdown == 1) {
                    seek = 1;
                    isdown = 0;
                }
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        SDL_Delay(args->framerate);
    }

    fclose(fp);
    return 0;
}
