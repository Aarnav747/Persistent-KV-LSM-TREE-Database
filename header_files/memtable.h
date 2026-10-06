#ifndef MEMTABLE_H
#define MEMTABLE_H

#include "node.h"
#include "arena.h"
#include <string.h>
#include <sys/param.h>

typedef struct skip_list_node_struct node;

typedef struct memtable_struct {
	node* headers[MAX_LAYER_LEVEL]; // pointers that help us traverse each of the layer sequentially, because they act as point to the first nodes of each layer
	arena* arena_mem;
} mmt_inst;

void traverse_layers(char key[], size_t key_len, node* past_nodes[], mmt_inst* cur_inst, int stop_before_match);

void insert_node(node* cur_node, mmt_inst* cur_inst);

node* lookup_node(char key[], size_t key_len, mmt_inst* cur_inst);

node* put_node(char key[], size_t key_len, char val[], size_t val_len, int io_type, mmt_inst* cur_inst);

int delete_node(char key[], size_t key_len, mmt_inst* cur_inst);

#endif
