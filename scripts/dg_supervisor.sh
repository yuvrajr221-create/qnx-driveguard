#!/bin/sh
echo $$ > /tmp/sup.pid
while true; do
  /tmp/dg_monitor >> /tmp/mon.log 2>&1
  echo "monitor exited, restarting" >> /tmp/sup.log
  sleep 1
done
