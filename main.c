#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"
#include "ipc_impl.h"

// Прототипы из ipc.c (принимают void*, но мы передаем &ctx)
int send(void * self, local_id dst, const Message * msg);
int send_multicast(void * self, const Message * msg);
int receive(void * self, local_id from, Message * msg);
int receive_any(void * self, Message * msg);

// Закрыть неиспользуемые дескрипторы для данного процесса
static void close_unused_fds(IPCContext *ctx) {
    for (int i = 0; i < ctx->n_processes; i++) {
        for (int j = 0; j < ctx->n_processes; j++) {
            if (i == j) continue;
            if (i == ctx->id) {
                // наш исходящий канал i->j: нужен только write end
                if (ctx->read_fd[i][j] >= 0) close(ctx->read_fd[i][j]);
                ctx->read_fd[i][j] = -1;
            } else if (j == ctx->id) {
                // входящий канал i->j: нужен только read end
                if (ctx->write_fd[i][j] >= 0) close(ctx->write_fd[i][j]);
                ctx->write_fd[i][j] = -1;
            } else {
                // чужие каналы не нужны
                if (ctx->read_fd[i][j] >= 0) close(ctx->read_fd[i][j]);
                if (ctx->write_fd[i][j] >= 0) close(ctx->write_fd[i][j]);
                ctx->read_fd[i][j] = -1;
                ctx->write_fd[i][j] = -1;
            }
        }
    }
}

static void logf_flush(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    fflush(f);
    va_end(ap);
}

static void send_event(IPCContext *ctx, MessageType type, const char *payload) {
    Message msg = {0};
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = 0; // PA1: физическое время не требуется
    size_t len = payload ? strlen(payload) : 0;
    if (len > MAX_PAYLOAD_LEN) len = MAX_PAYLOAD_LEN;
    msg.s_header.s_payload_len = (uint16_t)len;
    if (len) memcpy(msg.s_payload, payload, len);
    send_multicast(ctx, &msg);
}

static void recv_all_of_type(IPCContext *ctx, MessageType type) {
    Message msg;
    for (int from = 0; from < ctx->n_processes; from++) {
        if (from == ctx->id) continue;
        // Ждем одно сообщение нужного типа от каждого отправителя
        while (1) {
            if (receive(ctx, (local_id)from, &msg) == 0 && msg.s_header.s_type == type) break;
        }
    }
}

static void build_started(char *buf, size_t sz, local_id id) {
    snprintf(buf, sz, log_started_fmt, id, getpid(), getppid());
}

static void build_done(char *buf, size_t sz, local_id id) {
    snprintf(buf, sz, log_done_fmt, id);
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s -p N\n", prog);
}

