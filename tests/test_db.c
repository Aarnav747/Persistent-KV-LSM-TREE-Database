#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "arena.h"
#include "node.h"
#include "memtable.h"
#include "wal.h"
#include "debug.h"

int main(void) {
	srand(6969); // fixed seed so failures reproduce

	arena a;
	a.total_size = 16 * 1024 * 1024;
	a.offset = 0;
	a.mem_ptr = malloc(a.total_size);

	mmt_inst mt;
	memset(mt.headers, 0, sizeof(mt.headers));
	mt.arena_mem = &a;

	/*printf("Add K1: \n");
	node* k1 = put_node("K1", 2, "V1", 2, IO_TYPE_SET_PUT, &mt);
	dump_list(&mt);
	printf("Add K2: \n");
	node* k2 = put_node("K2", 2, "K2", 2, IO_TYPE_SET_PUT, &mt);
	printf("K2 Added. K1's Next node: %.*s\n", (int)k1->next_ptrs[0]->key_len, k1->next_ptrs[0]->buff_ptr);
	dump_list(&mt);
	printf("Add K1 again: \n");
	node* new_k1 = put_node("K1", 2, "K1", 2, IO_TYPE_SET_PUT, &mt);
	printf("New K1 Added\n");
	printf("New K1 Added again. New K1's Next node: %.*s\n", (int)new_k1->next_ptrs[0]->key_len, new_k1->next_ptrs[0]->buff_ptr);*/

	diff_fuzz(&mt);

	// put + lookup
	assert(put_node("a", 1, "1", 1, IO_TYPE_SET_PUT, &mt) != NULL);
	node* n = lookup_node("a", 1, &mt);
	assert(n != NULL && n->val_len == 1);
	assert(memcmp(n->buff_ptr + n->key_len, "1", 1) == 0);

	// overwrite with a longer value
	put_node("a", 1, "hello", 5, IO_TYPE_SET_PUT, &mt);
	n = lookup_node("a", 1, &mt);
	assert(n != NULL && n->val_len == 5);
	assert(memcmp(n->buff_ptr + n->key_len, "hello", 5) == 0);

	// prefix keys
	put_node("cat", 3, "x", 1, IO_TYPE_SET_PUT, &mt);
	put_node("cats", 4, "y", 1, IO_TYPE_SET_PUT, &mt);
	assert(lookup_node("cat", 3, &mt) != NULL);
	assert(lookup_node("cats", 4, &mt) != NULL);
	assert(lookup_node("ca", 2, &mt) == NULL);

	// delete
	assert(delete_node("a", 1, &mt) == 0);
	n = lookup_node("a", 1, &mt);
	assert(n != NULL && n->del_status == IO_TYPE_SET_DEL);
	
	// write-ahead log 
	int wal_fd = open("wal_log.txt", O_APPEND | O_CREAT | O_RDWR, 0644);

	char key_wal[1024];
	char val_wal[1024];

	memcpy(key_wal, mt.headers[0]->buff_ptr, mt.headers[0]->key_len);
	memcpy(val_wal, mt.headers[0]->buff_ptr + mt.headers[0]->key_len, mt.headers[0]->val_len);

	wal_write(wal_fd, key_wal, mt.headers[0]->key_len, val_wal, mt.headers[0]->val_len, IO_TYPE_SET_PUT);

	memcpy(key_wal, mt.headers[1]->buff_ptr, mt.headers[1]->key_len);                                               memcpy(val_wal, mt.headers[1]->buff_ptr + mt.headers[1]->key_len, mt.headers[1]->val_len);                                                                              wal_write(wal_fd, key_wal, mt.headers[1]->key_len, val_wal, mt.headers[1]->val_len, IO_TYPE_SET_PUT);

	// level 0 must be sorted
	for (node* c = mt.headers[0]; c && c->next_ptrs[0]; c = c->next_ptrs[0]) {
		node* nx = c->next_ptrs[0];
		size_t m = c->key_len < nx->key_len ? c->key_len : nx->key_len;
		int r = memcmp(c->buff_ptr, nx->buff_ptr, m);
		assert(r < 0 || (r == 0 && c->key_len < nx->key_len));
	}

	// fill the arena: should return NULL, not crash
	char key[16];
	int i = 0;
	while (1) {
		int len = snprintf(key, sizeof(key), "k%d", i++);
		if (put_node(key, len, "v", 1, IO_TYPE_SET_PUT, &mt) == NULL) break;
	}

	printf("all tests passed\n");
	free(a.mem_ptr);
	return 0;
}
