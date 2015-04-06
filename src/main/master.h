/*
 * master.h
 *
 *  Created on: 2015年1月4日
 *      Author: ron
 */

#ifndef SRC_MAIN_MASTER_H_
#define SRC_MAIN_MASTER_H_
#include <pthread.h>
#include <mpi.h>
#include "opera_code_sturcture.h"
#include "namespace.h"
#include "conf.h"
#include "../tool/message.h"
#include "../global.h"

/**
 * 数据服务器描述
 */
typedef struct
{
	int id;								//机器id
	long last_time;						//上次交互时间
	uint32_t s_blocks_count;
	uint32_t s_free_blocks_count;
	uint_least8_t status;
	char *block_mem_map;
}data_server_des;

typedef struct{
	unsigned int server_count;
	unsigned long index;
	data_server_des *data_server_list;
}data_servers;

/**
 * node of request queue, including request information from client and data server
 */
typedef struct{
	unsigned short request_type;
	MPI_Status status;
	void *message;
	request *next;
}request;

/**
 * request queue
 */
typedef struct{
	int request_num;
	request *head;
	request *tail;
}master_request_queue;

static void init_queue();

static void in_queue();

static request* de_queue();

static int request_is_empty();

static request* malloc_request(char *buf, int size);

static file_location_des *maclloc_data_block(unsigned long file_size);

/**
 * use memcpy to implement copy of MPI_Status
 */
static void mpi_status_assignment(MPI_Status *status, MPI_Status *s);

/* master initialize
 */
static void master_init();

static void master_server();

static void log_backup();

static void heart_blood();

static void namespace_control();

static int create_file();

pthread_t thread_master_log_backup, thread_master_namespace, thread_master_heart;
pthread_mutex_t mutex_message_buff, mutex_namespace, mutex_request_queue;

char message_buff[MAX_COM_MSG_LEN];

static master_request_queue request_queue_list;

/*
 * data server
 */

static void data_server_init();

static data_servers master_data_servers;

#endif /* SRC_MAIN_MASTER_H_ */
