#!/bin/bash
K="-p 2200 -i $HOME/.ssh/id_ed25519 -o StrictHostKeyChecking=accept-new"
D="$(cd "$(dirname "$0")" && pwd)"
sudo chmod 666 /dev/kvm
if ! ssh -n $K -o ConnectTimeout=3 root@localhost true 2>/dev/null; then
  bash $HOME/qnxvm/run.sh; sleep 30
fi
ssh -n $K root@localhost '[ -f /tmp/sup.pid ] && kill $(cat /tmp/sup.pid); slay dg_monitor; slay dg_sensor; sleep 1; true'
for f in $HOME/dg_monitor $HOME/dg_sensor $D/dg_supervisor.sh; do
  b=$(basename $f)
  ssh $K root@localhost "cat > /tmp/$b && chmod +x /tmp/$b" < $f
done
ssh -n $K root@localhost '/tmp/dg_supervisor.sh </dev/null >/dev/null 2>&1 & sleep 1; /tmp/dg_sensor </dev/null >/dev/null 2>&1 & sleep 3; cat /tmp/stats.json'
