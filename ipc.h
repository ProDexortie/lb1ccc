/**
 * @file     ipc.h
 * @Author   Student Implementation  
 * @date     2024
 * @brief    IPC functions for distributed banking system
 */

#ifndef __IFMO_DISTRIBUTED_CLASS_IPC__H
#define __IFMO_DISTRIBUTED_CLASS_IPC__H

#include "common.h"

// IPC context for processes
typedef struct {
    local_id id;                    // Process ID
    int process_count;              // Total number of processes
    int pipes[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1][2]; // [from][to][read/write]
} IPC;

//------------------------------------------------------------------------------
// Functions to be implemented by students
//------------------------------------------------------------------------------

/** Send a message to the process with id=dst.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param dst     ID of recipient
 * @param msg     Message to send
 *
 * @return 0 on success, any non-zero value on error
 */
int send(void * self, local_id dst, const Message * msg);

/** Send multicast message.
 *
 * Send msg to all other processes including parrent.
 * Should stop on the first error.
 * 
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message to multicast.
 *
 * @return 0 on success, any non-zero value on error
 */
int send_multicast(void * self, const Message * msg);

/** Receive a message from the process with id=from.
 *
 * Might block depending on IPC settings.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param from    ID of the process to receive message from
 * @param msg     Message buffer to receive message
 *
 * @return 0 on success, any non-zero value on error
 */
int receive(void * self, local_id from, Message * msg);

/** Receive a message from any process.
 *
 * Receive a message from any process, in case of blocking I/O should be used with extra care
 * to avoid deadlocks.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message buffer to receive message
 *
 * @return 0 on success, any non-zero value on error
 */
int receive_any(void * self, Message * msg);

#endif // __IFMO_DISTRIBUTED_CLASS_IPC__H
