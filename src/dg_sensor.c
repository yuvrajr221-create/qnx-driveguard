#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/neutrino.h>
typedef struct { uint32_t seq; float speed_kmh; float gap_m; float closing_ms; } req_t;
typedef struct { uint32_t seq; int level; float ttc; } rep_t;
static int attach(void) {
    int pid, chid;
    FILE *f = fopen("/tmp/dg_chan", "r");
    if (!f) return -1;
    int n = fscanf(f, "%d %d", &pid, &chid);
    fclose(f);
    if (n != 2) return -1;
    return ConnectAttach(0, pid, chid, _NTO_SIDE_CHANNEL, 0);
}
static void out(float s, float g, double lat, int up, int lv, float ttc, int brk, double jit) {
    FILE *f = fopen("/tmp/stats.tmp", "w");
    if (!f) return;
    fprintf(f, "{\"speed\":%.1f,\"gap\":%.1f,\"lat\":%.1f,\"up\":%d,\"level\":%d,\"ttc\":%.1f,\"brake\":%d,\"jit\":%.0f}\n", s, g, lat, up, lv, ttc, brk, jit);
    fclose(f);
    rename("/tmp/stats.tmp", "/tmp/stats.json");
}
static float lead_speed(double t) {
    double c = t - 30.0 * (long)(t / 30.0);
    if (c < 8) return 22;
    if (c < 11) return 22 - (c - 8) * (14.0 / 3.0);
    if (c < 19) return 8;
    if (c < 23) return 8 + (c - 19) * 3.5;
    return 22;
}
int main(void) {
    int coid = -1, cnt = 0, lv = 0, brake = 0, tick = 0;
    uint32_t i = 0;
    double sum = 0, t = 0;
    float ego = 22.0f, gap = 45.0f, ttc = 99.0f;
    struct timespec prev;
    clock_gettime(CLOCK_MONOTONIC, &prev);
    double jit = 0;
    int tchid = ChannelCreate(0);
    int tcoid = ConnectAttach(0, 0, tchid, _NTO_SIDE_CHANNEL, 0);
    struct sigevent ev;
    SIGEV_PULSE_INIT(&ev, tcoid, SIGEV_PULSE_PRIO_INHERIT, _PULSE_CODE_MINAVAIL, 0);
    timer_t tid;
    if (timer_create(CLOCK_MONOTONIC, &ev, &tid) == -1) { perror("timer_create"); return 1; }
    struct itimerspec its = { { 0, 10000000 }, { 0, 10000000 } };
    timer_settime(tid, 0, &its, NULL);
    for (;; i++) {
        struct _pulse pu;
        if (MsgReceive(tchid, &pu, sizeof(pu), NULL) != 0) continue;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double dt = (now.tv_sec - prev.tv_sec) + (now.tv_nsec - prev.tv_nsec) / 1e9;
        prev = now;
        { double e = dt - 0.010; if (e < 0) e = -e; if (e > jit) jit = e; }
        if (dt > 0.1) dt = 0.1;
        t += dt;
        float lead = lead_speed(t);
        float target = gap > 50 ? 26.0f : 22.0f, acc;
        if (lv == 2) acc = -7.0f;
        else if (lv == 1) acc = -3.0f;
        else acc = ego < target ? 1.5f : (ego > target + 0.5f ? -1.0f : 0.0f);
        brake = lv > 0;
        ego += acc * dt;
        if (ego < 0) ego = 0;
        gap += (lead - ego) * dt;
        if (gap < 2) gap = 2;
        if (coid < 0) coid = attach();
        if (coid >= 0) {
            req_t m = { i, ego * 3.6f, gap, ego - lead };
            rep_t r;
            struct timespec a, b;
            clock_gettime(CLOCK_MONOTONIC, &a);
            int rc = MsgSend(coid, &m, sizeof(m), &r, sizeof(r));
            clock_gettime(CLOCK_MONOTONIC, &b);
            if (rc == -1) { ConnectDetach(coid); coid = -1; }
            else {
                lv = r.level; ttc = r.ttc;
                sum += (b.tv_sec - a.tv_sec) * 1e6 + (b.tv_nsec - a.tv_nsec) / 1e3;
                cnt++;
            }
        }
        if (coid < 0) { lv = 1; ttc = 99.0f; }
        if (++tick >= 50) {
            out(ego * 3.6f, gap, cnt ? sum / cnt : 0, coid >= 0, lv, ttc, brake, jit * 1e6);
            sum = 0; cnt = 0; tick = 0; jit = 0;
        }
    }
}
