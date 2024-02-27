#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <list.h>

// structure for thread arguments 
typedef struct {
    int socket_fd;                      // Socket file descriptor for UDP communication.
    char remote_hostname[256];          // Remote hostname to send messages to.
    char remote_port[6];                // Remote port to send messages to.
    pthread_mutex_t* input_mutex;       // Mutex to control typing access between users.
    List* send_list;       // Shared message list for storing incoming/outgoing messages.
    List* rec_list;
    pthread_cond_t* message_ready_cond; // Condition variable to signal when a new message is ready.
} thread_args_t;



#endif 

