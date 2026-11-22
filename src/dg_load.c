#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <sys/neutrino.h>
#define N 3000
#define PERIOD_NS 2000000L
typedef struct { uint32_t seq; float speed_kmh; float gap_m; } req_t;
typedef struct { uint32_t seq; int alert; } rep_t;
static int chid, tprio = 50, tpol = SCHED_FIFO;
static volatile int stop_hogs;
static double lat[N];
static void setsched(int pol, int prio) {
    struct sched_param sp;
    memset(&sp, 0, sizeof sp);
    sp.sched_priority = prio;
    int rc = pthread_setschedparam(pthread_self(), pol, &sp);
    if (rc) fprintf(stderr, "setschedparam rc=%d\n", rc);
}
static void *hog(void *a) {
    setsched(SCHED_RR, 10);
    volatile unsigned long x = 0;
    while (!stop_hogs) x++;
    return NULL;
}
static void *server(void *a) {
    setsched(tpol, tprio);
    req_t m; rep_t r;
    for (;;) {
        int rcvid = MsgReceive(chid, &m, sizeof m, NULL);
        if (rcvid <= 0) continue;
        float v = m.speed_kmh / 3.6f;
        r.seq = m.seq;
        r.alert = (v > 0.5f && m.gap_m / v < 2.0f);
        MsgReply(rcvid, 0, &r, sizeof r);
    }
    return NULL;
}
static int cmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static double ns(struct timespec t) { return t.tv_sec * 1e9 + t.tv_nsec; }
int main(int argc, char **argv) {
    int nhog = argc > 1 ? atoi(argv[1]) : 0;
    tprio = argc > 2 ? atoi(argv[2]) : 50;
    tpol = (argc > 3 && !strcmp(argv[3], "rr")) ? SCHED_RR : SCHED_FIFO;
    chid = ChannelCreate(0);
    pthread_t t;
    for (int i = 0; i < nhog; i++) pthread_create(&t, NULL, hog, NULL);
    pthread_create(&t, NULL, server, NULL);
    setsched(tpol, tprio);
    int coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < N; i++) {
        next.tv_nsec += PERIOD_NS;
        if (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        req_t m = { i, 60.0f, 20.0f };
        rep_t r;
        MsgSend(coid, &m, sizeof m, &r, sizeof r);
        struct timespec e;
        clock_gettime(CLOCK_MONOTONIC, &e);
        lat[i] = (ns(e) - ns(next)) / 1000.0;
        if (lat[i] > PERIOD_NS / 1000.0) clock_gettime(CLOCK_MONOTONIC, &next);
    }
    stop_hogs = 1;
    qsort(lat, N, sizeof lat[0], cmp);
    printf("hogs=%d prio=%d %s N=%d us: min=%.1f p50=%.1f p99=%.1f max=%.1f\n",
           nhog, tprio, tpol == SCHED_RR ? "RR" : "FIFO", N,
           lat[0], lat[N / 2], lat[N * 99 / 100], lat[N - 1]);
    return 0;
}
