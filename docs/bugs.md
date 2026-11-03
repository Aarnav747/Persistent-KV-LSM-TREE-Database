[23/10/26]:-
The while loop in read_all exits with EOF within the first iteration of the loop, so buffer remains empty, and the 'key_len' and 'val_len' components consists of garbage values from that buffer, hence leading to the stack overflow when we initialize the 'key' and 'val' array respectively. I found this out with simple print debugging, and this was the output:
"read_all loop exited with EOF
read_all loop iteration count: 0
read_all_ret: 0
30 61 86 ca 7f 00 00 00 5c 3f eb 51 73 00 00 00 08 a0 2f 52
key_len: 548858650928
val_len: 495295610716
30 61 86 ca 7f 00 00 00 5c 3f eb 51 73 00 00 00 08 a0 2f 52"

Fix: Previously, we were opening the 'sst.log' file as: 'int sst_fd = open("sst.log", O_APPEND | O_CREAT | O_RDWR, 0644);', And on top, we were using the 'sst_fd' file descriptor both for flushing and reading purposes, and since we've used the O_APPEND flag here, is the reason our read function was returning EOF on the first iteration of it's loop. Now we've changed it to using seperate fds for both purposes in 'test_db.c'. 

[27/10/26]:-
Whenever we ran the cmp_fstat_read function, it was throwing an error of "Node not found", basically when it called found_node. We found this bug, by tracing the return value of fsync inside the write_sst function, where it was returning -1.

Fix: The problem lied inside the caller of read_all (read_sst), because it wasn't handling the condition where read_all returned a clean EOF (meaning 0). And this always happend after all of the keys were read, but read_all was still being called because we did not terminate the loop at EOF. Therefore we added a branch for this, whenever read_all is called, and now it runs properly.
