/*
 * Created on 2015.1.23
 * Author:binyang
 *
 * This file will finish all data server's work
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "dataserver.h"
#include "../structure/vfs_structure.h"
#include "../../tool/errinfo.h"

//this is a demo, there are many things to add

//many kinds of locks
pthread_rwlock_t msg_queue_rw_lock;

data_server_t* data_server;

int get_current_imformation(data_server_t * server_imf)
{
	FILE* fp;
	char temp[100];
	char *parameter;

	fp = fopen("/proc/meminfo", "r");
	if(fp == NULL)
	{
		perror("can not get memory information");
		return 1;
	}

	fgets(temp, 100, fp);
	strtok(temp, " ");//分割字符串
	parameter = strtok(NULL, " ");
	server_imf->d_memory_used = strtoul(parameter, NULL, 10);

	fgets(temp, 100, fp);
	strtok(temp, " ");
	parameter = strtok(NULL, " ");
	server_imf->d_memory_free = strtoul(parameter, NULL, 10);
	server_imf->d_memory_used -= server_imf->d_memory_free;

	fclose(fp);
	return 0;
}

data_server_t* init_dataserver(total_size_t t_size, int dev_num)
{
	data_server = (data_server_t* )malloc(sizeof(data_server_t));
	if(data_server == NULL)
		err_sys("error while allocate data server");

	if( (data_server->d_super_block = vfs_init(t_size, dev_num)) == NULL )
		err_quit("error in init_dataserver function");

	//get_current_imformation(data_server)
	//It's just a joke, do not be serious
	data_server->files_buffer = (dataserver_file_t* )malloc(sizeof(dataserver_file_t)
			* D_FILE_BSIZE);

	//init f_arr_buff
	data_server->f_arr_buff = (vfs_hashtable_t* )malloc(sizeof(vfs_hashtable_t));
	data_server->f_arr_buff->hash_table_size = D_PAIR_BSIZE;
	data_server->f_arr_buff->blocks_arr = (unsigned int* )malloc(sizeof(unsigned int)
			* D_PAIR_BSIZE);
	data_server->f_arr_buff->chunks_arr = (unsigned long long*)malloc(sizeof(unsigned long long)
			* D_PAIR_BSIZE);

	data_server->m_data_buffer = (char* )malloc(sizeof(char) * D_DATA_BSIZE
			* MAX_DATA_MSG_LEN);
	//init threads pool
	//data_server->t_buffer = (pthread_t* )malloc(sizeof(pthread_t) * D_THREAD_SIZE);

	//init msg_cmd_buffer
	data_server->m_cmd_queue = (msg_queue_t* )malloc(sizeof(msg_queue_t));
	data_server->m_cmd_queue->msg = (common_msg_t* )malloc(sizeof(common_msg_t)
			* D_MSG_BSIZE);
	data_server->m_cmd_queue->current_size = 0;
	data_server->m_cmd_queue->head_pos = 0;
	data_server->m_cmd_queue->tail_pos = 0;

	//init many kinds of locks
	if(pthread_rwlock_init(&msg_queue_rw_lock, NULL) != 0)
			err_sys("init pthread lock wrong");
	return data_server;
	//end of init
}
