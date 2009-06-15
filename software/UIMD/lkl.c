
#include "lkl.h"
#include "common.h"
#include "connect.h"

static short data_to_send[MAX_CLIENTS];
pthread_cond_t cond_var[MAX_CLIENTS];
short terminate_thread = 0;

char data[MOUSE_DATA_LENGTH]; // = NULL;

/* the communication threads */
pthread_t threads[MAX_CLIENTS], event_consumer;
pthread_t mouse_activity_interceptor, kb_actv_interceptor;
short init_mouse_interceptor = 0;
short connected_count = 0;
short init_threads = 0;

/* mutex to protect log file from corruption */
pthread_mutex_t log_msg = PTHREAD_MUTEX_INITIALIZER;
/* one mutex for each thread sending data to a client */
pthread_mutex_t send_data[MAX_CLIENTS];

/* buffering related */
extern struct input_event event_buffer[EVENT_BUFFER_MAX];
extern pthread_mutex_t mtx_buffer;
extern int counter, in, out;
extern pthread_cond_t buffer_empty;


extern int   client_socks[MAX_CLIENTS];
extern short is_client_connected[MAX_CLIENTS];
extern int client_count;
extern char mouse_event_interface[STRING_LENGTH];
extern char kb_event_interface[STRING_LENGTH];

/* thread function to send data to a client */
void* sent_to_client(void *idx)
{
	int client_index = *((int*) idx), i, j;
	short bytes_sent = 0, data_length = 0;
	char temp[STRING_LENGTH];

	sprintf(temp, "Client index: %d; Client fd: %d", client_index, client_socks[client_index]);
	pthread_mutex_lock(&log_msg);
	log_message(temp);
	pthread_mutex_unlock(&log_msg);

	if(client_socks[client_index] > -1)
	{
		while(1)
		{
			if(terminate_thread || (is_client_connected[client_index] == 0))
				break;

			pthread_mutex_lock(&send_data[client_index]);
			pthread_mutex_lock(&log_msg);
			log_message("trying to send data");
			pthread_mutex_unlock(&log_msg);

			while(data_to_send[client_index] == 0)
			{
				pthread_mutex_lock(&log_msg);
				log_message("Waiting before sending .....");
				pthread_mutex_unlock(&log_msg);
				pthread_cond_wait(&cond_var[client_index] ,&send_data[client_index]);
			}


			pthread_mutex_lock(&log_msg);
			log_message("Proceeding to send data");
			pthread_mutex_unlock(&log_msg);

			if(data_to_send[client_index] == 1)
			{
				if(data[0] == 'm')
					data_length = MOUSE_DATA_LENGTH - 1;
				else
				{
					pthread_mutex_lock(&log_msg);
					log_message("Invalid data");
					pthread_mutex_unlock(&log_msg);					
				}

				errno = 0;				
				bytes_sent = send(client_socks[client_index], data, data_length, 0);

				if((errno != 0) || (bytes_sent < data_length))
				{
					sprintf(temp, "Data: %s : Bytes sent: %d ", data, bytes_sent);
					pthread_mutex_lock(&log_msg);
					log_message(temp);
					log_message(strerror(errno));
					pthread_mutex_unlock(&log_msg);
				}

				data_to_send[client_index] = 0;
			}
			pthread_mutex_unlock(&send_data[client_index]);

			usleep(MSEC);
		}
	}

	pthread_mutex_lock(&send_data[client_index]);
	close(client_socks[client_index]);
	client_socks[client_index] = INVALID_SOCKET;
	is_client_connected[client_index] = 0;
	pthread_mutex_unlock(&send_data[client_index]);

	pthread_mutex_lock(&log_msg);
	log_message(temp);
	log_message("** Client connection thread exiting **");
	pthread_mutex_unlock(&log_msg);
}

