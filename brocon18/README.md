# BroCon 2018

This repository contains code and slides from our presentation at [BroCon
2018][brocon18].

## Demo

In the demo, we use Python 3 in conjunction with
[Broker](https://github.com/bro/broker). At first we setup a [virtual Python
environment](https://docs.python.org/3/library/venv.html):

```shell
export PREFIX=$(pwd)/env
python3 -m venv $PREFIX
source $PREFIX/bin/activate
```

Then, we install Broker.

```shell
git submodule update --recursive --init
cd broker
# The next two steps won't be necessary soon when Broker has the new CAF.
cd 3rdparty/caf
git checkout master
cd -
./configure \
  --generator=Ninja \
  --build-type=Release \
  --prefix=$PREFIX \
  --with-python=$PREFIX/bin/python
cd build
ninja
ninja install
cd ../..
```

Finally, we make sure that we find the Broker Python modules without setting
`PYTHONPATH` to `$PREFIX/lib/python`:

```shell
site_packages=$(python -c "import site; print(site.getsitepackages()[0])")
cp sitecustomize.py $site_packages
```

For convenience, we provide a little script that adjust environment variables
such that Bro and VAST do not require prefixing/aliasing:

```shell
eval `./dev-path path/to/code`
```

Here, `path/to/code` is the directory with local builds of VAST and Bro.

### Scenario

The demo illustrates a scenario where Bro asks VAST for historical data via
Broker. For ease of exposition, we first model both applications as "stubs" via
Broker's Python bindings. We then replace the Bro stub with an actual Bro
script, and finally replace the VAST stub with a C++ bridge that translates
Bro's events into VAST commands.

From a data flow perspective, we have two Broker *endpoints* that communicate
in a publish/subscribe style:

1. Both Bro and VAST subscribe to a *control* and *data* topic, which default
   to `/vast/control` and `/vast/data`.
2. The VAST stub listens at a predefined port, which defaults to
   `43000`.
3. The Bro stub connects to the predefined port and initiates a peering between
   the two endpoints.
4. The Bro stub publishes a query to the control topic.
5. VAST receives the query, and publishes the results to the data channel.
6. VAST signals query completion by sending a none value.

```
                 Bro                                      VAST
                  |                                        |
  subscribe:      |                                        |  subscribe:
  - /vast/control |                                        |  - /vast/control
  - /vast/data    |                                        |  - /vast/data
                  |          establish peering             |
                  | -------------------------------------> |
                  | <------------------------------------- |
                  |                                        |
                  |  /vast/control: [UUID, expression]     |
                  | -------------------------------------> |
                  |                                        |  perform lookup
                  |                                        |  for 'expression'
                  |  /vast/data: [UUID, x] for x in xs     |  => 'xs' results
                  | <------------------------------------- |
                  |                                        |
                  |  /vast/data: [UUID, nil]               |  lookup complete
                  | <------------------------------------- |
```


### Example 1: Stub <-> Stub

This scenario mocks VAST and Bro with two Python scripts.

1. Launch the VAST stub: `./stub vast`
2. Launch the Bro stub: `./stub bro`

We also provide straight-line code of the [VAST stub](stub-vast) and [Bro
stub](stub-bro) for easier consumption.

### Example 2: Bro <-> Stub

This scenario mocks VAST with a Python script and makes Bro connect to it.

1.  Launch the VAST stub: `./stub vast`
2.  Launch Bro to connect to the stub: `bro vast.bro`

For (2) to work, you must have a `bro` binary from the current master branch in
your `PATH`.

### Example 3: Bro <-> VAST

This scenario uses the `bro-to-vast` bridge to connect to a running VAST
instance and then makes makes Bro connect to to the bridge.

First, we import some data into an actual VAST instance:

```shell
# Start VAST
vast start

# Download a PCAP trace.
wget http://downloads.digitalcorpora.org/corpora/scenarios/2009-m57-patents/net/net-2009-11-18-10:32.pcap.gz -o trace.pcap.gz

# Create Bro logs from the trace
mkdir bro
cd bro
zcat ../trace.pcap | bro -C -r -
cd ..

# Import the trace and Bro logs.
zcat trace.pcap | vast import pcap
cat bro/*.log | vast import bro

# Sanity check that we can perform queries:
vast export ascii 'id.orig_h == 192.168.1.1'
```

Second, we start the VAST bridge that connects to VAST:

```
bro-to-vast
```

Third, we launch Bro to connect to the VAST bridge:

```
bro vast.bro
```

There exists one caveat at the moment. All three tools rely on
[CAF][caf] for the underlying communication. When CAF applications connect with
each other, they perform a handshake that requires matching *application
identifiers*. Broker sets this identifier to `broker.v<N>` where `<N>` is the
Broker version. The `bro-to-vast` bridge uses this identifier by default, but
VAST needs to be told to use it as well. To this end, you can simply create a
file `vast.ini` in your working directory with the following contents:

```ini
[middleman]
app-identifier="broker.v1"
```

This repository comes with such a [vast.ini](vast.ini) already. This workaround
will be necessary [until CAF supports a list of additional application
identifiers](https://github.com/actor-framework/actor-framework/issues/756).

## Evaluation

Our data and tools for analyzing Broker performance are located in the
[evaluation](evaluation) directory. Our analysis is twofold: we look at latency
and throughput.

The following test systems were at our disposal:

- macOS Mojave 10.14, iMac 18,3, Intel i7-7700K (8) @ 4.20GHz, 16 GiB RAM
- linux nixos unstable (19.03-pre), Dell XPS13 9343,
  Intel i7-5500U (4) @ 2.40GHz, 8 GiB RAM
- FreeBSD 11.2 amd64, Intel Xeon E5-2640 (32) @ 2GHz, 128 GiB RAM

### Latency

We measure latency as round-trip time (RTT) in a ping-pong scenario between peered
endpoints in separate processes. One process publishes a *ping* message to which
another process responds with a *pong* message. One RTT is time is the
difference between the arrival time of the pong message and the sent time of
the ping message.

To better understand how latency changes with a large number of nodes, we also
add *relay* nodes in the middle. They only forward every message to the next
node. This diagram illustrates the setup:

```
      ping node  <-->  relay node  <-->  ...  <--> relay node  <-->  pong node
```

In our demo, we vary the number of relay nodes from 0 to 10. To generate test
data, invoke the latency script as follows:

```shell
cd evaluation/latency
./benchmark 0 10
```

The two arguments to `benchmark` determine the minimum and maximum number of
relays to include. The script generates a file `latency.csv`. The
[latency](evaluation/latency) directory includes runs from our test machines.

We also wrote an [R script](evaluation/visualize.R) that visualize the
generated data. It takes one or more latency files as input and generates PDFs
in the same directory of the latency file:

```shell
cd evaluation
./visualize.R latency/imac-18-3-mojave.csv
```

### Throughput

We measure throughput in a similar way as latency: instead of using ping and
pong nodes we use `broker-pipe`, a small message generator that acts as
publisher or subscriber:

```
     broker-pipe   -->  relay node   -->  ...   --> relay node   -->  broker-pipe
```

We build a pipeline between a `broker-pipe` publisher and subscriber, with a
varying number of relays in the middle. We patched `broker-pipe` to print rate
information instead payload data. You need this patch to run our benchmarks:

```shell
cd broker
git apply ../throughput.patch
cd build
ninja && ninja install
```

Thereafter, you can run the throughput measurements as follows:

```shell
cd evaluation/throughput
./benchmark 0 10
```

As for the latency evaluation, the two arguments to `benchmark` determine the
minimum and maximum number of relays to include. The script generates a file
`throughput.csv`. The [throughput](evaluation/throughput) directory includes runs
from our test machines.

Our [R script](evaluation/visualize.R) also understands the throughput CSV
files and generates plots accordingly:

```shell
cd evaluation
./visualize.R throughput/imac-18-3-mojave.csv
```

[caf]: https://github.com/actor-framework/actor-framework
[brocon18]: https://www.brocon2018.com
