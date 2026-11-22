# DriveGuard

A real-time vehicle collision monitor that runs on QNX 8.0, with a live, colourful dashboard.

Category: Automotive & Mobility

## What it does

- `dg_sensor` (QNX process) simulates vehicle speed and the gap to the car ahead, and sends every sample to the monitor with QNX message passing (`MsgSend`).
- `dg_monitor` (QNX process, SCHED_FIFO priority 50) computes time to collision and replies with an alert flag (`MsgReceive`, `MsgReply`). The alert threshold is 2 seconds.
- `dg_supervisor.sh` (runs on QNX) starts the monitor again if it dies.
- `gateway.py` (runs on the Linux host) reads the sensor output from the QNX VM over SSH and serves `/api/stats`. `/api/kill` runs `slay dg_monitor` inside the VM, so the dashboard can inject a real fault.
- `web/index.html` is the dashboard. It shows speed, gap, time to collision, monitor reply time, process health and an event log. The Kill button shows the supervisor recovering the monitor.

## Where QNX is used

- `src/dg_monitor.c`: `ChannelCreate`, `MsgReceive`, `MsgReply`, `pthread_setschedparam` with SCHED_FIFO 50
- `src/dg_sensor.c`: `ConnectAttach`, `MsgSend`, `ConnectDetach`
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

## Limitations

- Sensor values are simulated.
- Dashboard and gateway run on the Linux host, not on QNX.
- The SDP is the unpatched 8.0.0 baseline.

## License

MIT
