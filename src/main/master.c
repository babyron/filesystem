/*
 * master.c
 *
 *  Created on: 2015年1月4日
 *      Author: ron
 */
#include <pthread.h>
#include <mpi.h>
#include <assert.h>
#include "master.h"
#include "../structure/message_queue.h"
#include "./namespace/namespace.h"
#include "./data_server/master_data_servers.h"
#include "../global.h"

/*========================Private Prototypes============*/
static namespace* namespace;
static msg_queue_t* message_queue;

static pthread_t *pthread_request_listener;
static pthread_t *pthread_request_handler;
static pthread_mutex_t *pthread_mutex_message_queue;

char *receive_buf;
char *send_buf;
/*========================API Implementation============*/

//TODO when shutdown the application, it must release all the memory resource
int master_init(){
	namespace = create_namespace(1024, 32);
	message_queue = alloc_msg_queue();
	pthread_request_listener = (pthread_t *)malloc(sizeof(pthread_t));
	pthread_request_handler = (pthread_t *)malloc(sizeof(pthread_t));
	pthread_mutex_message_queue = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

	receive_buf = (char *)malloc(MAX_CMD_MSG_LEN);
	send_buf = (char *)malloc(MAX_CMD_MSG_LEN);
	if(namespace == NULL || message_queue == NULL || pthread_request_listener == NULL
			|| pthread_request_handler == NULL ||pthread_mutex_message_queue == NULL){
		master_destroy();
		return -1;
	}
}

//TODO dangerous operation, when shutdown the application
//TODO self-defined structure may be released by it's own free function
int master_destroy(){
	free(namespace);
	free(message_queue);
	free(pthread_request_listener);
	free(pthread_request_handler);
	free(pthread_mutex_message_queue);
}


void init_master_server_info(int machine_count){
	master_data_servers.server_count = machine_count;
	master_data_servers.data_server_list = (data_server_des*)malloc(sizeof(data_server_des) * machine_count);
	assert(master_data_servers.data_server_list != NULL);
}

void mpi_status_assignment(MPI_Status *status, MPI_Status *s) {
	memcpy(status, s, sizeof(MPI_Status));
}

request_node* malloc_request(char *buf, int size, MPI_Status *status) {
	request_node *tmp_request = (request_node*) malloc(sizeof(request_node));
	tmp_request->message = (char *) malloc(size);
	strncpy(tmp_request->message, buf, size);
	tmp_request->next = 0;
	mpi_status_assignment(&(tmp_request->status), status);
	return tmp_request;
}

void free_request_node(request_node *request) {
	free(request->message);
	free(request);
}

/**
 *
 */
void data_server_init() {
	init_queue();
	//master_data_servers.index = 1;
	master_data_servers.server_count = 100;
	master_data_servers.data_server_list = (data_server_des *) malloc(sizeof(data_server_des) * master_data_servers.server_count);
	memset(&master_data_servers, 0, sizeof(data_server_des) * master_data_servers.server_count);
}

/**
 * 1. name space lock initialize
 * 2. message buffer lock initialize
 * 3. request handler start
 * 4. master server start
 */
//void master_init() {
//	//puts("=====master initialize start=====");
////	MPI_Status status;
////	MPI_Recv(message_buff, MAX_COM_MSG_LEN, MPI_CHAR, MPI_ANY_SOURCE,
////	MPI_ANY_TAG, MPI_COMM_WORLD, &status);
////	puts(message_buff);
//	//TODO 初始化数据服务器的状态
//	msg_queue = alloc_msg_queue();
//	assert(msg_queue != NULL);
//
//	pthread_mutex_init(&mutex_message_buff, NULL);
//	pthread_mutex_init(&mutex_namespace, NULL);
//
//	pthread_create(&thread_master_server, NULL, master_server, NULL);
//	pthread_create(&thread_request_handler, NULL, request_handler, NULL);
//
//	pthread_join(thread_master_server, NULL);
//	pthread_join(thread_request_handler, NULL);
//
//	pthread_cond_init(&cond_request_queue, NULL);
//	//puts("=====master initialize end=====");
//}

