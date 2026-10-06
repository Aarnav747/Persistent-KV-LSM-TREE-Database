#include "memtable.h"

void traverse_layers(char key[], size_t key_len, node* past_nodes[], mmt_inst* cur_inst, int stop_before_match /* this parameter helps traversal differentiate between what the caller wants. If it's, say, lookup calling, we most definitely need the same node (if it exists in the list). but if another function is calling, we might instead want to skip whenever iterating at that layer. Otherwise, it could cause problems while unlinking in put_node later on, which is why we added this parameter. */) {
	int memcmp_ret;
	node* cur_node = NULL;
	node* past_node = NULL;
	int len;

	for (int n = (MAX_LAYER_LEVEL - 1); n >= 0; n--) {
		if (past_node == NULL) {
			cur_node = cur_inst->headers[n];
		} else {
			cur_node = past_node->next_ptrs[n];
		}

		while (cur_node != NULL) {
			len = MIN(cur_node->key_len, key_len); /* used MIN() instead of the if/else branch because well, it is more convenient. */

			memcmp_ret = memcmp(cur_node->buff_ptr, key, len);

			if (memcmp_ret == 0) {
				if (cur_node->key_len > key_len) {
					cur_node = NULL;

				} else if (cur_node->key_len == key_len) {
					if (stop_before_match == SEARCH_MODE_UNLINK) {
						cur_node = NULL;
					} else if (stop_before_match == SEARCH_MODE_LOOKUP) {
						past_node = cur_node;
						cur_node = cur_node->next_ptrs[n];
					}

				} else { /* added a check for the condition where cur_node's key length is smaller. without this check, improper inserts previously occurred, causing problems with sorting the skip list. */
					past_node = cur_node;
					cur_node = cur_node->next_ptrs[n];
                		}

			} else if (memcmp_ret < 0) {
				past_node = cur_node;
				cur_node = cur_node->next_ptrs[n];

			} else {
				cur_node = NULL;
			}
		}

		past_nodes[n] = past_node;
	}
}

void insert_node(node* cur_node, mmt_inst* cur_inst) {

	node* past_nodes[MAX_LAYER_LEVEL];
	traverse_layers(cur_node->buff_ptr, cur_node->key_len, past_nodes, cur_inst, SEARCH_MODE_UNLINK);
	node* past_node = NULL;
	node* next_node = NULL;

	for (int n = (cur_node->layer_cnt - 1); n >= 0; n--) {
		past_node = past_nodes[n];

		if (past_node == NULL) /* no node key smaller than the selected key exists in this layer, so make the new node (cur_node) the header of that list */ {
							                	next_node = cur_inst->headers[n];										cur_inst->headers[n] = cur_node;
										cur_node->next_ptrs[n] = next_node;										continue;
									}

						        		next_node = past_node->next_ptrs[n];
									past_node->next_ptrs[n] = cur_node;
									cur_node->next_ptrs[n] = next_node;
								}

}

node* lookup_node(char key[], size_t key_len, mmt_inst* cur_inst) {

	node *past_nodes[MAX_LAYER_LEVEL];

	traverse_layers(key, key_len, past_nodes, cur_inst, SEARCH_MODE_LOOKUP /* we do this in lookup, because while using lookup we might also want to actually search for the node to see if its in the list, thats what lookup is even for, so passing -1 as a parameter executes the branch where the same node is added to the past_nodes array aswell */); // by traversing the skip list and storing the previous nodes it becomes easier and faster to find the node with the key, we're searching for, compared to traversing the original skip list (which might take O(n)), this approach can possibly take O(log(n)) instead. and we can confirm that the node, if it exists, will be at layer 1 (the original skip list), so it will be at past_nodes[0] */
								if (past_nodes[0] == NULL) return NULL;
								int memcmp_ret = memcmp(key, past_nodes[0]->buff_ptr, key_len); // compares both keys, just in case
								if (memcmp_ret == 0 && past_nodes[0]->key_len == key_len) return past_nodes[0];
															return NULL;
														}

node* put_node(char key[], size_t key_len, char val[], size_t val_len, int io_type, mmt_inst* cur_inst) {

	node* old_node = lookup_node(key, key_len, cur_inst);
	node* cur_node = create_node(key, key_len, val, val_len, io_type, cur_inst->arena_mem);

	if (cur_node == NULL) {
		return NULL;
	} else if (old_node != NULL) {
		node* past_nodes[MAX_LAYER_LEVEL];
		node* past_node;
		node* next_node;

		traverse_layers(key, key_len, past_nodes, cur_inst, SEARCH_MODE_UNLINK);

		int layer_cnt = MAX(old_node->layer_cnt, cur_node->layer_cnt);

		for (int n = (layer_cnt - 1); n >= 0; n--) {
			past_node = past_nodes[n];

			if (n < old_node->layer_cnt && n < cur_node->layer_cnt) /* both nodes exists at this layer */ {
				if (past_node == NULL) /* we previously did not have this condition check, and it did cause segfaults when we tried to access past_node, but it turned out to be null. so here whenever it is the case, we just make the cur_node the header of that layer, cause no other node exists there yet */ {
					cur_inst->headers[n] = cur_node;
					continue;
				}

				next_node = old_node->next_ptrs[n];
				past_node->next_ptrs[n] = cur_node;
				cur_node->next_ptrs[n] = next_node;

			} else if (n < old_node->layer_cnt && n >= cur_node->layer_cnt) /* only old_node exists at this layer */ {
											if (past_node == NULL) /* since cur_node obviously does not exist at this layer, and there is no past_node aswell, meaning old_node is the header, so we unlink it from that layer and make the header NULL, and that makes the layer empty */ {
					cur_inst->headers[n] = NULL;
					continue;
				}

				next_node = old_node->next_ptrs[n];
				past_node->next_ptrs[n] = next_node;

			} else if (n < cur_node->layer_cnt && n >= old_node->layer_cnt) /* only cur_node exists at this layer */ {
											if (past_node == NULL) {
					cur_inst->headers[n] = cur_node;
					continue;
				}

				next_node = past_node->next_ptrs[n];
				past_node->next_ptrs[n] = cur_node;
				cur_node->next_ptrs[n] = next_node;
			}
																} /* both nodes not existing at a layer can't really happen because we start the iteration from either layer_cnt of old_node or cur_node, so this just means we've finished iterating */
		
		return cur_node;
	
	}
	
	insert_node(cur_node, cur_inst);
	
	return cur_node;


}

int delete_node(char key[], size_t key_len, mmt_inst* cur_inst) {

	char val[] = "";

	node* cur_node = put_node(key, key_len, val, 0, IO_TYPE_SET_DEL, cur_inst);

	if (cur_node == NULL) return -1;

	return 0;
}
