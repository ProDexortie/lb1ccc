#ifndef IPC_IMPL_H
#define IPC_IMPL_H

#include "ipc.h"

// Внутренняя структура контекста IPC (общее определение для main.c и ipc.c)
typedef struct {
    local_id id;                 // локальный id процесса (0..N)
    int n_processes;             // всего процессов, включая родителя (N+1)
    int read_fd[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];  // from -> to (read end)
    int write_fd[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1]; // from -> to (write end)
} IPCContext;

#endif // IPC_IMPL_H
