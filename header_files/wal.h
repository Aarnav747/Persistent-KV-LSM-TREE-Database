#ifndef WAL_H
#define WAL_H

#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>

#include "tags.h"

struct wal_struct {                                      	size_t total_len; // total length of the struct
	int io_type; // io operation type (PUT/DELETE)
	size_t key_len; // key length
	size_t val_len; // value/data length
	char buffer[]; // stores key and val bytes. layout: [key bytes][value bytes]   
                         
	/* key + val are added later in 'buffer' during the call to wal_write() */
};

int wal_write(int wal_fd, char key[], char val[], int io_type);

int wal_read(int wal_fd);

#endif
