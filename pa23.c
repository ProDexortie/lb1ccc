#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>

#include "common.h"
#include "pa2345.h"
#include "banking.h"
#include "ipc.h"

/*
 IPC: полносвязная топология однонаправленных каналов i->j.
 Неблокирующие read/write с busy-poll обработкой.
*/

typedef struct {
    int rd; // read end for channel (src -> this)
    int wr; // write end for channel (this -> dst)
} ChannelFD;

typedef struct {
    local_id id;           // my local id
    int nprocs;            // total processes including parent (0..Nchildren)
    ChannelFD ch[ MAX_PROCESS_ID + 1 ][ MAX_PROCESS_ID + 1 ]; // [src][dst]
    FILE* events;          // events.log
    FILE* pipes;           // pipes.log
} Ctx;

// ---------- helpers ----------

static void set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) fl = 0;
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void safe_close(int fd) {
    if (fd >= 0) close(fd);
}

static void log_open(Ctx* ctx) {
    ctx->events = fopen(events_log, "a");
    ctx->pipes  = fopen(pipes_log,  "a");
}

static void log_close(Ctx* ctx) {
    if (ctx->events) fclose(ctx->events);
    if (ctx->pipes)  fclose(ctx->pipes);
    ctx->events = NULL;
    ctx->pipes = NULL;
}

static void eprintf(Ctx* ctx, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (ctx->events) {
        vfprintf(ctx->events, fmt, ap);
        fflush(ctx->events);
    }
    va_end(ap);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    fflush(stdout);
    va_end(ap);
}

static void epfprintf(Ctx* ctx, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (ctx->events) {
        vfprintf(ctx->events, fmt, ap);
        fflush(ctx->events);
    }
    va_end(ap);
}

static void pprintf(Ctx* ctx, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (ctx->pipes) {
        vfprintf(ctx->pipes, fmt, ap);
        fflush(ctx->pipes);
    }
    va_end(ap);
}

static void msg_init(Message* m, MessageType t, const void* payload, uint16_t len) {
    m->s_header.s_magic = MESSAGE_MAGIC;
    m->s_header.s_type  = t;
    m->s_header.s_payload_len = len;
    m->s_header.s_local_time  = get_physical_time();
    if (len && payload) {
        memcpy(m->s_payload, payload, len);
    }
}

static ssize_t write_all(int fd, const void* buf, size_t n) {
    const char* p = (const char*)buf;
    size_t done = 0;
    while (done < n) {
        ssize_t r = write(fd, p + done, n - done);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // busy
                continue;
            }
            return -1;
        }
        done += (size_t)r;
    }
    return (ssize_t)done;
}

static ssize_t read_all(int fd, void* buf, size_t n) {
    char* p = (char*)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r == 0) {
            // pipe closed
            return -1;
        }
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // busy
                continue;
            }
            return -1;
        }
        got += (size_t)r;
    }
    return (ssize_t)got;
}

// ---------- ipc.h required functions ----------

static int get_wr_fd(Ctx* ctx, local_id dst) {
    return ctx->ch[ctx->id][dst].wr;
}

static int get_rd_fd(Ctx* ctx, local_id from) {
    return ctx->ch[from][ctx->id].rd;
}

int send(void * self, local_id dst, const Message * msg) {
    Ctx* ctx = (Ctx*)self;
    if (dst == ctx->id) return -1;
    int fd = get_wr_fd(ctx, dst);
    if (fd < 0) return -1;
    // write header then payload
    if (write_all(fd, &msg->s_header, sizeof(MessageHeader)) < 0) return -1;
    if (msg->s_header.s_payload_len > 0) {
        if (write_all(fd, msg->s_payload, msg->s_header.s_payload_len) < 0) return -1;
    }
    return 0;
}

