# Copilot Instructions

## Project Overview

This is a distributed systems programming assignment focused on banking operations using Inter-Process Communication (IPC). The project implements a banking system where multiple processes communicate to perform financial transfers.

## Project Context

- **Language**: C
- **Domain**: Distributed Systems, Inter-Process Communication
- **Academic Context**: University programming assignment (ITMO University style)
- **Key Concepts**: Process synchronization, message passing, banking transactions, Lamport timestamps

## Code Structure

### Core Files
- `pa23.c` - Main implementation file where students implement the `transfer()` function
- `banking.h` - Banking data structures and function declarations (DO NOT MODIFY)
- `bank_robbery.c` - Test/example implementation (DO NOT MODIFY)  
- `pa2345.h` - Logging format constants (DO NOT MODIFY)
- `common.h`, `ipc.h` - References to common headers (symbolic links)
- `libruntime.so` - Runtime library for testing

### Key Data Structures
- `TransferOrder` - Transfer request between processes
- `BalanceState` - Balance at a specific timestamp  
- `BalanceHistory` - Balance history for a process
- `AllHistory` - Complete system balance history

## Coding Guidelines

### Student Implementation Areas
- Only modify `pa23.c` - specifically the `transfer()` function
- Do NOT modify header files marked with "Students must not modify this file!"
- Focus on implementing IPC mechanisms for inter-process transfers

### Code Style
- Follow existing C coding conventions in the project
- Use proper error handling for IPC operations
- Maintain thread/process safety
- Include appropriate logging using provided format strings

### IPC Patterns
- Implement message passing between processes
- Use proper synchronization mechanisms
- Handle process coordination for distributed transactions
- Implement Lamport timestamp ordering where needed

### Security Considerations
- Validate transfer amounts and process IDs
- Ensure atomic transaction operations
- Prevent race conditions in concurrent access
- Handle edge cases (invalid transfers, non-existent processes)

## Testing Approach

- Use the provided `bank_robbery()` function for testing transfers
- Verify balance consistency across all processes
- Test with multiple concurrent processes
- Validate transaction history accuracy
- Check proper message ordering with timestamps

## Common Patterns

When implementing banking transfers:
1. Validate source and destination process IDs
2. Check sufficient balance before transfer
3. Send transfer order to source process
4. Source decreases balance and forwards to destination
5. Destination increases balance and sends ACK
6. Update transaction history with proper timestamps

## Helpful Context

- This follows a classic distributed banking algorithm
- Processes communicate via message passing (not shared memory)
- Each process maintains its own balance and transaction history
- The parent process coordinates transfers but doesn't hold balances
- Proper ordering is crucial for consistency

## Don't Do

- Don't modify read-only header files
- Don't implement shared memory solutions (use message passing)
- Don't skip proper error handling for IPC operations
- Don't ignore timestamp ordering requirements
- Don't break the existing API contracts