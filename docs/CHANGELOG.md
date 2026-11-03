[29/10/26]:
Changed types of lengths (like key_len and val_len) from size_t to uint32_t, but only for the entries that will go in the memory, to ensure correct byte-reading and get rid of the platform dependency that size_t has. Previously, our key_len, val_len and io_type (the tombstone) variables were 8 bytes, 8 bytes and 4 bytes long respectively, in memory, and changing them to uint32_t and uint8_t brings the header size of an sstable entry from 20 bytes down to 9 bytes, which also saves up memory in the long run.

[30/10/26]:
Added new tags, which are SST_KEYLEN_SIZE (4 bytes, = sizeof(uint32_t)), SST_VALLEN_SIZE (4 bytes, = sizeof(uint32_t)) and SST_IOTYPE_SIZE (1 byte, = sizeof(uint8_t)), to get rid of using sizeof() repeatedly, which was kind of confusing at first. Also added MAX_KEY_SIZE and MAX_VAL_SIZE, whom values are 64KB and 16MB respectfully (for now). This was added to prevent addition of keys or values that might way bigger in size than nessecary, for an in-memory storage engine.

[30/10/26]:
I removed the local buffers in flush_sst and read_sst, which were named w_buffer and r_buffer. They used to store the data of a node, but now we prefer using an arena memory space instead. The buffers had a clear limit, and while the arena still do, its nonetheless a much better technique to use in this case. We currently allocate it once and re-use it throughout flush_sst and read_sst. This arena is different from the arena of the memtable.

[03/10/26]:
Finished writing the 'cmp_read' function. Which compares the nodes in the original skip-list (layer 0) with the latest node that was read from the read_sst function. I previously had a lookup method instead of layer-0 one, where we read a node through read_sst, called this function, and than it called lookup_node to find it in the memtable. But later I found that this wasn't a reliable choice of a method, because if the flush (via flush_sst) might've skipped a node while performing it's flushing, we wouldn't find that out very easily, and it easily be a severe bug, so I instead went with this traverse approach.
