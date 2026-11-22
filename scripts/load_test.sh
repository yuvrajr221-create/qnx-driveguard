#!/bin/bash
K="-p 2200 -i $HOME/.ssh/id_ed25519 -o StrictHostKeyChecking=accept-new"
source ~/qnx800/qnxsdp-env.sh
cd "$(dirname "$0")/.."
qcc -Vgcc_ntox86_64 -o $HOME/dg_load src/dg_load.c || exit 1
ssh -n $K root@localhost '[ -f /tmp/sup.pid ] && kill $(cat /tmp/sup.pid); slay dg_sensor; slay dg_monitor; true'
ssh $K root@localhost "cat > /tmp/dg_load && chmod +x /tmp/dg_load" < $HOME/dg_load
for args in "0 50 fifo" "4 50 fifo" "4 10 rr"; do
  ssh -n $K root@localhost "/tmp/dg_load $args"
done