/* CONSUMER 1 */
/* Remove events from buffer and send to clients */
void* consume_events(void* p)
{
	char tmp[MOUSE_DATA_LENGTH], temp[STRING_LENGTH];
	short i, j, data_not_sent, total_failures = 0, max_failures = 2*MAX_CLIENTS, failures = 0;
	struct input_event inpt_evnt;
	short szinp_evnt = sizeof(struct input_event);
	short szushort = sizeof(unsigned short), szuint = sizeof(unsigned int);

	tmp[0] = 'm';
	tmp[MOUSE_DATA_LENGTH - 1] = '\0';

	while(1)
	{
		if(terminate_thread)
			break;

		pthread_mutex_lock(&mtx_buffer);

		/* sleep if buffer is empty */
		while(counter == 0)
		{
			pthread_mutex_lock(&log_msg);
			log_message("** Waiting for event **");
			pthread_mutex_unlock(&log_msg);

			pthread_cond_wait(&buffer_empty, &mtx_buffer);
		}

		sprintf(temp, "Counter = %d", counter);
		pthread_mutex_lock(&log_msg);
		log_message("** Found event **");
		log_message(temp);
		pthread_mutex_unlock(&log_msg);
		

		/* pop next event */
		memcpy(&inpt_evnt, &event_buffer[out], szinp_evnt);
		out = (out + 1) % EVENT_BUFFER_MAX;
		--counter;
		pthread_mutex_unlock(&mtx_buffer);

		/* serialize data to a string */
		j = 1;
		memcpy(&tmp[j], &(inpt_evnt.type), szushort);
		j += szushort;

		memcpy(&tmp[j], &(inpt_evnt.code), szushort);
		j += szushort;

		memcpy(&tmp[j], &(inpt_evnt.value), szuint);

		/* check if the threads finished sending last event */
		total_failures = 0;
		//memset(client_failures, '0', MAX_CLIENTS * sizeof(short));
		while(total_failures < max_failures)
		{
			failures = 0;
			for(i = 0; i < MAX_CLIENTS && (is_client_connected[i] == 1); ++i)
			{
				if(data_to_send[i] == 1)
				{
					if(total_failures > 0)
					{
						sprintf(temp, "Index = %d; Fd = %d", i, client_socks[i]);
						pthread_mutex_lock(&log_msg);
						log_message("** A thread didn't finish sending previous data **");
						log_message("Check if the network is down!");
						log_message(temp);
						pthread_mutex_unlock(&log_msg);
					}
					++failures;

					//++(client_failures[i]);
					/*if(client_failures[i] >= 5)
					{
						is_client_connected[i] = 0;
						pthread_mutex_lock(&log_msg);
						log_message("*** A client is not responding. Closing connection. ***");
						pthread_mutex_unlock(&log_msg);
					}*/
				}
			}

			if(failures == 0)
				break;
			total_failures += failures;
			usleep(MSEC * 100); /* wait for the thread to finish */
		}

		memcpy(data, tmp, MOUSE_DATA_LENGTH);

		for(i = 0; (i < MAX_CLIENTS) && (is_client_connected[i] == 1); ++i)
		{
			pthread_mutex_lock(&send_data[i]);
			data_to_send[i] = 1;
			pthread_cond_signal(&cond_var[i]);
			pthread_mutex_unlock(&send_data[i]);
		}
		usleep(MSEC * 200);
	}

	pthread_mutex_lock(&log_msg);
	log_message("*** Consumer thread exited ***");
	pthread_mutex_unlock(&log_msg);
}

void create_threads(void)
{
	int rc, i;

	if(init_threads == 0)
	{
		data[0] = '\0';
		for(i = 0; i < MAX_CLIENTS; ++i)
		{
			pthread_cond_init (&cond_var[i], NULL);
			pthread_mutex_init(&send_data[i], NULL);
			data_to_send[i] = 0;
		}

		rc = pthread_create(&event_consumer, NULL, consume_events, NULL);
		if(rc != 0)
			log_message("*** Failed to create event consumer ***");
		else
			log_message("*** Event consumer thread created ***");

		init_threads = 1;
	}
}

/*** PRODUCER 1 ***/
/* mouse file descriptor */
int mouse_fd;

