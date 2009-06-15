#ifndef __LKl_H__
#define __LKl_H__

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/io.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define _MULTI_THREADED
#include <pthread.h>

#include "common.h"

#define MSEC 1

#define KEYBOARD_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#endif
