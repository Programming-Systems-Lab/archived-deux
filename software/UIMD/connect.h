#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/stat.h>
#include <stdio.h>

#include "common.h"
#include "lkl.h"

int client_connect(char clients[][STRING_LENGTH] /*, int socks[], short is_connected[]*/);

#endif