void* log_mouse_events()
{
	int count, i;
	struct input_event mouse_event[EVENT_BUNCH_SIZE];
	char dev_name[STRING_LENGTH];
	short szinpt_evnt = sizeof(struct input_event);
	int max_read_sz = szinpt_evnt * EVENT_BUNCH_SIZE;
	
	pthread_mutex_lock(&log_msg);
	log_message("Trying to open mouse event interface: " /*MOUSE_DEVICE*/);
	log_message(mouse_event_interface);
	pthread_mutex_unlock(&log_msg);

	
	/*mouse_fd = open(MOUSE_DEVICE, O_RDONLY);*/
	mouse_fd = open(mouse_event_interface, O_RDONLY);
	if( mouse_fd < 0 )
	{
		pthread_mutex_lock(&log_msg);
		log_message("Failed to open mouse device");
		pthread_mutex_unlock(&log_msg);
		return;
	}

	/* write device name to log file */
	count = ioctl(mouse_fd, EVIOCGNAME(sizeof(dev_name)), dev_name);
	if(count < 0)
	{
		pthread_mutex_lock(&log_msg);
		log_message("**** Failed to get mouse device name ****");
		pthread_mutex_unlock(&log_msg);
	}
	else
	{
		pthread_mutex_lock(&log_msg);
		log_message("Mouse device name: ");
		log_message(dev_name);
		pthread_mutex_unlock(&log_msg);
	}

	while(1)
	{
		//count = read(mouse_fd, &mouse_event, szinpt_evnt);
		count = read(mouse_fd, mouse_event, max_read_sz);

		if(count > 0)
		{
			/* find number of events read */
			count /= szinpt_evnt;

			/* we have a mouse event to send */
			/* Q the message(s) in the buffer */
			pthread_mutex_lock(&mtx_buffer);

			/* can we do it in one shot */
			if((in + count) < EVENT_BUFFER_MAX)
			{
				if(counter == EVENT_BUFFER_MAX)
				{
					/* signal waiting consumer */
					pthread_cond_signal(&buffer_empty);
					pthread_mutex_unlock(&mtx_buffer);

					pthread_mutex_lock(&log_msg);
					log_message("*****EVENT BUFFER FULL*****. Try increasing buffer size.");
					pthread_mutex_unlock(&log_msg);
					continue;
				}

				memcpy(&event_buffer[in], mouse_event, szinpt_evnt * count);
				in += count;
				counter += count;
			}
			else
			{
				for(i = 0; i < count; ++i)
				{
					if(counter == EVENT_BUFFER_MAX)
					{
						pthread_mutex_lock(&log_msg);
						log_message("*****EVENT BUFFER FULL*****. Try increasing buffer size");
						pthread_mutex_unlock(&log_msg);
						break;
					}

					memcpy(&event_buffer[in], &mouse_event[i], szinpt_evnt);
					in = (in + 1) % EVENT_BUFFER_MAX;
					++counter;
				}
			}

			pthread_cond_signal(&buffer_empty);
			pthread_mutex_unlock(&mtx_buffer);
		}

		//usleep(MSEC);

		if(terminate_thread > 0)
			break;
	}

	close(mouse_fd);
	pthread_mutex_lock(&log_msg);
	log_message("*** Mouse event thread exited ***");
	pthread_mutex_unlock(&log_msg);
}

