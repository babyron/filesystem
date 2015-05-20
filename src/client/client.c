/*
 * client.c
 *
 *  Created on: 2015年1月4日
 *      Author: ron
 */

/**
 * 1. 监听来自客户的请求
 */
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <mpi.h>
#include "client.h"
#include "../structure/basic_queue.h"
#include "../tool/message.h"
#include "../tool/file_tool.h"
#include "../global.h"
#include "../tool/errinfo.h"

//TODO temporary definition
int master_rank = 0;

/*================private variables===============*/
static char *send_buf;
static char *receive_buf;
static char *file_buf;
static basic_queue_t *message_queue;

/*====================private declarations====================*/
static void send_data(char *file_name, unsigned long file_size, basic_queue_t *location_queue);
static int client_create_file_op(char *file_path, char *file_name);

/*====================private functions====================*/
static void send_data(char *file_name, unsigned long file_size, basic_queue_t *location_queue)
{
	FILE *fp = fopen(file_name, "r");
	int block_num = ceil((double) file_size / BLOCK_SIZE);
	int i = 0, j = 0, read_size;
	for (; i <= block_num - 1; i++)
	{
		if (i != block_num - 1)
		{
			read_size = fread(file_buf, sizeof(char), BLOCK_SIZE, fp);
			fseek(fp, BLOCK_SIZE, BLOCK_SIZE * i);
		} else
		{
			read_size = fread(file_buf, sizeof(char), file_size - (block_num - 1) * BLOCK_SIZE, fp);
//			for(j = 0; j != length - (block_num - 1) * FILE_BLOCK_SIZE; j++)
//				putchar(file_buf[j]);
		}
	}
	fclose(fp);
}

static int client_create_file_op(char *file_path, char *file_name)
{

	long file_length = file_size(file_path);
	if (file_length == -1)
	{
		return -1;
	}

	int result, i;
	char create_file_buff[CLIENT_MASTER_MESSAGE_SIZE];
	int master_malloc_result;

	MPI_Status status;

	client_create_file message;
	message.operation_code = CREATE_FILE_CODE;
	message.file_size = file_length;
	strcpy(message.file_name, file_name);
	memcpy(send_buf, &message, sizeof(client_create_file));

	//TODO not stable
	//MPI_Barrier( MPI_COMM_WORLD);
	MPI_Send(send_buf, MAX_CMD_MSG_LEN, MPI_CHAR, master_rank, CLIENT_INSTRCTION_MESSAGE_TAG, MPI_COMM_WORLD);
	err_ret("client.c: client_create_file_op waiting for allocate answer");
	MPI_Recv(&master_malloc_result, 1, MPI_INT, master_rank, CLIENT_INSTRUCTION_ANS_MESSAGE_TAG, MPI_COMM_WORLD, &status);
	if(master_malloc_result == 0){
		err_ret("client.c: client_create_file_op master allocate new file space failed");
		return -1;
	}
	err_ret("client.c: client_create_file_op waiting for allocate location");
	if(master_malloc_result == 1)
	{
		while(1)
		{
			MPI_Recv(create_file_buff, CLIENT_MASTER_MESSAGE_SIZE, MPI_CHAR, master_rank, CLIENT_INSTRUCTION_ANS_MESSAGE_TAG, MPI_COMM_WORLD, &status);
			ans_client_create_file *ans = (ans_client_create_file *)create_file_buff;
			for(i = 0; i != ans->block_num; i++)
			{
				printf("server_id = %d, block_seq = %ld, global_num = %ld\n", ans->block_global_num[i].server_id, ans->block_global_num[i].block_seq, ans->block_global_num[i].global_id);
			}
		}
	}
	//TODO 如果master成功分配内存，那么此时应该等待接收data server机器信息
	//log_info("client_create_file end");
	err_ret("client.c: client_create_file_op end without error");
	return 0;
}

int client_init() {
	send_buf = (char*) malloc(MAX_CMD_MSG_LEN);
	receive_buf = (char*) malloc(MAX_CMD_MSG_LEN);
	file_buf = (char *)malloc(BLOCK_SIZE);
	message_queue = alloc_basic_queue(sizeof(common_msg_t), -1);
	message_queue->dup = common_msg_dup;
	//message_queue->free = common_msg_free;

	if(send_buf == NULL || receive_buf == NULL || file_buf == NULL || message_queue == NULL){
		client_destroy();
	}

	client_create_file_op("/home/ron/test/read.in", "/readin");
	client_create_file_op("/home/ron/test/read.in", "/readin");
	client_create_file_op("/home/ron/test/read.in", "/readin");
	return 0;
}

int client_destroy(){
	free(send_buf);
	free(receive_buf);
	free(file_buf);
	destroy_basic_queue(message_queue);
}




