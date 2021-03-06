/*
 * datasever.h
 *
 * Created on 2015.1.23
 * Author:binyang
 */
#ifndef _FILESYSTEM_DATASERVER_H_
#define _FILESYSTEM_DATASERVER_H_

#include <pthread.h>
#include "vfs_structure.h"
#include "message.h"
#include "rpc_server.h"

//buffer size
#define F_ARR_SIZE (1 << 12)
#define BUFF_NODE_SIZE (1 << 8)
#define THREAD_POOL_SIZE 1
#define MSG_QUEUE_SIZE (1 << 7)
#define HEART_FREQ 3

//many kinds of locks

struct data_server_operations
{
	void* (*m_cmd_receive)(void * msg_queue_arg);
	void (*m_resolve)(event_handler_t* event_handler, void* msg_queue);
};

struct data_server
{
	unsigned int d_memory_used;
	unsigned int d_memory_free;
	unsigned int d_memory_used_for_filesystem;
	unsigned int d_memory_free_for_filesystem;
	unsigned int d_network_free;
	unsigned int machine_id;

	dataserver_sb_t* d_super_block;
	struct data_server_operations* d_op;

	//buffers
	basic_queue_t* buff_node_queue;//each thread buffer is construct by buffer node
	basic_queue_t* m_data_buff;//message data buffer
	basic_queue_t *common_msg_buff;//each thread need a common message buffer
	basic_queue_t* file_buff;//each read or write requests need a file information buffer
	basic_queue_t* f_arr_buff;//array that contain maps from global to local
	//all used for f_arr_buff
	unsigned long* f_arr_bitmap;

	//provide thread pool and message passing
	rpc_server_t* rpc_server;
};

typedef struct data_server data_server_t;

//define of some function in struct's operations
//about message receive and resolve
void m_cmd_receive();//there will be a thread run this function
void* m_resolve(event_handler_t* event_handler, void* msg_queue);

//about data server
data_server_t* alloc_dataserver(total_size_t t_size, int dev_num);
int get_current_imformation(data_server_t* server_imf); //返回目前数据节点的信息
void destroy_dataserver();

//this is the ultimate goal!
void *dataserver_run(void *arg);
#endif