/*** PRODUCER 2 ***/
/* Enqueue keyboard event in buffer */
int kb_fd = -1;
void* log_kb2_events()
{
	int count, i;
	struct input_event kb_event[EVENT_BUNCH_SIZE];
	char dev_name[STRING_LENGTH];
	short szinpt_evnt = sizeof(struct input_event);
	int max_read_sz = szinpt_evnt * EVENT_BUNCH_SIZE;
	
	pthread_mutex_lock(&log_msg);
	log_message("Trying to open kb event interface: " /*KB_DEVICE*/);
	log_message(kb_event_interface);
	pthread_mutex_unlock(&log_msg);

	/*kb_fd = open(KB_DEVICE, O_RDONLY);*/
	kb_fd = open(kb_event_interface, O_RDONLY);
	if( kb_fd < 0 )
	{
		pthread_mutex_lock(&log_msg);
		log_message("Failed to open kb device");
		pthread_mutex_unlock(&log_msg);
		return;
	}

	/* write device name to log file */
	count = ioctl(kb_fd, EVIOCGNAME(sizeof(dev_name)), dev_name);
	if(count < 0)
	{
		pthread_mutex_lock(&log_msg);
		log_message("**** Failed to get kb device name ****");
		pthread_mutex_unlock(&log_msg);
	}
	else
	{
		pthread_mutex_lock(&log_msg);
		log_message("KB device name: ");
		log_message(dev_name);
		pthread_mutex_unlock(&log_msg);
	}

	while(1)
	{
		count = read(kb_fd, kb_event, max_read_sz);

		if(count > 0)
		{
			/* find number of events read */
			count /= szinpt_evnt;

			/* we have a keyboard event to send;
			Q the message in the buffer */
			pthread_mutex_lock(&mtx_buffer);

			/* can we do it in one shot */
			if((in + count) < EVENT_BUFFER_MAX)
			{
				if(counter == EVENT_BUFFER_MAX)
				{
					/* signal waiting consumer */
					pthread_cond_signal(&buffer_empty);
					pthread_mutex_unlock(&mtx_buffer);

					pthread_mutex_lock(&log_msg);
					log_message("*****EVENT BUFFER FULL*****. Try increasing buffer size.");
					pthread_mutex_unlock(&log_msg);
					continue;
				}

				memcpy(&event_buffer[in], kb_event, szinpt_evnt * count);
				in += count;
				counter += count;
			}
			else
			{
				for(i = 0; i < count; ++i)
				{
					if(counter == EVENT_BUFFER_MAX)
					{
						pthread_mutex_lock(&log_msg);
						log_message("*****EVENT BUFFER FULL*****. Try increasing buffer size");
						pthread_mutex_unlock(&log_msg);
						break;
					}

					memcpy(&event_buffer[in], &kb_event[i], szinpt_evnt);
					in = (in + 1) % EVENT_BUFFER_MAX;
					++counter;
				}
			}

			pthread_cond_signal(&buffer_empty);
			pthread_mutex_unlock(&mtx_buffer);
		}

		//usleep(MSEC);

		if(terminate_thread > 0)
			break;
	}

	close(kb_fd);
	pthread_mutex_lock(&log_msg);
	log_message("*** Keyboard event thread exited ***");
	pthread_mutex_unlock(&log_msg);
}

/* increase priority for thread */
void jackup_priority(const pthread_t* ppthrd)
{
	struct sched_param schd_prm;
	int policy;

	/* jack up the priority a bit */
	memset(&schd_prm, 0, sizeof(schd_prm));
	if(pthread_getschedparam(*ppthrd, &policy, &schd_prm))
		log_message("pthread_getschedparam() failed.");
		
	if(sched_get_priority_max(policy) >= (schd_prm.sched_priority + PRIORITY_JACKUP))
		schd_prm.sched_priority += PRIORITY_JACKUP;

	if(pthread_setschedprio(*ppthrd, schd_prm.sched_priority))
		log_message("Priority jack-up attempt failed for thread");
	else
		log_message("Priority jacked-up for thread");
}

/* starts the mouse and key interceptor threads */
void start_log(void)
{
	int rc;

	if(init_threads == 0)
		create_threads();

	rc = pthread_create(&mouse_activity_interceptor, NULL, log_mouse_events, NULL);
	if(rc != 0)
		log_message("**** Failed to create mouse activity interceptor pthread ****");
	else
	{
		log_message("mouse interceptor created");
		//jackup_priority(&mouse_activity_interceptor);
	}

	rc = pthread_create(&kb_actv_interceptor, NULL, log_kb2_events, NULL);
	if(rc != 0)
		log_message("**** Failed to create keyboard activity interceptor pthread ****");
	else
	{
		log_message("keyboard interceptor created");
		//jackup_priority(&kb_actv_interceptor);
	}

	pthread_join(mouse_activity_interceptor, NULL);
	pthread_join(kb_actv_interceptor, NULL);
}

