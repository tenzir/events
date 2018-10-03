#!/bin/bash

function launch_ping() {
  port=$1
  num_relays=$2
  for payload in 0 1 10 100 1000 10000 100000; do
    echo "start ping for $num_relays relays with payload $payload connecting to port $port"
    out=$(../build/release/bin/broker-node ${verbose+'-v'} -N "ping" -p "[<tcp://[::1]:$port>]" -t foobar -m "'ping'" -n ${iterations} -s $payload)
    while read -r line; do
      echo "$num_relays,$payload,$line" >> latency.csv
    done <<< "$out"
  done
}

function launch_relay() {
  listen=$1
  peer=$2
  echo "start relay ${listen} <-> ${peer}" 1>&2
  ../build/release/bin/broker-node -N "relay_${listen}" ${verbose+'-v'} -l ${listen} -t foobar -m "'relay'" -p "[<tcp://[::1]:${peer}>]" &
}

function launch_pong() {
  listen=$1
  echo "start pong at port ${listen}" 1>&2
  ../build/release/bin/broker-node -N "pong" ${verbose+'-v'} -l ${listen} -t foobar -m "'pong'" & 2> pong.log
}

function run_test() {
  num_relays=$1
  echo $num_relays
  launch_pong $port
  ((port++))
  for node in $(seq 1 $num_relays); do
    echo "launching relay $port"
    launch_relay $port $((port-1))
    ((port++))
  done
  launch_ping $((port-1)) $num_relays
}

USAGE="usage: $0 [options] <min-num-relays> <max-num-relays>"

append=false
port=4001
iterations=100
ret=0
while getopts "n:p:ahv" flag; do
case "$flag" in
    n) iterations=$OPTARG;;
    p) port=$OPTARG;;
    a) append=true;;
    h) help_=true;;
    v) verbose=true;;
    \?) help_=true; ret=1;;
esac
done
shift $((OPTIND-1))

if [ "$help_" = true ] || [ "$#" -lt 2 ]; then
  echo $USAGE
  exit $ret;
fi

if [ "$append" = false ]; then
  rm -f latency.csv
fi

for i in $(seq $1 $2); do
  run_test $i
done

# cleanup
killall broker-node

exit $ret
