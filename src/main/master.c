/*
 * master.c
 *
 *  Created on: 2015年1月4日
 *      Author: ron
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <mpi.h>
#include <assert.h>
#include "master.h"
#include "./namespace/namespace.h"
#include "./data_server/master_data_servers.h"
#include "../structure/basic_queue.h"
#include "../structure/namespace_code.h"
#include "../tool/message.h"
#include "../global.h"

/*====================Private Prototypes====================*/
static namespace *master_namespace;
static basic_queue_t *message_queue;
static data_servers *master_data_servers;

static pthread_t *pthread_request_listener;
static pthread_t *pthread_request_handler;
static pthread_t *pthread_damon_handler;
static pthread_mutex_t *mutex_message_queue;
static pthread_cond_t *cond_message_queue;

static char *receive_buf;
static char *send_buf;
static common_msg_t *msg_buff;
static common_msg_t *msg_pop_buff;
static common_msg_t *request_handle_buff;

static void set_common_msg(common_msg_t *msg, int source, char *message);
static void *master_server();

static int master_create_file(char *file_name, unsigned long file_size);

/*====================private functions====================*/
static void mpi_status_assignment(MPI_Status *status, MPI_Status *s) {
	memcpy(status, s, sizeof(MPI_Status));
}

static void set_common_msg(common_msg_t *msg, int source, char *message){
	unsigned short *operation_code = (unsigned short*)message;
	msg->operation_code = *operation_code;
	msg->source = source;
	strncpy(msg->rest, message, MAX_CMD_MSG_LEN);
}

static int queue_push_msg(basic_queue_t *queue, int source, char *message){
	set_common_msg(msg_buff, source, receive_buf);
	queue->basic_queue_op->push(queue, msg_buff);
}

/**
 *	master server
 *	receive message and put message into the message queue
 */
static void* master_server(void *arg) {
	MPI_Status status;
	while (1) {
		MPI_Recv(receive_buf, MAX_CMD_MSG_LEN, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		//TODO need to catch status error

		pthread_mutex_lock(mutex_message_queue);
		queue_push_msg(message_queue, status.MPI_SOURCE, receive_buf);
		pthread_cond_signal(cond_message_queue);
		pthread_mutex_unlock(mutex_message_queue);
	}
	pthread_cond_destroy(cond_message_queue);
	return 0;
}

static void* request_handler(void *arg) {
	while (1) {
		pthread_mutex_lock(mutex_message_queue);
		while (message_queue->basic_queue_op->is_empty(message_queue))
		{
			pthread_cond_wait(cond_message_queue, mutex_message_queue);
		}
		message_queue->basic_queue_op->pop(message_queue, msg_pop_buff);
		pthread_mutex_unlock(mutex_message_queue);
		if (msg_pop_buff != NULL)
		{
			unsigned short operation_code = msg_pop_buff->operation_code;
//			if (*operation_code == CREATE_FILE_CODE) {
//				answer_client_create_file(request);
//			}
		}
	}
	return 0;
}

static int answer_client_create_file(common_msg_t *request){
	client_create_file *file_request = (client_create_file *)(request->rest);
	//TODO the first is not going to provide any fault tolerance, but the name space modify should be temporary whenever is not confirmed
	int status = namespace_create_file(master_namespace , file_request->file_name);
	if(status != OPERATE_SECCESS){
		return status;
	}

	basic_queue_t *queue = master_data_servers->opera->file_allocate_machine(master_data_servers, file_request->file_size, BLOCK_SIZE);
	int malloc_result = (queue == NULL ? 0 : 1);
	//TODO if communicate error
	MPI_Send(&malloc_result, 1, MPI_INT, request->source, CLIENT_INSTRUCTION_ANS_MESSAGE_TAG, MPI_COMM_WORLD);
	if(malloc_result == 0){
		return -1;
	}

	ans_client_create_file *ans = (ans_client_create_file *)malloc(sizeof(ans_client_create_file));
	int ans_message_size = ceil(queue->current_size / LOCATION_MAX_BLOCK);
	int i;
	for(i = 1; i <= ans_message_size; i++){
		if(i != ans_message_size){
			ans->is_tail = 0;
			ans->block_num = LOCATION_MAX_BLOCK;
		}else{
			ans->is_tail = 1;
			ans->block_num = queue->current_size - (i - 1) * LOCATION_MAX_BLOCK;
		}
		//TODO need to generate a unique id
		//ans->generated_id
		ans->operation_code = CREATE_FILE_ANS_CODE;
		memcpy(ans->block_global_num, queue->elements + (i - 1) * LOCATION_MAX_BLOCK * queue->element_size, ans->block_num * queue->element_size);
		MPI_Send(ans, sizeof(ans_client_create_file), MPI_CHAR, request->source, CLIENT_INSTRUCTION_ANS_MESSAGE_TAG, MPI_COMM_WORLD);
	}
	free(ans);
	return 0;
}

/*====================API Implementation====================*/
//TODO when shutdown the application, it must release all the memory resource
int master_init(){
	/*allocate necessary memory*/
	master_namespace = create_namespace(1024, 32);
	message_queue = alloc_basic_queue(sizeof(common_msg_t), -1);
	master_data_servers = data_servers_create(1024, 0.75, 10);
	queue_set_free(message_queue, common_msg_free);
	queue_set_dup(message_queue, common_msg_dup);

	pthread_request_listener = (pthread_t *)malloc(sizeof(pthread_t));
	pthread_request_handler = (pthread_t *)malloc(sizeof(pthread_t));
	mutex_message_queue = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	cond_message_queue = (pthread_cond_t *)malloc(sizeof(cond_message_queue));

	receive_buf = (char *)malloc(MAX_CMD_MSG_LEN);
	send_buf = (char *)malloc(MAX_CMD_MSG_LEN);
	msg_buff = (common_msg_t *)malloc(sizeof(common_msg_t));
	msg_pop_buff = (common_msg_t *)malloc(sizeof(common_msg_t));
	request_handle_buff = (common_msg_t *)malloc(sizeof(common_msg_t));
	if(master_namespace == NULL || message_queue == NULL || pthread_request_listener == NULL
			|| pthread_request_handler == NULL || mutex_message_queue == NULL){
		master_destroy();
		return -1;
	}

	/*initialize server thread and critical resource lock*/
	pthread_cond_init(cond_message_queue, NULL);
	pthread_mutex_init(mutex_message_queue, NULL);
	pthread_create(pthread_request_listener, NULL, master_server, NULL);
	pthread_create(pthread_request_handler, NULL, request_handler, NULL);
	pthread_join(*pthread_request_listener, NULL);
	pthread_join(*pthread_request_handler, NULL);
}



//TODO dangerous operation, when shutdown the application
//TODO self-defined structure may be released by it's own free function
int master_destroy()
{
	free(master_namespace);
	free(message_queue);
	free(pthread_request_listener);
	free(pthread_request_handler);
	free(mutex_message_queue);
	data_servers_destroy();

}
