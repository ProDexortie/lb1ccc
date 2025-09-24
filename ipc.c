#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <sys/types.h>

#include "ipc.h"
#include "ipc_impl.h"

static int write_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)w;
        left -= (size_t)w;
    }
    return 0;
}

static int read_all(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    size_t left = n;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r == 0) {
            // EOF — пишущий конец закрыт
            return -1;
        }
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += (size_t)r;
        left -= (size_t)r;
    }
    return 0;
}

int send(void * self, local_id dst, const Message * msg) {
    IPCContext *ctx = (IPCContext *)self;
    if (!ctx || dst < 0 || dst >= ctx->n_processes) return -1;
    int fd = ctx->write_fd[ctx->id][dst];
    if (fd < 0) return -1;

    // Сначала заголовок, затем полезная нагрузка
    if (write_all(fd, &msg->s_header, sizeof(MessageHeader)) != 0) return -1;
    if (msg->s_header.s_payload_len > 0) {
        if (write_all(fd, msg->s_payload, msg->s_header.s_payload_len) != 0) return -1;
    }
    return 0;
}

int send_multicast(void * self, const Message * msg) {
    IPCContext *ctx = (IPCContext *)self;
    if (!ctx) return -1;
    for (int i = 0; i < ctx->n_processes; ++i) {
        if (i == ctx->id) continue;
        if (send(self, (local_id)i, msg) != 0) return -1;
    }
    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    IPCContext *ctx = (IPCContext *)self;
    if (!ctx || !msg || from < 0 || from >= ctx->n_processes) return -1;
    int fd = ctx->read_fd[from][ctx->id];
    if (fd < 0) return -1;

    // Читаем заголовок
    if (read_all(fd, &msg->s_header, sizeof(MessageHeader)) != 0) return -1;

    // Проверка магии (не критично, но полезно)
    if (msg->s_header.s_magic != MESSAGE_MAGIC) {
        // Попытка читать полезную нагрузку, чтобы не ломать поток
        uint16_t len = msg->s_header.s_payload_len;
        if (len > 0) {
            if (len > MAX_PAYLOAD_LEN) return -1;
            if (read_all(fd, msg->s_payload, len) != 0) return -1;
        }
        return -1;
    }

    // Читаем полезную нагрузку
    uint16_t len = msg->s_header.s_payload_len;
    if (len > 0) {
        if (len > MAX_PAYLOAD_LEN) return -1;
        if (read_all(fd, msg->s_payload, len) != 0) return -1;
    }
    return 0;
}

int receive_any(void * self, Message * msg) {
    IPCContext *ctx = (IPCContext *)self;
    if (!ctx || !msg) return -1;

    // Готовим poll() по всем входящим каналам i->self.id
    struct pollfd pfds[MAX_PROCESS_ID + 1];
    int who[MAX_PROCESS_ID + 1];
    int count = 0;

    for (int i = 0; i < ctx->n_processes; ++i) {
        if (i == ctx->id) continue;
        int fd = ctx->read_fd[i][ctx->id];
        if (fd >= 0) {
            pfds[count].fd = fd;
            pfds[count].events = POLLIN;
            pfds[count].revents = 0;
            who[count] = i;
            count++;
        }
    }

    if (count == 0) return -1;

    while (1) {
        int rv = poll(pfds, (nfds_t)count, -1);
        if (rv < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        for (int k = 0; k < count; ++k) {
            if (pfds[k].revents & POLLIN) {
                // С этого отправителя и читаем
                int from = who[k];
                int fd = pfds[k].fd;
                // Читаем заголовок
                if (read_all(fd, &msg->s_header, sizeof(MessageHeader)) != 0) return -1;
                uint16_t len = msg->s_header.s_payload_len;
                if (len > 0) {
                    if (len > MAX_PAYLOAD_LEN) return -1;
                    if (read_all(fd, msg->s_payload, len) != 0) return -1;
                }
                return 0;
            }
        }
    }
}
