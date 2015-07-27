/*
 * mpi_rpc.c
 *
 *  Created on: 2015年7月10日
 *      Author: ron
 */

#include <math.h>
#include "mpi_rpc_server.h"
#include "../structure_tool/zmalloc.h"
#include "../structure_tool/syn_tool.h"
#include "../structure_tool/log.h"
#include "message.h"

//#define RPC_SERVER_DEBUG 1

/*--------------------Private Declaration---------------------*/
static void server_start(mpi_rpc_server_t *server);
static void server_stop(mpi_rpc_server_t *server);
static int send_result(void *param, int dst, int tag, int len, reply_msg_type_t type);

/*--------------------Global Declaration----------------------*/
mpi_rpc_server_t *create_mpi_rpc_server(int thread_num, int rank,
		resolve_handler_t resolve_handler);
void destroy_mpi_rpc_server(mpi_rpc_server_t *server);

/*--------------------Private Implementation------------------*/
static void server_start(mpi_rpc_server_t *server)
{
	server->thread_pool->tp_ops->start(server->thread_pool);

#if defined(RPC_SERVER_DEBUG)
	puts("RPC_SERVER && THREAD POOL START");
#endif

	//TODO multi_thread access server_thread_cancel may read error status
	while(!server->server_thread_cancel) {
		//TODO if multi_client send request to this, Is there any parallel problem ?
		recv_common_msg(server->recv_buff, MPI_ANY_SOURCE, MPI_ANY_TAG);
#if defined(RPC_SERVER_DEBUG)
		puts("RPC_SERVER PUT MESSAGE");
#endif
		server->request_queue->op->syn_queue_push(server->request_queue, server->recv_buff);
	}
}

static void server_stop(mpi_rpc_server_t *server) {
	server->server_thread_cancel = 1;
}

static int send_result(void *param, int dst, int tag, int len, 
		reply_msg_type_t type)
{
	acc_msg_t acc_msg;
	head_msg_t head_msg;

	switch(type)
	{
		case ACC:
			send_acc_msg(param, dst, tag);
			break;
		case DATA:
			send_data_msg(param, dst, tag);
			break;
		case ANS:
			head_msg.op_status = ACC_OK;
			send_head_msg(&head_msg, dst, tag);
			recv_acc_msg(&acc_msg, dst, tag);
			if(acc_msg.op_status == ACC_OK)
				send_msg(param, dst, tag, head->len);
			else
			{
				log_write(LOG_WARN, "client can not receiving message");
				zfree(head);
				return -1;
			}
			break;
		case CMD:
			send_cmd_msg(param, dst, tag);
			break;
		case HEAD:
			send_head_msg(msg, dst, tag);
			break;
		default:
			log_write(LOG_ERR, "wrong type of message, check your code");
			zfree(head);
			return -1;
	}
	return 0;
}

/*--------------------API Implementation--------------------*/
mpi_rpc_server_t *create_mpi_rpc_server(int thread_num, int server_id,
		resolve_handler_t resolve_handler)
{
	mpi_rpc_server_t *this = zmalloc(sizeof(mpi_rpc_server_t));

	this->request_queue = alloc_syn_queue(128, sizeof(common_msg_t));
	this->server_id = server_id;
	this->server_thread_cancel = 0;
	this->thread_pool = alloc_thread_pool(thread_num, this->request_queue, resolve_handler);

	this->op = zmalloc(sizeof(mpi_rpc_server_op_t));
	this->op->server_start = server_start;
	this->op->server_stop = server_stop;
	this->op->send_result = send_result;

	this->recv_buff = zmalloc(sizeof(common_msg_t));
	return this;
}

void destroy_mpi_rpc_server(mpi_rpc_server_t *server)
{
	server_stop(server);
	destroy_syn_queue(server->request_queue);
	destroy_thread_pool(server->thread_pool);
	zfree(server->op);
	zfree(server->recv_buff);
	zfree(server);
}
