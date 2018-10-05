#!/bin/bash

function cleanup {
  $(killall broker-node 2> /dev/null)
  $(killall broker-pipe 2> /dev/null)
}
trap cleanup INT TERM EXIT

function launch_relay() {
  listen=$1
  peer=$2
  echo "start relay ${listen} <-> ${peer}" 1>&2
  $broker_node -N "relay_${listen}" ${verbose:+'-v'} -l ${listen} -t '/bench/throughput' -m "relay" -p "[<tcp://[127.0.0.1]:${peer}>]" &
}

function launch_subscriber {
  listen=$1
  num_relays=$2
  bytes=$3
  echo "start subscriber at ${listen}" 1>&2
  $broker_pipe -l ${listen} -t '/bench/throughput' --mode="subscribe" --impl="blocking" --rate | xargs -i echo "$num_relays,$payload,{}" >> throughput.csv &
}

function launch_publisher {
  peer=$1
  bytes=$2
  echo "start publisher connecting to ${peer}, sending ${bytes} bytes per message indefinitely" 1>&2
  ./generate ${bytes} | $broker_pipe --peers="[\"127.0.0.1:${peer}\"]" -t '/bench/throughput' --mode="publish" --impl="blocking" &
}

function bench() {
  num_relays=$1
  bytes=$2
  echo "relays: $num_relays, bytes: $bytes"
  launch_subscriber $port $num_relays $bytes
  sleep 1
  ((port++))
  for node in $(seq 1 $num_relays); do
    launch_relay $port $((port-1))
    sleep 1
    ((port++))
  done
  launch_publisher $((port-1)) ${bytes}

  sleep 15
  cleanup
}

USAGE="usage: $0 [options] <min-num-relays> <max-num-relays>"

unset verbose
unset help_
broker_node=../../broker/build/bin/broker-node
broker_pipe=../../broker/build/bin/broker-pipe
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
  rm -f throughput.csv
  echo "relays,payload,messages" > throughput.csv
fi

make generate

for relays in $(seq $1 $2); do
  for payload in 0 1 10 100 1000 10000 100000 1000000 10000000; do
  #for payload in 0 1 10 100; do
    bench $relays $payload
  done
done


exit $ret
