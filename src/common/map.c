/*
 * map.c
 *
 *  Created on: 2015年7月2日
 *      Author: ron
 */

#include <stdio.h>
#include "map.h"
#include "zmalloc.h"

/* Copy from redis
 * MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static uint32_t map_gen_hash_function(const sds key, size_t size) {
	size_t len = sds_len(key);
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = 5381;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
    	case 3: h ^= data[2] << 16;
    			h ^= data[1] << 8;
    			h ^= data[0]; h *= m;
    			break;
    	case 2: h ^= data[1] << 8;
    			h ^= data[0]; h *= m;
    			break;
    	case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (uint32_t)h % size;
}

static pair_t *find(map_t *this, const sds key){
	uint32_t h = map_gen_hash_function(key, this->size);

	//printf("hey hey this = %d\n", h);
	list_t *l = *(this->list + h);

	list_iter_t *iter = l->list_ops->list_get_iterator(l, AL_START_HEAD);
	list_node_t *node = l->list_ops->list_next(iter);
	while(node) {
		if(sds_cmp(((pair_t *)node->value)->key, key) == 0) {
			return (pair_t *)node->value;
		}
		node = l->list_ops->list_next(iter);
	}

	return NULL;
}

static int put(map_t *this, const sds key, void *value) {
	pair_t *node = find(this, key);

	if(node == NULL) {
		uint32_t h = map_gen_hash_function(key, this->size);
		list_t *l = *(this->list + h);

		pair_t *pair = (pair_t *)zmalloc(sizeof(pair_t));
		pair->key = this->key_dup(key);
		pair->value = this->value_dup ? this->value_dup(value) : value;

		l->list_ops->list_add_node_tail(l, (void *)pair);
		this->current_size++;
		return 1;
	}else {
		if(this->value_free) {
			this->value_free(node->value);
		}else {
			zfree(node->value);
		}
		node->value = this->value_dup ? this->value_dup(value) : value;
		return 0;
	}
}

static void *get(map_t *this, const sds key) {
	pair_t *node = find(this, key);

	return node == NULL ? NULL : (this->value_dup ? this->value_dup(node->value) : node->value);
}

static int contains(map_t *this, sds key) {
	return find(this, key) != NULL;
}

static size_t get_size(map_t *this) {
	return this->current_size;
}

void del(map_t *this, sds key){
	uint32_t h = map_gen_hash_function(key, this->size);

	list_t *l = *(this->list + h);
	list_iter_t *iter = l->list_ops->list_get_iterator(l, AL_START_HEAD);
	list_node_t *node = l->list_ops->list_next(iter);
	while(node) {
		if(sds_cmp(((pair_t *)node->value)->key, key) == 0) {
			l->list_ops->list_del_node(l, node);
			this->current_size--;
			return;
		}
		node = l->list_ops->list_next(iter);
	}
}

map_t *create_map(size_t size, void *(*value_dup)(const void *value), void (*value_free)(void *value),
		void *(*list_dup)(void *ptr), void (*list_free)(void *ptr)) {
	int i;

	map_t *this = (map_t *)(zmalloc(sizeof(map_t)));
	this->op = zmalloc(sizeof(map_op_t));
	this->list = (list_t **)zmalloc(sizeof(list_t *) * size);

	for(i = 0; i < size; i++) {
		*(this->list + i) = list_create();
		(*(this->list + i))->dup = list_dup;
		(*(this->list + i))->free = list_free;
	}

	this->current_size = 0;
	this->size = size;
	this->op->put = put;
	this->op->get = get;
	this->op->del = del;
	this->op->contains = contains;
	this->op->get_size = get_size;

	this->key_dup = sds_dup;
	this->key_free = sds_free;
	this->value_dup = value_dup;
	this->value_free = value_free;

	return this;
}

void destroy_map(map_t *this) {
	zfree(this->op);
	int i;
	for(i = 0; i < this->size; i++) {
		list_release(*(this->list + i));
	}
	zfree(this);
}

//UNIT TEST
//#define MAP_TEST 1
#if  defined(GLOBAL_TEST) || defined(MAP_TEST)
void *pair_dup(void *pair){
	pair_t *p = zmalloc(sizeof(pair_t));
	p->key = sds_dup(((pair_t *)pair)->key);
	int *t = zmalloc(sizeof(int));
	*t = *((int *)((pair_t *)pair)->key);
	p->value = t;
	return (void *)p;
}

void pair_free(void *pair){
	pair_t *p = pair;
	sds_free(p->key);
	zfree(p->value);
	zfree(p);
}

void v_free(void *v) {
	int *t = v;
	zfree(t);
}
void *v_dup(const void *v){
	int *t = zmalloc(sizeof(int));
	*t = *((int *)v);
	return (void *)t;
}

int main() {
	//if v_dup or v_free is NULL, it will be shallow copy, make your copy function right
	map_t *map = create_map(10, v_dup, v_free, pair_dup, pair_free);

	//put
	sds s = sds_new("1234");
	int v = 1234;
	map->op->put(map, s, &v);
	int *t = map->op->get(map, s);
	printf("the value we get = %d\n", *t);

	//modify
	v = 2345;
	map->op->put(map, s, &v);
	t = map->op->get(map, s);
	printf("the value we get = %d\n", *t);

	//delete
	map->op->del(map, s);
	t = map->op->get(map, s);
	if(t == NULL) {
		puts("NONE");
	}else {
		printf("the value we get = %d\n", *t);
	}

	destroy_map(map);
	return 0;
}
#endif