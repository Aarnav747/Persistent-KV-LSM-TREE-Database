#include "wal.h"

// runs every new io operation performed (basically when PUT/DELETE instructions are ran)
int wal_write(int wal_fd, char key[], char val[], int io_type) /* used to save the operations that led to the current conditions */ {
	size_t buffer_len = strlen(key) + strlen(val);

	struct wal_struct *wal_record = malloc(sizeof(struct wal_struct) + buffer_len);

	wal_record->total_len = buffer_len + sizeof(struct wal_struct);
	wal_record->io_type = io_type;
	wal_record->key_len = strlen(key);
	wal_record->val_len = strlen(val);

	// fill wal_record's buffer
	memcpy(wal_record->buffer, key, wal_record->key_len);

	char *val_ptr = wal_record->buffer + wal_record->key_len;
	memcpy(val_ptr, val, wal_record->val_len);

	ssize_t written_bytes = write(wal_fd, wal_record, wal_record->total_len);

	if (written_bytes < 0) return -1;

	if ((size_t)written_bytes < wal_record->total_len) {
		size_t total_written = written_bytes;
		unsigned char *cur_pos;

		// a pointer at the starting index of the unwritten data in the struct
		while (total_written < wal_record->total_len) {
		cur_pos = (unsigned char *)wal_record + total_written;

		written_bytes = write(wal_fd, cur_pos, wal_record->total_len - total_written);

		total_written += written_bytes;
		}
	}

	// flush all of the unsaved changes to the disk space
	fsync(wal_fd);

	free(wal_record);

	return 0;
}

int wal_read(int wal_fd) {

	while (1) {
		struct wal_struct *wal_record_temp = malloc(sizeof(struct wal_struct));

		int read_bytes1 = read(wal_fd, wal_record_temp, sizeof(struct wal_struct));

		if (read_bytes1 == 0) {
			free(wal_record_temp);
			break;
		}

        	struct wal_struct *wal_record = realloc(wal_record_temp, wal_record_temp->total_len); // reallocating the size of the struct because of the buffer

		int read_bytes2 = read(wal_fd, wal_record->buffer, (wal_record_temp->total_len - sizeof(struct wal_struct)));

		free(wal_record);
	}

	return 0;
}
