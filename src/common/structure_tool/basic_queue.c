#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "basic_queue.h"

/*==============================PRIVATE VARIABLES========================*/
static const int default_capacity = 1 << 8;
//static const double load_factor = 0.75;

/*==============================PRIVATE PROTOTYPES=======================*/
static int cal_new_length(int length);
static void copy_queue_content(void *new_elements, basic_queue_t* this);
static int msg_queue_resize(basic_queue_t* this);

static int has_next(basic_queue_iterator *);
static void next(basic_queue_iterator *, void *dest);

static int basic_queue_push(basic_queue_t*, void*);
static void basic_queue_pop(basic_queue_t*, void*);
static int is_empty(basic_queue_t*);
static int is_full(basic_queue_t*);

/*==============================MESSAGE QUEUE=============================*/

/*calculate the new length of queue*/
static int cal_new_length(int length)
{
	int new_length = 1;
	while (new_length <= length)
	{
		new_length = new_length << 1;
	}
	return new_length;
}

/*copy old elements to the new elements, TODO need to test*/
static void copy_queue_content(void *new_elements, basic_queue_t* this)
{
	if(this->head_pos < this->tail_pos)
	{
		memcpy(new_elements, this->elements, this->current_size * this->element_size);
	}else
	{
		int start = this->head_pos * this->element_size;
		int count = (this->queue_len - this->head_pos) * this->element_size;
		memcpy(new_elements, (char*)this->elements + start, count);
		memcpy((char*)new_elements + count, this->elements, this->tail_pos * this->element_size);
	}
}


static int msg_queue_resize(basic_queue_t* this)
{
	int new_length = cal_new_length(this->queue_len);
	void *new_elements = malloc(this->element_size * new_length);
	if (new_elements == NULL)
	{
		return -1;
	}

	copy_queue_content(new_elements, this);

	this->head_pos = 0;
	this->tail_pos = this->current_size;
	free(this->elements);
	this->elements = new_elements;
	this->queue_len = new_length;
	new_elements = NULL;
	return 0;
}

static int has_next(basic_queue_iterator * iterator)
{
	return iterator->it_size != iterator->queue->current_size;
}

static void next(basic_queue_iterator *iterator, void *dest)
{
	iterator->queue->dup(dest, iterator->queue->elements + iterator->offset * iterator->queue->element_size);
	iterator->offset = (iterator->offset + 1) % (iterator->queue->queue_len);
	iterator->it_size++;
}

static int basic_queue_push(basic_queue_t* this, void* element)
{
	int result = 0;
	if(is_full(this))
	{
		result = msg_queue_resize(this);
		if(result == -1)
		{
			return result;
		}
	}
	assert(this->current_size != this->queue_len);

	if(this->dup)
	{
		this->dup(this->elements + this->tail_pos * this->element_size, element);
	}
	else
	{
		memcpy(this->elements+this->tail_pos * this->element_size, element, this->element_size);
	}
	this->tail_pos = (this->tail_pos + 1) % this->queue_len;
	this->current_size++;
	return result;
}

static void basic_queue_pop(basic_queue_t* this, void* element)
{
	int offset;
	if (is_empty(this))
	{
		//TODO Does this works ?
		return;
	}
	offset = this->head_pos;
	if(this->dup)
		this->dup(element, this->elements + offset * this->element_size);
	else
		memcpy(element, this->elements + offset * this->element_size, this->element_size);
	this->head_pos = (this->head_pos + 1) % this->queue_len;
	this->current_size--;
}

static int is_empty(basic_queue_t* this)
{
	return this->current_size == 0;
}

static int is_full(basic_queue_t* this)
{
	return this->queue_len == this->current_size;
}

basic_queue_iterator *create_basic_queue_iterator(basic_queue_t *queue)
{
	if(queue == NULL){
		return NULL;
	}
	basic_queue_iterator * iterator = (basic_queue_iterator *)malloc(sizeof(basic_queue_iterator));
	iterator->queue = queue;
	iterator->offset = queue->head_pos;
	iterator->has_next = has_next;
	iterator->next = next;
	iterator->it_size = 0;
	return iterator;
}

basic_queue_t* alloc_basic_queue(int queue_len, int type_size)
{
	basic_queue_t* this;
	this = (basic_queue_t*) malloc(sizeof(basic_queue_t));
	if (this == NULL)
	{
		//err_ret("error in alloc_msg_queue");
		return NULL;
	}

	this->head_pos = 0;
	this->tail_pos = 0;
	this->current_size = 0;
	this->queue_len = queue_len > 0 ? queue_len : default_capacity;
	this->element_size = type_size;
	this->elements = (void*) malloc(type_size * this->queue_len);
	if (this->elements == NULL)
	{
		free(this);
		return NULL;
	}

	//initial queue operations
	this->basic_queue_op = (basic_queue_op_t*) malloc(sizeof(basic_queue_op_t));
	if (this->basic_queue_op == NULL)
	{
		free(this->elements);
		free(this);
		return NULL;
	}

	this->basic_queue_op->push = basic_queue_push;
	this->basic_queue_op->pop = basic_queue_pop;
	this->basic_queue_op->is_empty = is_empty;
	this->basic_queue_op->is_full = is_full;

	this->free = NULL;
	this->dup = NULL;
	return this;
}

void *get_queue_element(basic_queue_t* this, int index){
	if(this == NULL){
		return NULL;
	}

	return this->elements + index * this->element_size;
}

void set_queue_element(basic_queue_t* this, int index, void *arg){
	if(this == NULL || arg == NULL){
		return;
	}

	this->dup(this->elements + index * this->element_size, arg);
}

void destroy_basic_queue(basic_queue_t* this)
{
	int i;
	if(this == NULL)
	{
		return;
	}

	this->basic_queue_op->push = NULL;
	this->basic_queue_op->pop = NULL;

	if(this->free){
		for(i = 0; i < this->queue_len; i++)
			this->free(this->elements + i * this->element_size);
	}

	free(this->elements);
	free(this->basic_queue_op);
	free(this);
}

void basic_queue_reset(basic_queue_t* this){
	this->current_size = 0;
	this->tail_pos = 0;
	this->head_pos = 0;
}


#if defined(GLOBAL_TEST) || defined(QUEUE_TEST)
int main() {
	return 0;
}
#endif