int send_multicast(void * self, const Message * msg) {
    Ctx* ctx = (Ctx*)self;
    for (local_id i = 0; i < ctx->nprocs; ++i) {
        if (i == ctx->id) continue;
        if (send(self, i, msg) != 0) return -1;
    }
    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    Ctx* ctx = (Ctx*)self;
    if (from == ctx->id) return -1;
    int fd = get_rd_fd(ctx, from);
    if (fd < 0) return -1;
    // read header
    if (read_all(fd, &msg->s_header, sizeof(MessageHeader)) < 0) return -1;
    if (msg->s_header.s_magic != MESSAGE_MAGIC) return -1;
    uint16_t len = msg->s_header.s_payload_len;
    if (len > 0) {
        if (read_all(fd, msg->s_payload, len) < 0) return -1;
    }
    return 0;
}

int receive_any(void * self, Message * msg) {
    Ctx* ctx = (Ctx*)self;
    for (;;) {
        for (local_id from = 0; from < ctx->nprocs; ++from) {
            if (from == ctx->id) continue;
            int fd = get_rd_fd(ctx, from);
            if (fd < 0) continue;
            // try to read header
            ssize_t r = read(fd, &msg->s_header, sizeof(MessageHeader));
            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                else continue;
            } else if (r == 0) {
                // closed
                continue;
            } else if ((size_t)r < sizeof(MessageHeader)) {
                // read remaining
                size_t left = sizeof(MessageHeader) - (size_t)r;
                if (read_all(fd, ((char*)&msg->s_header) + r, left) < 0) {
                    return -1;
                }
            }
            if (msg->s_header.s_magic != MESSAGE_MAGIC) {
                return -1;
            }
            uint16_t len = msg->s_header.s_payload_len;
            if (len > 0) {
                if (read_all(fd, msg->s_payload, len) < 0) return -1;
            }
            return 0;
        }
        // spin
    }
}

// ---------- pipes build/teardown ----------

static void build_pipes(Ctx* root, int nchildren) {
    int nprocs = nchildren + 1;
    root->nprocs = nprocs;
    for (int i = 0; i < nprocs; ++i) {
        for (int j = 0; j < nprocs; ++j) {
            root->ch[i][j].rd = -1;
            root->ch[i][j].wr = -1;
        }
    }
    // create two pipes for each unordered pair (i, j): i->j and j->i
    for (int i = 0; i < nprocs; ++i) {
        for (int j = i + 1; j < nprocs; ++j) {
            int p1[2], p2[2];
            if (pipe(p1) < 0 || pipe(p2) < 0) {
                perror("pipe");
                exit(1);
            }
            // i -> j uses p1: i writes, j reads
            root->ch[i][j].wr = p1[1];
            root->ch[j][i].rd = p1[0];
            // j -> i uses p2
            root->ch[j][i].wr = p2[1];
            root->ch[i][j].rd = p2[0];
        }
    }
}

static void close_unused(Ctx* ctx) {
    // keep only: for my id K: wr[id][*] and rd[*][id]
    for (int i = 0; i < ctx->nprocs; ++i) {
        for (int j = 0; j < ctx->nprocs; ++j) {
            if (i == j) {
                if (ctx->ch[i][j].rd >= 0) { safe_close(ctx->ch[i][j].rd); ctx->ch[i][j].rd=-1; }
                if (ctx->ch[i][j].wr >= 0) { safe_close(ctx->ch[i][j].wr); ctx->ch[i][j].wr=-1; }
                continue;
            }
            // read end belongs to (src, this)
            if (j == ctx->id) {
                // keep rd[i][this]
                if (ctx->ch[i][j].rd >= 0) set_nonblock(ctx->ch[i][j].rd);
            } else {
                if (ctx->ch[i][j].rd >= 0) { pprintf(ctx, "Closing rd %d->%d fd %d\n", i, j, ctx->ch[i][j].rd); safe_close(ctx->ch[i][j].rd); ctx->ch[i][j].rd=-1; }
            }
            // write end belongs to (this, dst)
            if (i == ctx->id) {
                // keep wr[this][j]
                if (ctx->ch[i][j].wr >= 0) set_nonblock(ctx->ch[i][j].wr);
            } else {
                if (ctx->ch[i][j].wr >= 0) { pprintf(ctx, "Closing wr %d->%d fd %d\n", i, j, ctx->ch[i][j].wr); safe_close(ctx->ch[i][j].wr); ctx->ch[i][j].wr=-1; }
            }
        }
    }
}

