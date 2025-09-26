/**
 * @file     common.h
 * @Author   Student Implementation
 * @date     2024
 * @brief    Common definitions for distributed banking system
 */

#ifndef __IFMO_DISTRIBUTED_CLASS_COMMON__H
#define __IFMO_DISTRIBUTED_CLASS_COMMON__H

#include <stdint.h>
#include <sys/types.h>

// Process ID type
typedef int8_t local_id;

// Timestamp type  
typedef int8_t timestamp_t;

// Constants
enum {
    PARENT_ID = 0,
    MAX_PROCESS_ID = 15
};

// Message types
enum {
    STARTED = 0,
    DONE,
    ACK,
    STOP,
    TRANSFER,
    BALANCE_HISTORY
};

// Maximum message size
enum {
    MAX_PAYLOAD_LEN = 255,
    MAX_MESSAGE_LEN = 1024  // Will be recalculated after MessageHeader is defined
};

// Message header
typedef struct {
    uint16_t s_magic;        // Magic signature for validation
    uint16_t s_payload_len;  // Length of payload in bytes
    int16_t  s_type;         // Message type
    timestamp_t s_local_time; // Physical time when message was sent
} __attribute__((packed)) MessageHeader;

// Message structure
typedef struct {
    MessageHeader s_header;
    char s_payload[MAX_PAYLOAD_LEN]; // Fixed size payload
} __attribute__((packed)) Message;

#endif // __IFMO_DISTRIBUTED_CLASS_COMMON__H
