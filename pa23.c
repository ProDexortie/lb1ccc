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

// Global Lamport clock for this process
static timestamp_t lamport_clock = 0;

// Process data for child
typedef struct {
    local_id id;
    balance_t balance;
    BalanceHistory history;
    timestamp_t current_time;
    balance_t pending_in;  // Track money in transit
} ChildData;

// Lamport clock functions
timestamp_t get_lamport_time(void) {
    return lamport_clock;
}

// Increment Lamport clock before sending
timestamp_t increment_lamport_time(void) {
    return ++lamport_clock;
}

// Update Lamport clock on message receipt  
void update_lamport_time(timestamp_t received_time) {
    if (received_time > lamport_clock) {
        lamport_clock = received_time;
    }
    lamport_clock++;
}

// Initialize message with header
void init_message(Message * msg, int type, uint16_t payload_len) {
    msg->s_header.s_magic = 0;  // Can be used for validation
    msg->s_header.s_payload_len = payload_len;
    msg->s_header.s_type = type;
    msg->s_header.s_local_time = increment_lamport_time(); // Use Lamport time and increment
}

// Update balance history for a time period
void update_balance_history(ChildData * child, balance_t new_balance, balance_t pending_amount) {
    timestamp_t current_time = get_lamport_time();
    
    // Fill in the time progression correctly - limit to MAX_T (127 for signed int8_t)
    while (child->current_time <= current_time && child->current_time <= 127) {
        child->history.s_history[child->current_time].s_time = child->current_time;
        child->history.s_history[child->current_time].s_balance = (child->current_time == current_time) ? new_balance : child->balance;
        child->history.s_history[child->current_time].s_balance_pending_in = (child->current_time == current_time) ? pending_amount : child->pending_in;
        
        child->current_time++;
    }
    
    // Update the balance and pending amount
    child->balance = new_balance;
    child->pending_in = pending_amount;
    
    // Update history length
    if (child->current_time > child->history.s_history_len) {
        child->history.s_history_len = (child->current_time <= 127) ? child->current_time : 128;
    }
}

// Child process main loop
void child_main(local_id id, balance_t initial_balance) {
    ChildData child;
    child.id = id;
    child.balance = initial_balance;
    child.current_time = 0;
    child.pending_in = 0;
    child.history.s_id = id;
    child.history.s_history_len = 1;
    
    ipc_ctx.id = id;
    
    // Initialize balance history with initial balance
    update_balance_history(&child, initial_balance, 0);
    
    // Send STARTED message
    Message msg;
    init_message(&msg, STARTED, 0);
    send_multicast(&ipc_ctx, &msg);
    
    printf(log_started_fmt, get_lamport_time(), id, getpid(), getppid(), initial_balance);
    
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
                        update_balance_history(&child, child.balance - order->s_amount, child.pending_in);
                        printf(log_transfer_out_fmt, get_lamport_time(), id, order->s_amount, order->s_dst);
                        
                        // Forward to destination
                        send(&ipc_ctx, order->s_dst, &received_msg);
                        
                    } else if (order->s_dst == id) {
                        // This is destination - increase balance and send ACK
                        // No pending money since we received it
                        update_balance_history(&child, child.balance + order->s_amount, 0);
                        printf(log_transfer_in_fmt, get_lamport_time(), id, order->s_amount, order->s_src);
                        
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
    
    // Send DONE message and update history one more time 
    init_message(&msg, DONE, 0);
    update_balance_history(&child, child.balance, child.pending_in); // Ensure final state is recorded
    send_multicast(&ipc_ctx, &msg);
    printf(log_done_fmt, get_lamport_time(), id, child.balance);
    
    // Send BALANCE_HISTORY to parent - send it directly via pipe without using message structure
    int parent_pipe = ipc_ctx.pipes[child.id][PARENT_ID][1];
    if (parent_pipe != -1) {
        write(parent_pipe, &child.history, sizeof(BalanceHistory));
        // Close the pipe to signal we're done
        close(parent_pipe);
    }
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
    
    printf(log_received_all_started_fmt, get_lamport_time(), PARENT_ID);
    
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
    
    printf(log_received_all_done_fmt, get_lamport_time(), PARENT_ID);
    
    // Collect all balance histories - read them directly
    AllHistory all_history;
    memset(&all_history, 0, sizeof(AllHistory));
    all_history.s_history_len = num_children;
    
    int history_count = 0;
    while (history_count < num_children) {
        // Try to read BalanceHistory directly from each child's pipe
        for (local_id i = 1; i <= num_children && history_count < num_children; i++) {
            int child_pipe = ipc_ctx.pipes[i][PARENT_ID][0];
            if (child_pipe != -1) {
                BalanceHistory history;
                ssize_t bytes_read = read(child_pipe, &history, sizeof(BalanceHistory));
                if (bytes_read == sizeof(BalanceHistory)) {
                    // Ensure we don't write out of bounds
                    if (history.s_id > 0 && history.s_id <= MAX_PROCESS_ID && history.s_id <= num_children) {
                        memcpy(&all_history.s_history[history.s_id], &history, sizeof(BalanceHistory));
                        history_count++;
                        // Debug print
                        fprintf(stderr, "Collected history for process %d, len=%d\n", history.s_id, history.s_history_len);
                    }
                }
            }
        }
        if (history_count < num_children) {
            usleep(1000);
        }
    }
    
    // Print history
    fprintf(stderr, "About to print history with s_history_len=%d\n", all_history.s_history_len);
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
