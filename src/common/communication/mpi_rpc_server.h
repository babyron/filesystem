/*
 * mpi_rpc_server.h
 *
 *  Created on: 2015年7月10日
 *      Author: ron
 */

#ifndef SRC_COMMON_COMMUNICATION_MPI_RPC_SERVER_H_
#define SRC_COMMON_COMMUNICATION_MPI_RPC_SERVER_H_

#include <stdint.h>
#include <mpi.h>
#include "mpi_rpc_structure.h"
#include "../sds.h"
#include "../syn_tool.h"
#include "../basic_list.h"
#include "../../global.h"
#include "../threadpool.h"

struct mpi_rpc_server {
	syn_queue_t *request_queue;
	MPI_Comm comm;
	MPI_Status status;
	int rank;
	int server_thread_cancel;
	void *recv_buff;
	struct mpi_rpc_server_op *op;
	thread_pool_t *thread_pool;
};

struct mpi_rpc_server_op {
	void (*server_start)(struct mpi_rpc_server *server);
	void (*server_end)(struct mpi_rpc_server *server);
	void (*send_result)(void **param);
};

typedef struct mpi_rpc_server mpi_rpc_server_t;
typedef struct mpi_rpc_server_op mpi_rpc_server_op_t;

mpi_rpc_server_t *create_mpi_rpc_server(int thread_num, int rank, void *(*resolve_handler)(event_handler_t *event_handler, void* msg_queue));
void destroy_mpi_rpc_server(mpi_rpc_server_t *server);

#endif /* SRC_COMMON_COMMUNICATION_MPI_RPC_SERVER_H_ */