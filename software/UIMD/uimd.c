/*
To compile:	gcc -lpthread -O2 -o uimd *.c
To run:		./uimd
To test daemon:	ps -ef | grep uimd
To test log:	tail -f /home/agagneja/uimd/uimd.log
To test signal:	kill -HUP `cat /home/agagneja/uimd/uimd.lock`
To terminate:	kill `cat /home/agagneja/uimd/uimd.lock`
*/

#include "lkl.h"
#include "connect.h"

char mode[STRING_LENGTH], clients[MAX_CLIENTS][STRING_LENGTH], server[STRING_LENGTH];
char run_dir[STRING_LENGTH];
char log_file[STRING_LENGTH];
char temp[STRING_LENGTH];
int server_port = 9990; /* default */
char mouse_event_interface[STRING_LENGTH];
char kb_event_interface[STRING_LENGTH];
char uinp_device[STRING_LENGTH];

void log_message(char* message)
{	
	FILE *logfile;
	const char* filename = LOG_FILE;
	time_t timet;

	logfile = fopen(filename, "a");
	if(!logfile)
		return;

	time(&timet);
	fprintf(logfile, "%s : %s\n", ctime(&timet), message);
	fclose(logfile);
}

/* sockets: one for each client */
int   client_socks[MAX_CLIENTS];
short is_client_connected[MAX_CLIENTS];

int /*uinp_fd = -1,*/ uinp_ms_fd = -1; /* uinput device file desc */
extern short terminate_thread;

extern int client_count;
extern int server_socket;

/* do clean-up on terminate signal */
void signal_handler(int sig)
{
	int i;

	switch(sig)
	{
		case SIGHUP:
			/* re-read config file (when we have one!!) */
			log_message("Hangup signal received");
			break;

		case SIGTERM:
			log_message("*** Terminate signal received ***");
			terminate_thread = 1;
			sleep(SLEEP_SECONDS); /* wait for the threads to terminate */

			/* cleanup code */
			/*for(i = 0; i < MAX_CLIENTS; ++i)
			{
				if(is_client_connected[i] > 0)
					close(client_socks[i]);
			}*/

			/* Destroy the uinput devices */
			/*if(server_socket != INVALID_SOCKET)
				close(server_socket);*/

			break;
	}
}

/* perform the ground work needed to create a daemon 
	1. Fork and create a new process
	2. Close file descriptors inherited from the parent process
*/
void daemonize()
{
	int i, lfp;
	char str[10];

	if(getppid() == 1)
		return; /* already a daemon */

	i = fork();
	if(i < 0)
		exit(1); /* fork error */

	if(i > 0)
		exit(0); /* parent exits */

	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for(i = getdtablesize(); i >= 0; --i) 
		close(i); /* close all descriptors */

	/* handle standart I/O */
	i = open("/dev/null", O_RDWR); /* open stdin */
	dup(i); /* open stdout */
	dup(i); /* open stderr */

	umask(027); /* set newly created file permissions */

	//chdir(RUNNING_DIR); /* change running directory */
	if(chdir(run_dir))
	{
		log_message("Invalid running directory!!");
	}
	
	lfp = open(LOCK_FILE, O_RDWR|O_CREAT, 0640);

	if(lfp < 0)
		exit(1); /* can not open */

	if(lockf(lfp, F_TLOCK, 0) < 0)
		exit(0); /* can not lock */

	/* first instance continues */
	sprintf(str, "%d\n", getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */

	signal(SIGCHLD, SIG_IGN); /* ignore child */
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);

	signal(SIGHUP, signal_handler); /* catch hangup signal */
	signal(SIGTERM, signal_handler); /* catch kill signal */
}