/**
 *	master server
 *	接收消息，加入队列
 */
void* master_server(void *arg) {
	log_info("master server start");
	MPI_Status status;
	init_queue();
	while (1) {
		pthread_mutex_lock(&mutex_message_buff);
		log_info("receive message start");
		MPI_Recv(receive_buf, MAX_CMD_MSG_LEN, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		log_info("receive message end");
		//TODO...
		//need to catch status error

		common_msg_t *reqeust = malloc_request(message_buff, MAX_CMD_MSG_LEN, &status);
		pthread_mutex_lock(&mutex_request_queue);
		in_queue(reqeust);
		log_info("signal the thread waiting request queue start");
		pthread_cond_signal(&cond_request_queue);
		pthread_mutex_unlock(&mutex_request_queue);
		pthread_mutex_unlock(&mutex_message_buff);
		log_info("signal the thread waiting request queue end");
	}
	pthread_cond_destroy(&cond_request_queue);
	return 0;
}

void *free_file_location_des(file_location_des *f){
	file_machine_location *ff = f->machines_head;
	file_machine_location *next;
	while(ff){
		next = ff->next;
		free(ff);
		ff = next;
	}
	return 0;
}

//给文件分配空间
file_location_des *maclloc_data_block(unsigned long file_size) {
	file_location_des * t = (file_location_des*) malloc(sizeof(file_location_des));
	unsigned long block_count = ceil((double)file_size / BLOCK_SIZE);
	t->machinde_count = 0;
	if(block_count == 0)
		return t;
	data_server_des *d = master_data_servers->data_server_list;
	t->machines_head = (file_machine_location*)malloc(sizeof(file_machine_location));
	t->machines_head->next = NULL;
	t->machines_tail = t->machines_head;
	unsigned int file_seq = 1;
	//遍历序列
	int search_index = 1;
	//TODO设置全局的块ID
	while(block_count){
		if(search_index <= master_data_servers->server_count && d->status){
			t->machines_tail->machinde_id = d->id;
			if(block_count <= d->s_free_blocks_count){
				t->machines_tail->count =  block_count;
				d->s_free_blocks_count -= block_count;
				//TODO分配全局ID
				break;
			}else{
				t->machines_tail->count = d->s_free_blocks_count;
				d->s_free_blocks_count = 0;
				block_count -= d->s_free_blocks_count;
				t->machines_tail->next = (file_machine_location*)malloc(sizeof(file_machine_location));
				t->machines_tail = t->machines_tail->next;
				t->machines_tail->next = NULL;
				d++;
			}
		}
	}
	if(block_count){
		free_file_location_des(t);
		return NULL;
	}
	return t;
}

void answer_client_create_file(request_node *request){
	create_file_structure *create = request->message;
	int i;
	file_location_des *file_location = maclloc_data_block(create->file_size);

	//there is not enough space to storage the file
	if(!file_location){
		MPI_Send(0, 1, MPI_INT, request->status->MPI_SOURCE, CLIENT_INSTRUCTION_ANS_MESSAGE_TAG, MPI_COMM_WORLD);
		return;
	}

	//allocate space success
	MPI_Send(1, 1, MPI_INT, request->status->MPI_SOURCE, CLIENT_INSTRUCTION_ANS_MESSAGE_TAG, MPI_COMM_WORLD);

	for(i = 0; i != file_location->machinde_count; i++){
		//TODO 发送创建文件信息
	}
	free_request_node(request);
}

void* request_handler(void *arg) {
	request_node *request;
	while (1) {
		pthread_mutex_lock(&mutex_request_queue);
		log_info("dequeue loop start");
		if (request_is_empty())
			pthread_cond_wait(&cond_request_queue, &mutex_request_queue);
		request = de_queue();
		log_info("dequeue loop end");
		pthread_mutex_unlock(&mutex_request_queue);
		if (request != NULL) {
			unsigned short *operation_code = request->message;
			if (*operation_code == CREATE_FILE_CODE) {
				answer_client_create_file(request);
			}
		}
	}
	return 0;
}
