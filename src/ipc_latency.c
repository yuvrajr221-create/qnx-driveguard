#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <sys/neutrino.h>

#define N 10000

typedef struct { uint32_t seq; float speed_kmh; float gap_m; } sensor_msg_t;
typedef struct { uint32_t seq; int alert; } reply_t;

static int chid;

static void *monitor(void *arg) {
    sensor_msg_t m;
    reply_t r;
    for (;;) {
        int rcvid = MsgReceive(chid, &m, sizeof(m), NULL);
        if (rcvid <= 0) continue;
        float v = m.speed_kmh / 3.6f;
        r.seq = m.seq;
        r.alert = (v > 0.1f && (m.gap_m / v) < 2.0f) ? 1 : 0;
        MsgReply(rcvid, 0, &r, sizeof(r));
    }
    return NULL;
}

int main(void) {
    pthread_t t;
    pthread_attr_t a;
    struct sched_param sp;
    chid = ChannelCreate(0);
    pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&a, SCHED_FIFO);
    sp.sched_priority = 50;
    pthread_attr_setschedparam(&a, &sp);
    pthread_create(&t, &a, monitor, NULL);

    int coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    uint64_t min = UINT64_MAX, max = 0, sum = 0;
    int alerts = 0;
    for (uint32_t i = 0; i < N; i++) {
        sensor_msg_t m = { i, 40.0f + (i % 80), 60.0f - (i % 50) };
        reply_t r;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        MsgSend(coid, &m, sizeof(m), &r, sizeof(r));
        clock_gettime(CLOCK_MONOTONIC, &t1);
        uint64_t ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ULL + t1.tv_nsec - t0.tv_nsec;
        if (ns < min) min = ns;
        if (ns > max) max = ns;
        sum += ns;
        alerts += r.alert;
    }
    printf("N=%d alerts=%d\n", N, alerts);
    printf("round-trip us: min=%.1f avg=%.1f max=%.1f\n", min / 1000.0, sum / 1000.0 / N, max / 1000.0);
    return 0;
}