/* reads config file at startup to read run mode and server/clients IP address*/
int set_configuration(char* config_file, char* mode, char clients[][256], char* server)
{
	FILE *configfile = NULL;
	char temp[STRING_LENGTH], *pTemp = NULL;
	int i = 0, client_count = -1;

	/* initialize strings*/
	temp  [0] = '\0';
	mode  [0] = '\0';
	server[0] = '\0';
	for( i = 0; i < MAX_CLIENTS; ++i)
		clients[i][0] = '\0';
	run_dir[0] = log_file[0] = temp[0] = mouse_event_interface[0] ='\0';
	kb_event_interface[0] = uinp_device[0] = '\0';

	/* open config file */
	configfile = fopen(config_file, "r");
	if(!configfile)
	{
		log_message("*** Failed to open config file ***");
		return 1;
	}

	/* read config file */
	fgets(temp, STRING_LENGTH, configfile);

	//while( temp[0] != '\0' )
	while( !feof(configfile) )
	{
		//log_message(temp);

		/* ignore comment lines */
		if( temp[0] == ';' || temp[0] == '\0' || temp[0] == '\n' )
		{
			//log_message("ignored comment");
			temp[0] = '\0';
			fgets(temp, STRING_LENGTH, configfile);
			continue;
		}

		/*log_message("after comment handling code");*/

		pTemp = strstr(temp, RUN_MODE);
		if(NULL != pTemp && pTemp[0] != '\0')
		{
			pTemp += strlen(RUN_MODE);

			mode[STRING_LENGTH - 1] = '\0';
			strncpy(mode, pTemp, STRING_LENGTH - 1);
			/* remove new-line added by fgets*/
			mode[strlen(mode) -1] = '\0';

			//log_message(mode);
		}
		else
		{
			//log_message("inside client ip block");
			pTemp = strstr(temp, CLIENT_IP);
			if(NULL != pTemp && pTemp[0] != '\0')
			{
				client_count++;
				if( client_count >= MAX_CLIENTS )
				{
					fclose(configfile);
					return 1;
				}
				pTemp += strlen(CLIENT_IP);

				clients[client_count][STRING_LENGTH - 1] = '\0';
				strncpy(clients[client_count], pTemp, STRING_LENGTH - 1);
				/* remove new-line added by fgets*/
				clients[client_count][strlen(clients[client_count]) - 1] = '\0';
	
				//log_message(clients[client_count]);
			}
			else
			{
				pTemp = strstr(temp, SERVER_IP);
				if(NULL != pTemp && pTemp[0] != '\0')
				{
					pTemp += strlen(SERVER_IP);

					server[STRING_LENGTH - 1] = '\0';
					strncpy(server, pTemp, STRING_LENGTH - 1);
					/* remove new-line added by fgets*/
					server[strlen(server) -1] = '\0';
	
					//log_message(server);
				}
				else if((pTemp = strstr(temp, RUN_DIR)) && NULL != pTemp && pTemp[0] != '\0')
				{
					pTemp += strlen(RUN_DIR);
					run_dir[STRING_LENGTH - 1] = '\0';
					strncpy(run_dir, pTemp, STRING_LENGTH - 1);
					run_dir[strlen(run_dir) -1] = '\0';
				}
				else if((pTemp = strstr(temp, MS_EVNT_INTFC)) && NULL != pTemp && pTemp[0] != '\0')
				{
					pTemp += strlen(MS_EVNT_INTFC);
					mouse_event_interface[STRING_LENGTH - 1] = '\0';
					strncpy(mouse_event_interface, pTemp, STRING_LENGTH - 1);
					mouse_event_interface[strlen(mouse_event_interface) -1] = '\0';
				}
				else if((pTemp = strstr(temp, KB_EVNT_INTFC)) && NULL != pTemp && pTemp[0] != '\0')
				{
					pTemp += strlen(KB_EVNT_INTFC);
					kb_event_interface[STRING_LENGTH - 1] = '\0';
					strncpy(kb_event_interface, pTemp, STRING_LENGTH - 1);
					kb_event_interface[strlen(kb_event_interface) -1] = '\0';
				}
				else if((pTemp = strstr(temp, UINP_DEV)) && NULL != pTemp && pTemp[0] != '\0')
				{
					pTemp += strlen(UINP_DEV);
					uinp_device[STRING_LENGTH - 1] = '\0';
					strncpy(uinp_device, pTemp, STRING_LENGTH - 1);
					uinp_device[strlen(uinp_device) -1] = '\0';
				}
				else if((pTemp = strstr(temp, SRVR_PORT)) && NULL != pTemp && pTemp[0] != '\0')
				{
					pTemp += strlen(SRVR_PORT);
					temp[STRING_LENGTH - 1] = '\0';
					strncpy(temp, pTemp, STRING_LENGTH - 1);
					temp[strlen(temp) -1] = '\0';
					sscanf(temp, "%d", &server_port);
				}
			}
		}

		temp[0] ='\0';
		fgets(temp, STRING_LENGTH, configfile);
	}

	log_message("Finished processing config file");

	/* check validity on configuration */

	fclose(configfile);
	return 0;
}


