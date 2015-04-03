/**
 * created on: 2015.3.29
 * author: Binyang, ron
 *
 * This file define some basic structure of message type.
 * messge operation code should be add in global.h
 */

#ifndef SRC_TOOL_MESSAGE_H_
#define SRC_TOOL_MESSAGE_H_

#include <stdio.h>
#include "../global.h"

#define MAX_COUNT_CID_R ((MAX_COM_MSG_LEN - 16) / 8) //max length of chunks id array in read message
#define MAX_COUNT_CID_W ((MAX_COM_MSG_LEN - 16) / 8) //max length of chunks id array in write message
#define MAX_COUNT_DATA  ((MAX_DATA_MSG_LEN - 16) / 8) //max count of data in one message package

/*
 *
 */
#define CLIENT_MASTER_MESSAGE_SIZE 4096
#define CLIENT_MASTER_MESSAGE_CONTENT_SIZE 3072
#define MASTER_ANSWER_CLIENT_BLOCK_SIZE 1024

/*
 * communicate tag
 */
#define CLIENT_INSTRCTION_MESSAGE 400
#define CLIENT_INSTRUCTION_ANS_MESSAGE 401

/*
 * operation code
 */
#define CREATE_FILE_CODE 3001

/*
 * structure instruction of create file
 */
typedef struct{
	unsigned short instruction_code;
	long file_size;
	char file_name[CLIENT_MASTER_MESSAGE_CONTENT_SIZE];
}create_file_structure;

/**
 * master answer of client create file
 */
typedef struct{
	unsigned short ans_code;
	unsigned char is_tail;
	unsigned int seq_num;
	int machine_rank;
	int block_count;
	unsigned long long block_global_num[MASTER_ANSWER_CLIENT_BLOCK_SIZE];
}create_file_ans_structure;


/**
 *Following structure defined message structure between client and data server
 *there are many things should be taken a attention.
 *
 *First:  client may be communicate with many users, and a user may be invoke many file options,
 *		  such as open many files. client should maintain user's conditions, like how many files
 *		  has opened so user can use RPC to communicate with client.
 *
 *Second: each operation to a certain file on client side will invoke a thread on data server side,
 *        that is to say for every read or write operation, data server will use a thread to handle it.
 *        data server will not maintain a connection will a client, so a thread will not be blocked
 *        if there are no new command be received.
 *
 *Third:  open and close operation will have no affection on data server, it only has affect on client,
 *        and client should store right information for users.
 */

//open operation from client to data server
//connect based operations
struct open_to_c
{
	//64 bytes
	unsigned short operation;//it is set to a certain code
	unsigned short fd;//the number of file describe, may be a 1 byte is enough
};

//read message from client to data server
//before you send this, you must sure that buffer is ready for read on client side
//read function may be like this : read(tag, offset, length, chunks_count, *chunks_arr)
struct read_c_to_d
{
	//64 bytes
	unsigned short operations;
	unsigned short unique_tag;//client must promise to offer a special tag to data server, you can use bitmap
	unsigned int offset;//offset from the begin of the first chunk that should be read
	//64bytes

	unsigned int read_len;//the length will read, just send to one data server
	unsigned int chunks_count;//the numbers of chunks will be read
	//64bytes aligned
	unsigned long long chunks_id_arr[MAX_COUNT_CID_R];
};

//return accept message when you are ready to receive data message
struct acc_d_to_c
{
	unsigned short operation_code;
	unsigned short status;//if the data server is OK to return data, 0 is fine

	unsigned int reserved;
};

//read and write use this message
struct msg_data
{
	unsigned int len;//data len in this package
	unsigned int offset;//form the beginning chunks

	unsigned int seqno;//number of this read
	char tail;//if tail
	char reserved[3];

	unsigned long long data[MAX_COUNT_DATA];
};

//origin write operation, do not support insert
struct write_c_to_d
{
	//64 bytes
	unsigned short operation_code;
	unsigned short unique_tag;
	unsigned int offset;
	//64 bytes
	unsigned int write_len;
	unsigned int chunks_count;
	//64 bytes aligned
	unsigned long long chunks_id_arr[MAX_COUNT_CID_W];
};

//you can use this structure to insert text to a file
//if a message can not contain enough block_ids, you can send a serials messages
// and the seqno and tail is useful for this condition.
struct write_c_to_d_ins
{
	unsigned short operation_code;
	unsigned short unique_tag;

	unsigned int seqno;
	char tail;
	//...
};

//close(fd) function will sent this structure to data server
struct close_to_c
{
	unsigned short operation_code;
	unsigned short fd;
};

#endif
