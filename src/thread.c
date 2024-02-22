#include "../include/thread.h"
#include <stdlib.h>
#include <stdio.h>

// Implement the thread creation and management functions

void create_thread(thread_control_t* ctrl, void* (*start_routine)(void*), void* arg) {
    if (pthread_create(&ctrl->thread_id, NULL, start_routine, arg) != 0) {
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }
}

void join_thread(thread_control_t* ctrl) {
    if (pthread_join(ctrl->thread_id, NULL) != 0) {
        perror("Failed to join thread");
        exit(EXIT_FAILURE);
    }
}

void initialize_thread_control(thread_control_t* ctrl) {
    pthread_mutex_init(&ctrl->mutex, NULL);
    pthread_cond_init(&ctrl->cond_var, NULL);
}

void destroy_thread_control(thread_control_t* ctrl) {
    pthread_mutex_destroy(&ctrl->mutex);
    pthread_cond_destroy(&ctrl->cond_var);
}

// Implement the mutex operations

void lock_mutex(pthread_mutex_t* mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        perror("Failed to lock mutex");
        exit(EXIT_FAILURE);
    }
}

void unlock_mutex(pthread_mutex_t* mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        perror("Failed to unlock mutex");
        exit(EXIT_FAILURE);
    }
}

// Implement the condition variable operations

void wait_cond(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (pthread_cond_wait(cond, mutex) != 0) {
        perror("Failed to wait on condition variable");
        exit(EXIT_FAILURE);
    }
}

void signal_cond(pthread_cond_t* cond) {
    if (pthread_cond_signal(cond) != 0) {
        perror("Failed to signal condition variable");
        exit(EXIT_FAILURE);
    }
}

void broadcast_cond(pthread_cond_t* cond) {
    if (pthread_cond_broadcast(cond) != 0) {
        perror("Failed to broadcast condition variable");
        exit(EXIT_FAILURE);
    }
}
