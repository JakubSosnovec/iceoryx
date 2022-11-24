# icehello with minimal memory usage

## Introduction

In some cases, it is needed to 

## How to build

Build iceoryx RouDi process and the hello world example with the given CMake
options configuration for minimal memory usage. For example, as follows:

```
cmake -Bbuild -Hiceoryx_meta -Ciceoryx_examples/small_memory/options.cmake -DEXAMPLES=ON
cmake --build build --target iox-roudi iox-cpp-publisher-helloworld iox-cpp-subscriber-helloworld
```

## How to run

Launch the RouDi process with the given TOML config file:

```
build/iox-roudi -c iceoryx_examples/small_memory/roudi_config.toml &
build/iceoryx_examples/icehello/iox-cpp-subscriber-helloworld &
build/iceoryx_examples/icehello/iox-cpp-publisher-helloworld &
```

To terminate the demo, just kill all the processes:

```
killall -r iox
```
