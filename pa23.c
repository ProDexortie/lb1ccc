/**
 * @file     pa23.c
 * @Author   Student Implementation
 * @date     2024
 * @brief    Main implementation of distributed banking system
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#include "banking.h"
#include "pa2345.h"

// Global IPC context
IPC ipc_ctx;

// Process data for child
typedef struct {
    local_id id;
    balance_t balance;
    BalanceHistory history;
    timestamp_t current_time;
} ChildData;

// Initialize message with header
void init_message(Message * msg, int type, uint16_t payload_len) {
    msg->s_header.s_magic = 0;  // Can be used for validation
    msg->s_header.s_payload_len = payload_len;
    msg->s_header.s_type = type;
    msg->s_header.s_local_time = get_physical_time();
}

// Update balance history for a time period
void update_balance_history(ChildData * child, balance_t new_balance) {
    timestamp_t current_time = get_physical_time();
    
    // Fill history gap if time jumped
    for (timestamp_t t = child->current_time; t < current_time && t < MAX_T; t++) {
        child->history.s_history[t].s_time = t;
        child->history.s_history[t].s_balance = child->balance;
        child->history.s_history[t].s_balance_pending_in = 0; // Always 0 for PA2
    }
    
    // Update current balance and time
    child->balance = new_balance;
    child->current_time = current_time;
    
    if (current_time < MAX_T) {
        child->history.s_history[current_time].s_time = current_time;
        child->history.s_history[current_time].s_balance = new_balance;
        child->history.s_history[current_time].s_balance_pending_in = 0;
    }
    
    // Update history length (ensure it doesn't exceed MAX_T)
    if (current_time >= child->history.s_history_len && current_time < MAX_T) {
        child->history.s_history_len = current_time + 1;
    } else if (current_time >= MAX_T && child->history.s_history_len < MAX_T) {
        child->history.s_history_len = MAX_T;
    }
}

// Child process main loop
void child_main(local_id id, balance_t initial_balance) {
    ChildData child;
    child.id = id;
    child.balance = initial_balance;
    child.current_time = 0;
    child.history.s_id = id;
    child.history.s_history_len = 1;
    
    ipc_ctx.id = id;
    
    // Initialize balance history with initial balance
    update_balance_history(&child, initial_balance);
    
    // Send STARTED message
    Message msg;
    init_message(&msg, STARTED, 0);
    send_multicast(&ipc_ctx, &msg);
    
    printf(log_started_fmt, get_physical_time(), id, getpid(), getppid(), initial_balance);
    
    // Main loop - receive messages
    int done = 0;
    while (!done) {
        Message received_msg;
        if (receive_any(&ipc_ctx, &received_msg) == 0) {
            switch (received_msg.s_header.s_type) {
                case TRANSFER: {
                    TransferOrder * order = (TransferOrder *)received_msg.s_payload;
                    
                    if (order->s_src == id) {
                        // This is source - decrease balance and forward
                        update_balance_history(&child, child.balance - order->s_amount);
                        printf(log_transfer_out_fmt, get_physical_time(), id, order->s_amount, order->s_dst);
                        
                        // Forward to destination
                        send(&ipc_ctx, order->s_dst, &received_msg);
                        
                    } else if (order->s_dst == id) {
                        // This is destination - increase balance and send ACK
                        update_balance_history(&child, child.balance + order->s_amount);
                        printf(log_transfer_in_fmt, get_physical_time(), id, order->s_amount, order->s_src);
                        
                        // Send ACK to parent
                        Message ack_msg;
                        init_message(&ack_msg, ACK, 0);
                        send(&ipc_ctx, PARENT_ID, &ack_msg);
                    }
                    break;
                }
                case STOP:
                    done = 1;
                    break;
            }
        }
        usleep(1000); // Small delay to prevent busy waiting
    }
    
    // Send DONE message
    init_message(&msg, DONE, 0);
    send_multicast(&ipc_ctx, &msg);
    printf(log_done_fmt, get_physical_time(), id, child.balance);
    
    // Send BALANCE_HISTORY to parent
    Message history_msg;
    init_message(&history_msg, BALANCE_HISTORY, sizeof(BalanceHistory));
    memcpy(history_msg.s_payload, &child.history, sizeof(BalanceHistory));
    send(&ipc_ctx, PARENT_ID, &history_msg);
}

// Parent process functions
void parent_main(int num_children) {
    ipc_ctx.id = PARENT_ID;
    
    // Wait for all STARTED messages
    int started_count = 0;
    while (started_count < num_children) {
        Message msg;
        if (receive_any(&ipc_ctx, &msg) == 0) {
            if (msg.s_header.s_type == STARTED) {
                started_count++;
            }
        }
        usleep(1000);
    }
    
    printf(log_received_all_started_fmt, get_physical_time(), PARENT_ID);
    
    // Run bank robbery
    bank_robbery(&ipc_ctx, num_children);
    
    // Send STOP to all children
    Message stop_msg;
    init_message(&stop_msg, STOP, 0);
    send_multicast(&ipc_ctx, &stop_msg);
    
    // Wait for all DONE messages
    int done_count = 0;
    while (done_count < num_children) {
        Message msg;
        if (receive_any(&ipc_ctx, &msg) == 0) {
            if (msg.s_header.s_type == DONE) {
                done_count++;
            }
        }
        usleep(1000);
    }
    
    printf(log_received_all_done_fmt, get_physical_time(), PARENT_ID);
    
    // Collect all balance histories
    AllHistory all_history;
    all_history.s_history_len = num_children;
    
    int history_count = 0;
    while (history_count < num_children) {
        Message msg;
        if (receive_any(&ipc_ctx, &msg) == 0) {
            if (msg.s_header.s_type == BALANCE_HISTORY) {
                BalanceHistory * history = (BalanceHistory *)msg.s_payload;
                memcpy(&all_history.s_history[history->s_id], history, sizeof(BalanceHistory));
                history_count++;
            }
        }
        usleep(1000);
    }
    
    // Print history
    print_history(&all_history);
}

// Transfer implementation
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
    IPC * ipc = (IPC *)parent_data;
    
    Message msg;
    TransferOrder order = {src, dst, amount};
    
    init_message(&msg, TRANSFER, sizeof(TransferOrder));
    memcpy(msg.s_payload, &order, sizeof(TransferOrder));
    
    // Send to source process
    send(ipc, src, &msg);
    
    // Wait for ACK
    Message ack_msg;
    while (1) {
        if (receive(ipc, dst, &ack_msg) == 0) {
            if (ack_msg.s_header.s_type == ACK) {
                break;
            }
        }
        usleep(1000);
    }
}

// Setup pipes for IPC
void setup_pipes(int num_processes) {
    // Initialize all pipes to -1
    for (int i = 0; i <= MAX_PROCESS_ID; i++) {
        for (int j = 0; j <= MAX_PROCESS_ID; j++) {
            ipc_ctx.pipes[i][j][0] = -1;
            ipc_ctx.pipes[i][j][1] = -1;
        }
    }
    
    // Create pipes between all processes
    for (int i = 0; i < num_processes; i++) {
        for (int j = 0; j < num_processes; j++) {
            if (i != j) {
                int pipefd[2];
                if (pipe(pipefd) == -1) {
                    perror("pipe");
                    exit(1);
                }
                
                ipc_ctx.pipes[i][j][0] = pipefd[0]; // read end
                ipc_ctx.pipes[i][j][1] = pipefd[1]; // write end
                
                // Set non-blocking mode
                fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
                fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
            }
        }
    }
    
    ipc_ctx.process_count = num_processes;
}

// Close unused pipes for a process
void close_unused_pipes(local_id id, int num_processes) {
    for (int i = 0; i < num_processes; i++) {
        for (int j = 0; j < num_processes; j++) {
            if (i != j) {
                if (i == id) {
                    // Close read end for outgoing pipes
                    if (ipc_ctx.pipes[i][j][0] != -1) {
                        close(ipc_ctx.pipes[i][j][0]);
                        ipc_ctx.pipes[i][j][0] = -1;
                    }
                } else if (j == id) {
                    // Close write end for incoming pipes
                    if (ipc_ctx.pipes[i][j][1] != -1) {
                        close(ipc_ctx.pipes[i][j][1]);
                        ipc_ctx.pipes[i][j][1] = -1;
                    }
                } else {
                    // Close both ends for unrelated pipes
                    if (ipc_ctx.pipes[i][j][0] != -1) {
                        close(ipc_ctx.pipes[i][j][0]);
                        ipc_ctx.pipes[i][j][0] = -1;
                    }
                    if (ipc_ctx.pipes[i][j][1] != -1) {
                        close(ipc_ctx.pipes[i][j][1]);
                        ipc_ctx.pipes[i][j][1] = -1;
                    }
                }
            }
        }
    }
}

int main(int argc, char * argv[]) {
    if (argc < 4 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: %s -p N balance1 balance2 ... balanceN\n", argv[0]);
        return 1;
    }
    
    int num_children = atoi(argv[2]);
    if (num_children <= 0 || num_children > MAX_PROCESS_ID) {
        fprintf(stderr, "Invalid number of children: %d\n", num_children);
        return 1;
    }
    
    if (argc != 3 + num_children) {
        fprintf(stderr, "Expected %d balance arguments, got %d\n", num_children, argc - 3);
        return 1;
    }
    
    // Parse balances
    balance_t balances[MAX_PROCESS_ID + 1];
    for (int i = 0; i < num_children; i++) {
        balances[i + 1] = (balance_t)atoi(argv[3 + i]);
    }
    
    // Setup IPC
    setup_pipes(num_children + 1); // +1 for parent
    
    // Fork children
    for (local_id i = 1; i <= num_children; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close_unused_pipes(i, num_children + 1);
            child_main(i, balances[i]);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            return 1;
        }
    }
    
    // Parent process
    close_unused_pipes(PARENT_ID, num_children + 1);
    parent_main(num_children);
    
    // Wait for all children
    for (int i = 0; i < num_children; i++) {
        wait(NULL);
    }
    
    return 0;
}
