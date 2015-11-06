/*
 * list_queue_util.c
 *
 *  Created on: 2015年10月12日
 *      Author: ron
 */
#include "list_queue_util.h"

void *list_to_array(list_t *list, int size) {
	position_des_t *pos = zmalloc(size * list->len);
	int index = 0;
	list_iter_t *iter = list->list_ops->list_get_iterator(list, AL_START_HEAD);
	while (list->list_ops->list_has_next(iter)) {
		position_des_t *p = ((list_node_t *) list->list_ops->list_next(iter))->value;
		memcpy(pos + index, p, size);
		index++;
	}
	list->list_ops->list_release_iterator(iter);
	return pos;
}

basic_queue_t *list_to_queue(list_t *list, size_t size) {
	if (list == NULL || size <= 0) {
		return NULL;
	}

	basic_queue_t *queue = alloc_basic_queue(list->len, size);

	queue->dup = list->dup;
	queue->free = list->free;
	list_iter_t *iter = list->list_ops->list_get_iterator(list, AL_START_HEAD);
	while (list->list_ops->list_has_next(iter)) {
		queue->basic_queue_op->push(queue,
				(list_node_t*) list->list_ops->list_next(iter)->value);
	}

	return queue;
}