// ---------- Banking/History ----------

typedef struct {
    balance_t balance;
    uint8_t last_filled_t; // last time index filled in history
    BalanceHistory hist;
} Account;

static void hist_init(Account* a, local_id id, balance_t init) {
    a->balance = init;
    a->hist.s_id = id;
    a->hist.s_history_len = 1;
    a->last_filled_t = 0;
    a->hist.s_history[0].s_time = 0;
    a->hist.s_history[0].s_balance = init;
    a->hist.s_history[0].s_balance_pending_in = 0;
}

static void hist_fill_to(Account* a, timestamp_t t) {
    if (t > MAX_T) t = MAX_T;
    // fill gaps from last_filled_t+1 to t with same balance
    for (uint16_t x = (uint16_t)a->last_filled_t + 1; x <= (uint16_t)t; ++x) {
        a->hist.s_history[x].s_time = (timestamp_t)x;
        a->hist.s_history[x].s_balance = a->balance;
        a->hist.s_history[x].s_balance_pending_in = 0;
    }
    a->last_filled_t = (uint8_t)t;
    if (a->hist.s_history_len < (uint8_t)(t + 1)) {
        a->hist.s_history_len = (uint8_t)(t + 1);
    }
}

static void hist_on_change(Account* a) {
    timestamp_t t = get_physical_time();
    if (t > MAX_T) t = MAX_T;
    hist_fill_to(a, t);
}

// size to send for BalanceHistory with compact s_history
static uint16_t bh_wire_size(const BalanceHistory* h) {
    uint16_t used = (uint16_t)h->s_history_len;
    uint16_t base = (uint16_t)offsetof(BalanceHistory, s_history);
    uint16_t part = used * (uint16_t)sizeof(BalanceState);
    return base + part;
}

// ---------- Protocol ----------

static void send_started(Ctx* ctx, Account* acc) {
    char buf[128];
    int ts = get_physical_time();
    snprintf(buf, sizeof(buf), log_started_fmt, ts, ctx->id, getpid(), getppid(), acc->balance);
    Message m;
    msg_init(&m, STARTED, buf, (uint16_t)strlen(buf));
    epfprintf(ctx, "%s", buf);
    send_multicast(ctx, &m);
}

static void send_done(Ctx* ctx, Account* acc) {
    char buf[128];
    int ts = get_physical_time();
    snprintf(buf, sizeof(buf), log_done_fmt, ts, ctx->id, acc->balance);
    Message m;
    msg_init(&m, DONE, buf, (uint16_t)strlen(buf));
    epfprintf(ctx, "%s", buf);
    send_multicast(ctx, &m);
}

static void log_all_started(Ctx* ctx) {
    int ts = get_physical_time();
    char buf[128];
    snprintf(buf, sizeof(buf), log_received_all_started_fmt, ts, ctx->id);
    epfprintf(ctx, "%s", buf);
}

static void log_all_done(Ctx* ctx) {
    int ts = get_physical_time();
    char buf[128];
    snprintf(buf, sizeof(buf), log_received_all_done_fmt, ts, ctx->id);
    epfprintf(ctx, "%s", buf);
}

static void log_transfer_out(Ctx* ctx, balance_t amount, local_id dst) {
    int ts = get_physical_time();
    char buf[128];
    snprintf(buf, sizeof(buf), log_transfer_out_fmt, ts, ctx->id, amount, dst);
    epfprintf(ctx, "%s", buf);
}

static void log_transfer_in(Ctx* ctx, balance_t amount, local_id src) {
    int ts = get_physical_time();
    char buf[128];
    snprintf(buf, sizeof(buf), log_transfer_in_fmt, ts, ctx->id, amount, src);
    epfprintf(ctx, "%s", buf);
}

