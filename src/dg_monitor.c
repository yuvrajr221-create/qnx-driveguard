#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/neutrino.h>
typedef struct { uint32_t seq; float speed_kmh; float gap_m; } req_t;
typedef struct { uint32_t seq; int alert; } rep_t;
int main(void) {
    int chid = ChannelCreate(0);
    struct sched_param sp = { .sched_priority = 50 };
    int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    printf("monitor pid=%d chid=%d prio_rc=%d\n", getpid(), chid, rc);
    fflush(stdout);
    FILE *f = fopen("/tmp/dg_chan.tmp", "w");
    if (!f) return 1;
    fprintf(f, "%d %d\n", getpid(), chid);
    fclose(f);
    rename("/tmp/dg_chan.tmp", "/tmp/dg_chan");
    for (;;) {
        req_t m; rep_t r;
        int rcvid = MsgReceive(chid, &m, sizeof(m), NULL);
        if (rcvid <= 0) continue;
        float v = m.speed_kmh / 3.6f;
        r.seq = m.seq;
        r.alert = (v > 0.5f && m.gap_m / v < 2.0f) ? 1 : 0;
        MsgReply(rcvid, 0, &r, sizeof(r));
    }
}
