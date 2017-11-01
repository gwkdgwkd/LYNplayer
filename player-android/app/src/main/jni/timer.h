#include "type.h"

typedef struct Timer{
    pthread_t timer_tid;
    pthread_mutex_t timer_mutex;
    pthread_cond_t timer_cond;
    struct timeval wake_up_time;
} Timer,*TimerPtr;

Timer timer;

void timer_init(TimerPtr tptr);
