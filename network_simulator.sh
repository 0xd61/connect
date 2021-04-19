#!/usr/bin/env bash
INTERFACE=lo
DELAY=100ms
GITTER=40ms
LOSS=30%

if [ "$1" == "start" ]; then
  sudo tc qdisc add dev ${INTERFACE} root netem delay ${DELAY} ${GITTER} loss ${LOSS}
elif [ "$1" == "stop" ]; then
  sudo tc qdisc del dev ${INTERFACE} root netem
else
  sudo tc qdisc show dev ${INTERFACE} root
fi
