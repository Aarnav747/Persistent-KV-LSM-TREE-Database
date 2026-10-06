#include "node.h"

int pick_layercnt() /* a function that is used to select the no. of layers for each node */ {
	int rand_ret = rand();

	int layer_limit = (RAND_MAX / 2);
	int layer_cnt = 0; // layer 0 corresponds to the first layer, which is the linked list itself

	if (rand_ret <= layer_limit) {
		while (rand_ret < layer_limit) {
			if (layer_cnt == MAX_LAYER_LEVEL) break;

			layer_cnt++;
			rand_ret = rand();
        	}
	} else {
		while (rand_ret > layer_limit) {
			if (layer_cnt == MAX_LAYER_LEVEL) break;

 			layer_cnt++;
			rand_ret = rand();
 		}
	}

	return layer_cnt;
}


node* create_node(char key[], size_t key_len, char val[], size_t val_len, int io_type, arena* arena_mem /* arena parameter instead of mmt_inst, because using mmt_inst as a parameter in node files, would create a circular dependency */) {
	int layer_cnt = pick_layercnt();

	node* cur_node = (node*)alloc_mem(arena_mem, (sizeof(node) + key_len + val_len + (layer_cnt * sizeof(node*))), _Alignof(node) /* align 8 bytes */);

	if (cur_node == NULL) return NULL;

	cur_node->layer_cnt = layer_cnt;
	cur_node->key_len = key_len;
	cur_node->val_len = val_len;
	cur_node->del_status = io_type;

	cur_node->buff_ptr = cur_node->buffer + (layer_cnt * sizeof(node*));

	memcpy(cur_node->buff_ptr, key, key_len);

	if (val_len > 0) {
		memcpy((cur_node->buff_ptr + key_len), val, val_len);
    	}

	cur_node->next_ptrs = (node**)cur_node->buffer;
	memset(cur_node->next_ptrs, 0, layer_cnt * sizeof(node*));

	return cur_node;
}
