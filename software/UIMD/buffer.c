
#include "common.h"
#include "buffer.h"

struct input_event event_buffer[EVENT_BUFFER_MAX];

int counter = 0;
int in = 0, out = 0;

/* condition variable */
pthread_cond_t buffer_empty = PTHREAD_COND_INITIALIZER;

/* mutex */
pthread_mutex_t mtx_buffer = PTHREAD_MUTEX_INITIALIZER;

