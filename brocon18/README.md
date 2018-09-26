# BroCon 2018

This repository contains code and slides from our presentation at [BroCon
2018][brocon18].


## Slides

Our slides are available for [download as PDF][slides.pdf].

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
git clone git@github.com:bro/broker.git
cd broker
git submodule update --recursive --init
./configure \
  --generator=Ninja \
  --prefix=$PREFIX \
  --with-python=$PREFIX/bin/python
cd build
ninja
ninja install
cd ..
```

Finally, we make sure that we find the Broker Python modules without setting
`PYTHONPATH` to `$PREFIX/lib/python`:

```shell
site_packages=$(python -c "import site; print(site.getsitepackages()[0])")
cp sitecustomize.py $site_packages
```

## Scenario

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

1. Launch the VAST stub: `./stub.py vast`
2. Launch the Bro stub: `./stub.py bro`


### Example 2: Bro <-> Stub

This scenario mocks VAST with a Python script and makes Bro connect to it.

1.  Launch the VAST stub: `./stub.py vast`
2.  Launch Bro to connect to the stub: `bro vast.bro`

For (2) to work, you must have a `bro` binary from the current master branch in
your `PATH`.

### Example 3: Bro <-> VAST

This scenario uses the `bro-to-vast` bridge to connect to a running VAST
instance and then makes makes Bro connect to to the bridge.

1.  Launch VAST: `vast start`
2.  Launch the Bro-to-VAST bridge: `bro-to-vast`
3.  Launch Bro to connect to VAST via the bridge: `bro vast.bro`

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

This workaround will be necessary [until CAF supports a list of additional
application identifiers](https://github.com/actor-framework/actor-framework/issues/756).

[caf]: https://github.com/actor-framework/actor-framework
[brocon18]: https://www.brocon2018.com
[slides.pdf]: TODO