int main(int argc, char *argv[]) {
    int n_children = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            n_children = atoi(argv[++i]);
        }
    }
    if (n_children <= 0 || n_children > MAX_PROCESS_ID) {
        usage(argv[0]);
        return 1;
    }

    const int n_total = n_children + 1; // включая родителя с id 0

    // Готовим матрицу пайпов в родителе
    IPCContext ctx_parent = {0};
    ctx_parent.id = 0;
    ctx_parent.n_processes = n_total;
    for (int i = 0; i < n_total; ++i) {
        for (int j = 0; j < n_total; ++j) {
            ctx_parent.read_fd[i][j] = -1;
            ctx_parent.write_fd[i][j] = -1;
        }
    }

    FILE *pfl = fopen(pipes_log, "a");
    if (!pfl) pfl = stdout; // запасной вариант

    for (int i = 0; i < n_total; ++i) {
        for (int j = 0; j < n_total; ++j) {
            if (i == j) continue;
            int fds[2];
            if (pipe(fds) == -1) {
                fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
                return 2;
            }
            ctx_parent.read_fd[i][j] = fds[0];
            ctx_parent.write_fd[i][j] = fds[1];
            fprintf(pfl, "Pipe %d->%d created: rfd=%d wfd=%d\n", i, j, fds[0], fds[1]);
            fflush(pfl);
        }
    }

    pid_t *pids = calloc(n_children + 1, sizeof(pid_t));
    if (!pids) return 3;

    // Форкаем детей с id 1..n_children
    for (int lid = 1; lid <= n_children; ++lid) {
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork() failed: %s\n", strerror(errno));
            return 4;
        }
        if (pid == 0) {
            // Дочерний процесс: копируем контекст и настраиваем дескрипторы
            IPCContext ctx = ctx_parent;
            ctx.id = (local_id)lid;
            close_unused_fds(&ctx);

            FILE *ev = fopen(events_log, "a");
            if (!ev) ev = stdout;

            char buf[256];
            build_started(buf, sizeof(buf), ctx.id);
            // Лог в файл и stdout
            logf_flush(ev, "%s", buf);
            fprintf(stdout, "%s", buf);
            fflush(stdout);

            // Рассылаем STARTED
            send_event(&ctx, STARTED, buf);
            // Ждем STARTED от всех
            recv_all_of_type(&ctx, STARTED);

            // Лог: получили все STARTED
            char rstart[128];
            snprintf(rstart, sizeof(rstart), log_received_all_started_fmt, ctx.id);
            logf_flush(ev, "%s", rstart);
            fprintf(stdout, "%s", rstart);
            fflush(stdout);

            // Имитация работы (при необходимости)
            // ...

            // Лог DONE и рассылка DONE
            char donebuf[128];
            build_done(donebuf, sizeof(donebuf), ctx.id);
            logf_flush(ev, "%s", donebuf);
            fprintf(stdout, "%s", donebuf);
            fflush(stdout);
            send_event(&ctx, DONE, donebuf);

            // Ждем DONE от всех
            recv_all_of_type(&ctx, DONE);

            // Лог: получили все DONE
            char rdone[128];
            snprintf(rdone, sizeof(rdone), log_received_all_done_fmt, ctx.id);
            logf_flush(ev, "%s", rdone);
            fprintf(stdout, "%s", rdone);
            fflush(stdout);

            fclose(ev);
            return 0; // ребенок завершает работу
        } else {
            pids[lid] = pid;
        }
    }

    // Родитель
    IPCContext ctx = ctx_parent;
    ctx.id = 0;
    close_unused_fds(&ctx);

    FILE *ev = fopen(events_log, "a");
    if (!ev) ev = stdout;

    // Родитель STARTED
    char pbuf[256];
    build_started(pbuf, sizeof(pbuf), ctx.id);
    logf_flush(ev, "%s", pbuf);
    fprintf(stdout, "%s", pbuf);
    fflush(stdout);

    // Родитель рассылает STARTED и ждет всех
    send_event(&ctx, STARTED, pbuf);
    recv_all_of_type(&ctx, STARTED);

    char rstart[128];
    snprintf(rstart, sizeof(rstart), log_received_all_started_fmt, ctx.id);
    logf_flush(ev, "%s", rstart);
    fprintf(stdout, "%s", rstart);
    fflush(stdout);

    // Родитель DONE
    char donebuf[128];
    build_done(donebuf, sizeof(donebuf), ctx.id);
    logf_flush(ev, "%s", donebuf);
    fprintf(stdout, "%s", donebuf);
    fflush(stdout);
    send_event(&ctx, DONE, donebuf);

    // Ждет DONE от всех
    recv_all_of_type(&ctx, DONE);

    char rdone[128];
    snprintf(rdone, sizeof(rdone), log_received_all_done_fmt, ctx.id);
    logf_flush(ev, "%s", rdone);
    fprintf(stdout, "%s", rdone);
    fflush(stdout);

    // Дожидаемся детей
    for (int i = 1; i <= n_children; ++i) {
        int status = 0;
        waitpid(pids[i], &status, 0);
    }

    fclose(ev);
    if (pfl && pfl != stdout) fclose(pfl);
    free(pids);
    return 0;
}
