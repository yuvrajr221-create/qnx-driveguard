#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/neutrino.h>
typedef struct { uint32_t seq; float speed_kmh; float gap_m; } req_t;
typedef struct { uint32_t seq; int alert; } rep_t;
static int attach(void) {
    int pid, chid;
    FILE *f = fopen("/tmp/dg_chan", "r");
    if (!f) return -1;
    int n = fscanf(f, "%d %d", &pid, &chid);
    fclose(f);
    if (n != 2) return -1;
    return ConnectAttach(0, pid, chid, _NTO_SIDE_CHANNEL, 0);
}
static void out(float s, float g, double lat, int up, int al) {
    FILE *f = fopen("/tmp/stats.tmp", "w");
    if (!f) return;
    fprintf(f, "{\"speed\":%.1f,\"gap\":%.1f,\"lat\":%.1f,\"up\":%d,\"alert\":%d}\n", s, g, lat, up, al);
    fclose(f);
    rename("/tmp/stats.tmp", "/tmp/stats.json");
}
int main(void) {
    int coid = -1, cnt = 0, al = 0;
    uint32_t i = 0;
    double sum = 0;
    for (;; i++) {
        float s = 62 + 34 * sin(i / 160.0) + 7 * sin(i / 40.0);
        float g = 32 + 27 * sin(i / 100.0 + 1);
        if (g < 5) g = 5;
        if (coid < 0) coid = attach();
        if (coid < 0) { out(s, g, 0, 0, 0); usleep(200000); continue; }
        req_t m = { i, s, g };
        rep_t r;
        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        int rc = MsgSend(coid, &m, sizeof(m), &r, sizeof(r));
        clock_gettime(CLOCK_MONOTONIC, &b);
        if (rc == -1) { ConnectDetach(coid); coid = -1; out(s, g, 0, 0, 0); usleep(200000); continue; }
        sum += (b.tv_sec - a.tv_sec) * 1e6 + (b.tv_nsec - a.tv_nsec) / 1e3;
        cnt++;
        al = r.alert;
        if (cnt >= 50) { out(s, g, sum / cnt, 1, al); sum = 0; cnt = 0; }
        usleep(10000);
    }
}