main()
{
	char tmp[STRING_LENGTH];
	short i;

	/* read config file */
	if( 0 != set_configuration(CONFIG_FILE, mode, clients, server) )
	{
		log_message("Invalid config file!");
		exit(1);
	}
	else
	{
		/* log configuration */
		/* run mode */
		sprintf(tmp, "%s%s", RUN_MODE, mode);
		log_message(tmp);

		/* server ip*/
		sprintf(tmp, "%s%s", SERVER_IP, server);
		log_message(tmp);

		/* client ip*/
		for(i = 0; i < MAX_CLIENTS; ++i)
		{
			sprintf(tmp, "%s%s", CLIENT_IP, clients[i]);
			log_message(tmp);
		}

		/* run_dir */
		sprintf(tmp, "%s%s", RUN_DIR, run_dir);
		log_message(tmp);

		/* mouse_event_interface */
		sprintf(tmp, "%s%s", MS_EVNT_INTFC, mouse_event_interface);
		log_message(tmp);

		/* kb_event_interface */
		sprintf(tmp, "%s%s", KB_EVNT_INTFC, kb_event_interface);
		log_message(tmp);

		/* uniput device */
		sprintf(tmp, "%s%s", UINP_DEV, uinp_device);
		log_message(tmp);

		/* server port */
		sprintf(tmp, "%s%d", SRVR_PORT, server_port);
		log_message(tmp);
	}

	/* create daemon */
	daemonize();
	log_message("** Daemon started **");

	if(!strcasecmp(mode, SERVER))
	{
		/* server mode code */
		int i = 0;
		int client_socks[MAX_CLIENTS];

		log_message("Running server mode:");
		for(i = 0; i < MAX_CLIENTS; ++i)
		{
			if((clients[i] != NULL) && (strlen(clients[i]) > 0))
				log_message(clients[i]);
		}

		if(getuid() || getgid())
		{
			log_message("Must be root to run UIMD!");
			exit(1);
		}

		/* start listening to incoming client connection(s) */
		server_connect(server);
		
		while(1)
		{
			if(client_count > 0)
			{
				log_message("Connected!! Starting to intercept keyboard and mouse input");
				start_log();
				break;
			}

			/*log_message("No client(s) connected. Sleeping");*/
			usleep(MSEC * 100); /* run */
		}
	}
	else if(!strcasecmp(mode, CLIENT))
	{
		int clientsock, i, j, write_ret;
		unsigned int clientlen;
		struct sockaddr_in serversockaddr, clientsocaddr;
		char buffer[STRING_LENGTH], temp[STRING_LENGTH];
		unsigned char *puchar = NULL;
		int received = -1, failure_count = 0;
		struct input_event event;
		struct uinput_user_dev mouse_dev;
		unsigned short type, code;
		unsigned int value;
		short szushort = sizeof(unsigned short), szuint = sizeof(unsigned int);

		log_message("running in client mode\nserver ip: ");
		log_message(server);

		/* Create the TCP socket */
		if((clientsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		{
			log_message("Failed to create server socket");
			return;
		}
		log_message("socket created");
		
		/* Construct the server sockaddr_in structure */
		memset(&serversockaddr, 0, sizeof(serversockaddr));  /* Clear struct */
		serversockaddr.sin_family      = AF_INET;            /* Internet/IP  */
		serversockaddr.sin_addr.s_addr = inet_addr(server);  /* server ip    */
		/*serversockaddr.sin_port        = htons(SERVER_PORT);*/ /* server port  */
		serversockaddr.sin_port        = htons(server_port);

		if((system("/sbin/modprobe uinput")) < 0)
		{
			log_message("Failed to load uinput driver. This is probably because the current user doesn't have root privileges");
			/* exit(1) */ /* continue for now */
		}

		memset(&mouse_dev, 0, sizeof(struct uinput_user_dev));

		strncpy (mouse_dev.name, "UIMD Mouse + Keyboard device", UINPUT_MAX_NAME_SIZE);
		mouse_dev.id.version = 4;
		mouse_dev.id.bustype = BUS_USB;

		// for mouse + keyboard events
		uinp_ms_fd = open(uinp_device, O_WRONLY | O_NDELAY);
		if(uinp_ms_fd < 0)
		{
			log_message("Unable to open ");
			log_message(uinp_device);

			/* try other options */
			uinp_ms_fd = open(UINPUT_DEVICE, O_WRONLY | O_NDELAY);
			if(uinp_ms_fd < 0)
			{
				log_message("Unable to open " UINPUT_DEVICE);

				uinp_ms_fd = open(UINPUT_DEVICE2, O_WRONLY | O_NDELAY);
				if(uinp_ms_fd < 0)
				{
					log_message("Unable to open " UINPUT_DEVICE2);

					uinp_ms_fd = open(UINPUT_DEVICE3, O_WRONLY | O_NDELAY);
					if(uinp_ms_fd < 0)
					{
						log_message("Unable to open " UINPUT_DEVICE3);
						log_message("*** UIMD needs root privileges ***"); /* or somebody moved uinput!! what?! why?? */
			        	return -1;
					}
				}
			}
		}

		ioctl(uinp_ms_fd, UI_SET_EVBIT, EV_KEY);
		ioctl(uinp_ms_fd, UI_SET_EVBIT, EV_REP); //new
		ioctl(uinp_ms_fd, UI_SET_EVBIT, EV_REL);
		ioctl(uinp_ms_fd, UI_SET_EVBIT, EV_ABS);
		ioctl(uinp_ms_fd, UI_SET_EVBIT, EV_SYN);
		ioctl(uinp_ms_fd, UI_SET_RELBIT, REL_X);
		ioctl(uinp_ms_fd, UI_SET_RELBIT, REL_Y);
		ioctl(uinp_ms_fd, UI_SET_RELBIT, REL_WHEEL);

		for(i=0; i < 256; i++) //new
			ioctl(uinp_ms_fd, UI_SET_KEYBIT, i);

		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_MOUSE);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_LEFT);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_MIDDLE);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_RIGHT);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_SIDE);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_EXTRA);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_FORWARD);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_BACK);
		ioctl(uinp_ms_fd, UI_SET_KEYBIT, BTN_TASK);

		write(uinp_ms_fd, &mouse_dev, sizeof(mouse_dev));

		if(ioctl(uinp_ms_fd, UI_DEV_CREATE) < 0)
		{
			log_message("** Unable to create UINPUT mouse + keyboard device **");
			exit(1);
		}
		log_message("** Mouse + keyboard device created **");

		log_message("Connecting ...");
		failure_count = 0;
		while(1)
		{
			/* Establish connection */
			errno = 0;
			if(connect(clientsock, (struct sockaddr *) &serversockaddr, sizeof(serversockaddr)) < 0) 
			{
				++failure_count;
				if(failure_count >= 100)
				{
					log_message("**** Failed to connect to server ****");
					log_message(strerror(errno));
					return;
				}

				sleep(SLEEP_SECONDS);
			}
			else
				break;
		}
		log_message("**** Connected to server ****" );

		failure_count = 0;
		while(1)
		{
			received = -1;

            /* Receive message */
			errno = 0;
			received = recv(clientsock, buffer, STRING_LENGTH, 0);
			if(received <= 0)
			{
				log_message(strerror(errno));
				++failure_count;
				if(failure_count > 20)
					break;

				/*usleep(MSEC);*/
				continue;
            }

			if(buffer[0] == 'm' && received >= MOUSE_DATA_LENGTH - 1) /* mouse data */
			{
				j = 1;

				memset(&event, 0, sizeof(event));
				memcpy(&(event.type), &buffer[j], szushort);
				j += szushort;

				memcpy(&(event.code), &buffer[j], szushort);
				j += szushort;

				memcpy(&(event.value), &buffer[j], szuint);

				gettimeofday(&event.time, NULL);
				/*event.type = type;
				event.code = code;
				event.value = value;*/

				errno = 0;
				write_ret = write(uinp_ms_fd, &event, sizeof(event));

				if(write_ret < sizeof(event))
				{
					log_message("*** Failed to write to uinput device ***");
					log_message(strerror(errno));
				}
				//log_message("got event");

				/* we get this from the event interface anyway so we don't need it really; just a waste of time probably*/
				/*event.type = EV_SYN;
				event.code = SYN_REPORT;
				event.value = 0;
				write(uinp_ms_fd, &event, sizeof(event));*/
			}

			usleep(MSEC); /* run */

			if(terminate_thread)
				break;
		}

		/* Destroy the input device */
		close(clientsock);
		ioctl(uinp_ms_fd, UI_DEV_DESTROY);
		close(uinp_ms_fd);

		log_message("destroyed device and closed uinput");
	}
}

