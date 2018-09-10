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

### Example 1: Stub <-> Stub

In this scenario, we mock Bro and VAST with two Python scripts.

1.  Launch the VAST stub: `./stub.py vast`
2.  Launch the Bro stub: `./stub.py bro`

You'll see data flowing between the two processes.

```
        Bro                                      VAST
         |          establish peering             |
         |<-------------------------------------->|
         |                                        |
         |      subscribe to /vast/quries         |
         |<-------------------------------------- |
         |                                        |
         |      subscribe to /vast/quries         |
         |--------------------------------------- |
```



### Example 2: Bro <-> Stub

### Example 3: Bro <-> VAST


[brocon18]: https://www.brocon2018.com
[slides.pdf]: TODO
