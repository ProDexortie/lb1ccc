/**
 * @file     ipc.c
 * @Author   Student Implementation
 * @date     2024
 * @brief    IPC implementation using pipes for distributed banking system
 */

#include "ipc.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

// External Lamport clock functions - implemented in pa23.c
extern void update_lamport_time(timestamp_t received_time);

int send(void * self, local_id dst, const Message * msg) {
    IPC * ipc = (IPC *)self;
    
    if (dst > MAX_PROCESS_ID || dst < 0) {
        return -1;
    }
    
    int fd = ipc->pipes[ipc->id][dst][1]; // write end
    if (fd == -1) {
        return -1;
    }
    
    size_t total_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;
    
    ssize_t bytes_written = write(fd, msg, total_len);
    if (bytes_written != (ssize_t)total_len) {
        return -1;
    }
    
    return 0;
}

int send_multicast(void * self, const Message * msg) {
    IPC * ipc = (IPC *)self;
    
    for (int i = 0; i < ipc->process_count; i++) {
        if (i != ipc->id) {
            if (send(self, i, msg) != 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    IPC * ipc = (IPC *)self;
    
    if (from > MAX_PROCESS_ID || from < 0) {
        return -1;
    }
    
    int fd = ipc->pipes[from][ipc->id][0]; // read end
    if (fd == -1) {
        return -1;
    }
    
    // First read the header
    ssize_t bytes_read = read(fd, &msg->s_header, sizeof(MessageHeader));
    if (bytes_read != sizeof(MessageHeader)) {
        if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return -1; // No data available
        }
        return -1;
    }
    
    // Read payload if exists
    if (msg->s_header.s_payload_len > 0) {
        bytes_read = read(fd, msg->s_payload, msg->s_header.s_payload_len);
        if (bytes_read != msg->s_header.s_payload_len) {
            return -1;
        }
    }
    
    // Update Lamport clock on message receipt
    update_lamport_time(msg->s_header.s_local_time);
    
    return 0;
}

int receive_any(void * self, Message * msg) {
    IPC * ipc = (IPC *)self;
    
    // Try to receive from any process
    for (int from = 0; from < ipc->process_count; from++) {
        if (from != ipc->id) {
            if (receive(self, from, msg) == 0) {
                return 0; // Lamport clock already updated in receive()
            }
        }
    }
    
    return -1; // No messages available
}
