#ifndef NODE_H
#define NODE_H

#include "arena.h"
#include "memtable.h"
#include "tags.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct memtable_struct mmt_inst;

typedef struct skip_list_node_struct node;

struct skip_list_node_struct {
	int layer_cnt; // number of total layers this node appears in
	char* buff_ptr; // skips past the pointer array in buffer and points to where the key bytes begin
	node** next_ptrs; // points to the buffer's pointer array part. the pointer array size is (layer_cnt * sizeof(node*))
	size_t key_len;
	size_t val_len;
	int del_status; // shows the current status of the node, could be either active (0 / IO_TYPE_SET_PUT) or deleted (1 / IO_TYPE_SET_DEL)
	_Alignas(node*) char buffer[]; // stores key and val bytes, aswell as the 'next' pointers of different layers. buffer layout: [pointer array][key bytes][value bytes]
};

int pick_layer(void);

node* create_node(char key[], size_t key_len, char val[], size_t val_len, int io_type, arena* arena_mem);

#endif
