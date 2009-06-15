#ifndef __COMMON_H__
#define __COMMON_H__

//#define RUNNING_DIR	 "/home/agagneja/uimd2"
#define LOCK_FILE	 "uimd.lock"
#define LOG_FILE	 "uimd.log"
//#define KEY_LOG_FILE "keylog.log"
#define CONFIG_FILE	 "uimd.config"
#define MAX_CLIENTS  5

#define RUN_MODE	  "run_mode="
#define CLIENT_IP	  "client_ip="
#define SERVER_IP	  "server_ip="
#define RUN_DIR		  "run_dir="
#define MS_EVNT_INTFC "mouse_event_interface="
#define KB_EVNT_INTFC "keyboard_event_interface="
#define UINP_DEV 	  "uinput_device="
#define SRVR_PORT     "server_port="

#define SERVER		  "server"
#define CLIENT        "client"

#define KEYBOARD_PORT 0x60
#define STRING_LENGTH 256
#define SLEEP_SECONDS 10
#define EVENT_BUNCH_SIZE 64
#define PRIORITY_JACKUP 10

#define INVALID_SOCKET -1

#define UINPUT_DEVICE  "/dev/input/uinput"
#define UINPUT_DEVICE2 "/dev/uinput"
#define UINPUT_DEVICE3 "/dev/misc/uinput"

#define MOUSE_DATA_LENGTH (sizeof(unsigned int) + 2 * sizeof(unsigned short) + 2)
#define EVENT_BUFFER_MAX 5000

#include <string.h>
#include <errno.h>

void log_message(char* message);

#endif