// ---------- transfer() for parent ----------

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount)
{
    Ctx* ctx = (Ctx*)parent_data;
    TransferOrder order = {.s_src=src, .s_dst=dst, .s_amount=amount};
    Message m;
    msg_init(&m, TRANSFER, &order, (uint16_t)sizeof(order));
    // send to Csrc
    if (send(parent_data, src, &m) != 0) {
        fprintf(stderr, "Parent failed to send TRANSFER to %d\n", src);
        exit(1);
    }
    // wait ACK from dst
    for (;;) {
        Message rcv;
        if (receive_any(parent_data, &rcv) == 0) {
            if (rcv.s_header.s_type == ACK) {
                // Accept any ACK (simple model)
                break;
            }
            // ignore other types
        }
    }
}

// ---------- Parent and Child routines ----------

static void parent_loop(Ctx* ctx, int nchildren, balance_t* init_balances) {
    (void)init_balances;
    // close all unrelated fds
    close_unused(ctx);

    // receive STARTED from all children
    int started = 0;
    while (started < nchildren) {
        Message m;
        if (receive_any(ctx, &m) == 0) {
            if (m.s_header.s_type == STARTED) {
                started++;
            }
        }
    }
    log_all_started(ctx);

    // robbery
    bank_robbery(ctx, (local_id)nchildren);

    // send STOP to all children
    Message stop;
    msg_init(&stop, STOP, NULL, 0);
    send_multicast(ctx, &stop);

    // wait DONE from all children
    int done = 0;
    while (done < nchildren) {
        Message m;
        if (receive_any(ctx, &m) == 0) {
            if (m.s_header.s_type == DONE) {
                done++;
            }
        }
    }

    // receive BALANCE_HISTORY from all children
    AllHistory all = {0};
    all.s_history_len = (uint8_t)nchildren;
    int got_hist = 0;
    while (got_hist < nchildren) {
        Message m;
        if (receive_any(ctx, &m) == 0) {
            if (m.s_header.s_type == BALANCE_HISTORY) {
                // parse BalanceHistory
                BalanceHistory bh = {0};
                size_t len = m.s_header.s_payload_len;
                if (len >= offsetof(BalanceHistory, s_history)) {
                    memcpy(&bh, m.s_payload, len);
                    local_id id = bh.s_id;
                    all.s_history[id] = bh;
                    got_hist++;
                }
            }
        }
    }

    print_history(&all);

    // wait children exit
    for (int i = 0; i < nchildren; ++i) {
        wait(NULL);
    }
}

