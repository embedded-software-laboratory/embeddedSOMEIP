# embeddedSOME/IP

We present embeddedSOME/IP, an open-source implementation of SOME/IP, the primary communication protocol of AUTOSAR Adaptive.
embeddedSOME/IP implements the full standard, including service discovery and the transport protocol, and combines a hardware abstraction layer with static, pool-based memory management.
It is designed for resource-constrained targets such as STM32H7 microcontrollers as well as POSIX hosts, while preserving interoperability with vSomeIP, the reference implementation by COVESA.
Benchmarked against vSomeIP, embeddedSOME/IP achieves lower communication latency while matching its discovery performance.

## Folder structure

| Folder | Contents |
|---|---|
| `src/someIp` | the library |
| `examples` | host executables, benchmark clients and functional demos |
| `tests` | unit, integration, e2e and latency tests |
| `tracer` | nodes for the external soa_tracer harness |

Inside `src/someIp`:

| Folder | Contents |
|---|---|
| `communication` | transport abstraction, POSIX and lwIP backends |
| `service_discovery` | SOME/IP-SD, offers, finds, subscriptions, entries and options |
| `transport-protocol` | SOME/IP-TP, segmenter and reassembler |
| `structs` | wire types, header, package, service, method |
| `handler` | receive path, dispatcher and service handler |
| `config` | platform selection and the pool sizing knobs |
| `utils` | static containers, pool allocators, timers, serializers |
| `storages`, `os`, `net`, `enums`, `logging`, `pattern` | small support headers |

`ESomeIp.hpp` is the entry point.

## Platform

The platform is chosen by the `SOMEIP_PLATFORM_STM32` macro, switched in `src/someIp/config/config.hpp`. POSIX is the default and STM32 is opt-in. The macro is a compile-time define, not a CMake option. The STM32 build is controlled by the CubeIDE project.

## Test cases

The benchmark clients live in `examples`. Each runs on the host and measures against an STM32 server flashed with a matching `SOMEIP_TESTCASE`.

| Client | STM32 image | Measures |
|---|---|---|
| `T1_rr_latency` | `SOMEIP_TESTCASE=1` | request response RTT, 16 to 1024 B |
| `T2_sd_ttfm` | `SOMEIP_TESTCASE=2` | SD discovery plus first response, fresh client per sample |
| `T3_notify_rr` | `SOMEIP_TESTCASE=3` | field SET to notification RTT |
| `T4_sd_ttfn` | `SOMEIP_TESTCASE=3` | subscribe to first notification |
| `T5_tp_rtt` | `SOMEIP_TESTCASE=5` | SOME/IP-TP segmented RTT and goodput |
| `B1_udp_rr` | `UDP_BASELINE=1` | plain UDP RTT, the floor for T1 |
| `B3_udp_notify` | `UDP_BASELINE=1` | plain UDP RTT at field sizes, the floor for T3 |

Always pass `--client-ip=<host IP on the board's subnet>`. Without it the SD multicast join may bind the wrong interface and discovery fails.

## Demos

`examples` also holds `D#` client and server pairs that run against each other on loopback, one per feature. `D3_field` covers fields and event groups over SD and is the only pair that currently builds. `D1_rr`, `D2_sd_rr` and `D4_ff` still use the pre-rename API and are commented out in `examples/CMakeLists.txt` until they are ported.

## Build

Needs CMake 3.15 or newer, a C++17 compiler and pthreads.

```console
cmake -S . -B build
cmake --build build -j
```

The library is built as the static target `someIp` with `-fno-exceptions`.

| Option | Default | Effect |
|---|---|---|
| `SOMEIP_BUILD_APPS` | `ON` | build the examples |
| `SOMEIP_BUILD_TESTS` | `ON` | build the tests |
| `SOMEIP_ENABLE_TRACING` | `OFF` | build the tracer nodes, needs the soa_tracer parent |
| `SWEEP_TRANSPORT` | `UDP` | transport for T1 to T5, must match the board's `SOMEIP_TRANSPORT` |

Run a demo:

```console
./build/examples/D3_field_server &
./build/examples/D3_field_client
```

## Tests

```console
cmake --build build --target embeddedSomeIP_test -j
ctest --test-dir build --output-on-failure
```
