#include "timer.h"

void timer_destroy(TimerPtr tptr)
{
	pthread_mutex_destroy(&tptr->timer_mutex);
	pthread_cond_destroy(&tptr->timer_cond);
}

void timer_thread(void* arg)
{
    TimerPtr tptr = (TimerPtr)arg;

    while(1){
        pthread_mutex_lock(&tptr->timer_mutex);
        while(tptr->wake_up_time.tv_sec == 0 && tptr->wake_up_time.tv_usec == 0) {
            pthread_cond_wait(&tptr->timer_cond, &tptr->timer_mutex);
        }
        pthread_mutex_unlock(&tptr->timer_mutex);

        select(0, NULL, NULL, NULL, &tptr->wake_up_time);

        pthread_mutex_lock(&tptr->timer_mutex);
        tptr->wake_up_time.tv_sec = 0;
        tptr->wake_up_time.tv_usec = 0;
        pthread_cond_signal(&tptr->timer_cond);
        pthread_mutex_unlock(&tptr->timer_mutex);
    }
}

void timer_init(TimerPtr tptr)
{
    memset(tptr, 0, sizeof(tptr));
    pthread_mutex_init(&tptr->timer_mutex, NULL);
    pthread_cond_init(&tptr->timer_cond, NULL);
    pthread_create(&tptr->timer_tid, NULL, timer_thread, (void*)tptr);
}