static void child_loop(Ctx* ctx, balance_t init_balance, int nchildren) {
    close_unused(ctx);

    Account acc;
    hist_init(&acc, ctx->id, init_balance);

    // STARTED
    send_started(ctx, &acc);

    // receive STARTED from other children (nchildren - 1)
    int need_started = nchildren - 1;
    int got_started = 0;
    while (got_started < need_started) {
        Message m;
        if (receive_any(ctx, &m) == 0) {
            if (m.s_header.s_type == STARTED) got_started++;
            else {
                // handle transfers if they appear
                if (m.s_header.s_type == TRANSFER) {
                    TransferOrder ord;
                    memcpy(&ord, m.s_payload, sizeof(ord));
                    if (ord.s_dst == ctx->id) {
                        acc.balance += ord.s_amount;
                        hist_on_change(&acc);
                        log_transfer_in(ctx, ord.s_amount, ord.s_src);
                        Message ack; msg_init(&ack, ACK, NULL, 0);
                        send(ctx, PARENT_ID, &ack);
                    } else if (ord.s_src == ctx->id) {
                        acc.balance -= ord.s_amount;
                        hist_on_change(&acc);
                        log_transfer_out(ctx, ord.s_amount, ord.s_dst);
                        Message fwd; msg_init(&fwd, TRANSFER, &ord, (uint16_t)sizeof(ord));
                        send(ctx, ord.s_dst, &fwd);
                    } else {
                        // ignore
                    }
                }
            }
        }
    }
    log_all_started(ctx);

    bool stop_received = false;
    int done_from_others = 0;
    int need_done_from_others = nchildren - 1;

    for (;;) {
        Message m;
        if (receive_any(ctx, &m) != 0) continue;
        switch (m.s_header.s_type) {
            case TRANSFER: {
                TransferOrder ord;
                memcpy(&ord, m.s_payload, sizeof(ord));
                if (ord.s_src == ctx->id) {
                    acc.balance -= ord.s_amount;
                    hist_on_change(&acc);
                    log_transfer_out(ctx, ord.s_amount, ord.s_dst);
                    Message fwd; msg_init(&fwd, TRANSFER, &ord, (uint16_t)sizeof(ord));
                    send(ctx, ord.s_dst, &fwd);
                } else if (ord.s_dst == ctx->id) {
                    acc.balance += ord.s_amount;
                    hist_on_change(&acc);
                    log_transfer_in(ctx, ord.s_amount, ord.s_src);
                    Message ack; msg_init(&ack, ACK, NULL, 0);
                    send(ctx, PARENT_ID, &ack);
                } else {
                    // ignore
                }
            } break;
            case STOP: {
                stop_received = true;
                send_done(ctx, &acc);
            } break;
            case DONE: {
                done_from_others++;
                if (stop_received && done_from_others >= need_done_from_others) {
                    hist_fill_to(&acc, get_physical_time());
                    Message histmsg;
                    uint16_t plen = bh_wire_size(&acc.hist);
                    msg_init(&histmsg, BALANCE_HISTORY, &acc.hist, plen);
                    send(ctx, PARENT_ID, &histmsg);
                    log_all_done(ctx);
                    return;
                }
            } break;
            default:
                break;
        }
    }
}

// ---------- main ----------

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s -p N S1 S2 ... SN\n", prog);
}

int main(int argc, char * argv[])
{
    // Parse args
    if (argc < 3) { usage(argv[0]); return 1; }
    int nchildren = 0;
    if (strcmp(argv[1], "-p") == 0 && argc >= 3) {
        nchildren = atoi(argv[2]);
        if (nchildren < 1 || nchildren > MAX_PROCESS_ID) {
            fprintf(stderr, "Invalid number of children\n");
            return 1;
        }
    } else {
        usage(argv[0]);
        return 1;
    }
    if (argc < 3 + nchildren) {
        fprintf(stderr, "Initial balances missing\n");
        return 1;
    }
    balance_t balances[MAX_PROCESS_ID + 1] = {0};
    for (int i = 1; i <= nchildren; ++i) {
        balances[i] = (balance_t)atoi(argv[2 + i]);
        if (balances[i] < 0) balances[i] = 0;
    }

    Ctx root = {0};
    root.id = PARENT_ID;
    log_open(&root);
    build_pipes(&root, nchildren);

    // Fork children
    for (int i = 1; i <= nchildren; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            Ctx ctx = root;
            ctx.id = (local_id)i;
            // open logs per process (separate FILE* handles)
            log_open(&ctx);
            child_loop(&ctx, balances[i], nchildren);
            log_close(&ctx);
            // close all fds inherited (best-effort)
            for (int a = 0; a < ctx.nprocs; ++a)
                for (int b = 0; b < ctx.nprocs; ++b) {
                    if (ctx.ch[a][b].rd >= 0) close(ctx.ch[a][b].rd);
                    if (ctx.ch[a][b].wr >= 0) close(ctx.ch[a][b].wr);
                }
            _exit(0);
        }
    }

    // Parent
    Ctx pctx = root;
    pctx.id = PARENT_ID;
    parent_loop(&pctx, nchildren, balances);

    // parent close logs
    log_close(&pctx);
    // close all fds
    for (int a = 0; a < pctx.nprocs; ++a)
        for (int b = 0; b < pctx.nprocs; ++b) {
            if (pctx.ch[a][b].rd >= 0) close(pctx.ch[a][b].rd);
            if (pctx.ch[a][b].wr >= 0) close(pctx.ch[a][b].wr);
        }

    return 0;
}