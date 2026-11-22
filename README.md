# DriveGuard

A real-time vehicle collision monitor that runs on QNX 8.0, with a live, colourful dashboard.

Category: Automotive & Mobility

## What it does

- `dg_sensor` (QNX process) runs a small car-following simulation: a lead car brakes and speeds up on a 30 second cycle, and our car brakes as the monitor tells it to. It is driven by a QNX periodic timer that delivers a pulse every 10 ms (`timer_create`, `SIGEV_PULSE_INIT`) and reports the largest period error. It sends every step to the monitor with QNX message passing (`MsgSend`). If the monitor is down it falls back to gentle braking.
- `dg_monitor` (QNX process, SCHED_FIFO priority 50) computes time to collision (gap divided by closing speed) and replies with a level: 0 safe, 1 warning (under 3 s), 2 emergency (under 2 s). It also looks at the time gap to the car ahead (`MsgReceive`, `MsgReply`).
- `dg_supervisor.sh` (runs on QNX) starts the monitor again if it dies.
- `gateway.py` (runs on the Linux host) reads the sensor output from the QNX VM over SSH and serves `/api/stats`. `/api/kill` runs `slay dg_monitor` inside the VM, so the dashboard can inject a real fault.
- `web/index.html` is the dashboard. It shows speed, gap, time to collision, monitor reply time, process health and an event log. The Kill button shows the supervisor recovering the monitor.

How we set up QNX 8.0 with only a phone: see [docs/TUTORIAL.md](docs/TUTORIAL.md).

## Where QNX is used

- `src/dg_monitor.c`: `ChannelCreate`, `MsgReceive`, `MsgReply`, `pthread_setschedparam` with SCHED_FIFO 50
- `src/dg_sensor.c`: `ConnectAttach`, `MsgSend`, `ConnectDetach`, `timer_create` with `SIGEV_PULSE_INIT`, pulse delivery through `MsgReceive`
- `src/dg_load.c`: priority load test (`SCHED_FIFO`, `SCHED_RR`, `clock_nanosleep`, `MsgSend`)
- `src/ipc_latency.c`: MsgSend/MsgReceive latency benchmark between two threads
- `scripts/dg_supervisor.sh`: runs inside the QNX VM
- `scripts/run_qemu.sh`: boots the QNX image built with `mkqnximage`

## Tested setup

- QNX SDP 8.0 (baseline 8.0.0, QNX Software Center 2.0.4), non-commercial license
- Host: Ubuntu 24.04 x86_64 (GitHub Codespaces, 4 cores, KVM available)
- Target: QEMU x86_64, image built with `mkqnximage`
- Toolchain: `qcc`, variant `gcc_ntox86_64` (GCC 12.2.0)

## Build and run

```
source ~/qnx800/qnxsdp-env.sh
qcc -Vgcc_ntox86_64 -o ~/dg_monitor src/dg_monitor.c
qcc -Vgcc_ntox86_64 -o ~/dg_sensor src/dg_sensor.c -lm
qcc -Vgcc_ntox86_64 -o ~/ipc_latency src/ipc_latency.c
```

Build the QNX image once:

```
ssh-keygen -q -t ed25519 -N '' -f ~/.ssh/id_ed25519
mkdir -p ~/qnxvm && cd ~/qnxvm
mkqnximage --type=qemu --arch=x86_64 --ssh-ident=$HOME/.ssh/id_ed25519.pub --cpu=2 --ram=2G --build --run=-h
```

The build step works. The run step of `mkqnximage` needs `brctl` and did not start the VM in our container, so we start QEMU ourselves: copy `scripts/run_qemu.sh` to `~/qnxvm/run.sh` (it needs `qemu-system-x86` and access to `/dev/kvm`).

Start everything:

```
bash scripts/dg_up.sh
python3 scripts/gateway.py
```

Open port 8080 in a browser. The badge in the top right says "Live from QNX" when real data arrives.

## Measured numbers

All numbers come from a QEMU VM on a shared cloud host, not from target hardware.

- `ipc_latency`, 10,000 MsgSend round trips: min 1.6 us, avg 7.5 us, max 163.8 us
- Dashboard reply time was about 15 to 40 us in our runs. This is a different measurement: the sensor sleeps 10 ms between messages, so the monitor wakes from idle each time. Do not compare it with the benchmark above.
- Monitor kill to recovery: about 2 seconds, read from the dashboard event log. The supervisor waits 1 second before restarting by design.

## Load test (QEMU VM, 2 virtual CPUs)

`scripts/load_test.sh` builds `src/dg_load.c` and runs it inside the QNX VM. A periodic sender (every 2 ms, 3,000 samples) calls the monitor with `MsgSend`. Lateness is the time from when a sample was due until the reply came back. The loaded runs add four CPU-hog threads at priority 10 (round robin).

| Case | p50 | p99 | max |
|---|---|---|---|
| no load, monitor priority 50 FIFO | 195 us | 309 us | 1,155 us |
| 4 hogs, monitor priority 50 FIFO | 431 us | 1,027 us | 3,747 us |
| 4 hogs, monitor priority 10 RR (same as the hogs) | 10,033 us | 16,988 us | 22,060 us |

These numbers are from a single run on a shared cloud host and vary between runs: an earlier run of the first case gave p50 378 us. They are not hardware measurements, and we did not run the same test on Linux.

## Simulation results (one 30 second cycle, QEMU VM)

`scripts/measure_cycle.py` reads `/api/stats` for 35 seconds and prints the smallest gap, the smallest time to collision and how often the level changed. In each cycle the lead car brakes hard once.

| Monitor thresholds (warning / emergency) | min gap | level changes | lowest speed |
|---|---|---|---|
| 2.5 s / 1.5 s (1 run) | 4.5 m | 9 | 19.7 km/h |
| 3 s / 2 s (2 runs) | 6.1 m and 6.1 m | 6 and 6 | 24.5 and 24.3 km/h |
| 3 s / 2 s, sensor driven by the QNX timer (1 run) | 6.1 m | 4 | 23.4 km/h |

The sensor writes a snapshot every 0.5 seconds, so the true smallest gap can be a little lower than shown. The scenario is simulated, so these numbers say nothing about real traffic.

## Limitations

- Sensor values come from a simple simulated car-following model, not from real sensors.
- Dashboard and gateway run on the Linux host, not on QNX.
- The SDP is the unpatched 8.0.0 baseline.

## License

MIT
