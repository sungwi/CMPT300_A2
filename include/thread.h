#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <list.h>

// Define a structure for thread arguments if your threads need to receive specific data.
typedef struct {
    int socket_fd;                      // Socket file descriptor for UDP communication.
    char remote_hostname[256];          // Remote hostname to send messages to.
    char remote_port[6];                // Remote port to send messages to.
    pthread_mutex_t* input_mutex;       // Mutex to control typing access between users.
    List* message_list;       // Shared message list for storing incoming/outgoing messages.
    pthread_cond_t* message_ready_cond; // Condition variable to signal when a new message is ready.
} thread_args_t;


// Define a structure for thread control, including pthread_t and any synchronization primitives.
typedef struct {
    pthread_t thread_id;                  // Thread identifier
    pthread_mutex_t mutex;                // Mutex for thread synchronization
    pthread_cond_t cond_var;              // Condition variable for thread signaling
    // You can add other fields related to thread state or control if needed
} thread_control_t;

// Function prototypes

// Thread creation and management
void create_thread(thread_control_t* ctrl, void* (*start_routine)(void *), void* arg);
void join_thread(thread_control_t* ctrl);
void initialize_thread_control(thread_control_t* ctrl);
void destroy_thread_control(thread_control_t* ctrl);

// Mutex operations
void lock_mutex(pthread_mutex_t* mutex);
void unlock_mutex(pthread_mutex_t* mutex);

// Condition variable operations
void wait_cond(pthread_cond_t* cond, pthread_mutex_t* mutex);
void signal_cond(pthread_cond_t* cond);
void broadcast_cond(pthread_cond_t* cond);

#endif // THREAD_MANAGEMENT_H

