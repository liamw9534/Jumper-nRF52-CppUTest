# Jumper-nRF52-CppUTest

## Introduction

Getting good test coverage on code for embedded systems can be challenging.  And just because the code
runs on inaccessible platforms with limited resources does not mean we can't follow TDD methods.

To assist in this goal, one can find many excellent tools and resources out there that make this job much
easier.  However, most are focused primarily on those writing embedded software using only C code.

Whilst C is still the most popular language for developing embedded systems, and with good reason, there
are certain features of the C++ language which many embedded projects could benefit from
(e.g., namespaces, encapsulation, abstraction, inheritance, stronger type checking).

This project demonstrates how to develop unit tests using CppUTest and run them on a Jumper virtual machine
for the nRF52.  This allows projects developed in pure C++ or a mixture of C and C++ to be developed and
tested with a single methodology.

## Background technology

The nRF52 is a Nordic Semiconductors processor with embedded ARM Cortex M4F core that integrates
a BLE 5 PHY and stack.  See https://www.nordicsemi.com/Products/nRF52-Series-SoC.

Jumper provides a plug-and-play virtual lab that brings the benefits of virtual machines to IoT
devices.  See https://jumper.io/about.

CppUTest is a lightweight C++ unit testing and mocking framework.  See https://cpputest.github.io.


## Getting Started

These instructions assume you are using some flavour of Linux e.g., Ubuntu.

### Prerequisites

You'll need:

1. An existing `jumper.io` account and your access token.
2. The latest `Jumper` tools installed on your local machine.  See https://docs.jumper.io/docs/install.html.
3. nRF52 SDK 15.0.0 installed on your local machine.  See http://infocenter.nordicsemi.com/topic/com.nordic.infocenter.nrf52/dita/nrf52/development/nrf52_dev_kit.html.
4. The latest `CppUTest` source bundle installed on your local machine.  See See https://cpputest.github.io.
5. `arm-none-eabi` v5.4 (or later) installed.  Make sure they are on your PATH.
6. A local copy of this repository.


### Installing and building

1. Setup some environment variables so you can build outside the various source trees.

```
export SDK_ROOT=<path to your nRF52 SDK>
export CPPUTEST_HOME=<path to your CppUTest>
export TOKEN=<your Jumper.io token>
```

2. Build the nRF52 unit test application image

```
cd <this repository>/examples/cpputest_basic/pca10040/blank/armgcc
make
```

Your firmware image file should be at `_build/nrf52832_xxaa.bin`.

## Running the test

Execute the following command to launch the virtual machine:

```
make start_emulator
```

### Expected output

You should see the following output:

```
Loading virtual device

Virtual device is running.


../../../test/test.cpp:38: error: Failure in TEST(ProductionMock, MockTest)
    Mock Failure: Expected call WAS NOT fulfilled.
    EXPECTED calls that WERE NOT fulfilled:
        productionFunction -> no parameters (expected 2 calls, called 1 time)
    EXPECTED calls that WERE fulfilled:
        <none>

.
../../../test/test.cpp:27: error: Failure in TEST(FirstTestGroup, SecondTest)
    expected <hello>
    but was  <world>
    difference starts at position 0 at: <          world     >
                                                   ^

.
../../../test/test.cpp:22: error: Failure in TEST(FirstTestGroup, FirstTest)
    Fail me!

.
Errors (3 failures, 3 tests, 3 ran, 3 checks, 0 ignored, 0 filtered out, 0 ms)
```

### How it works

The Jumper virtual machine allows native arm code to be executed on its
ARM Cortex M4F core.  All standard output from the CppUTest framework is
redirected to a UART peripheral using the nRF52 SDK redirect library.  The UART
output is visible as a trace output in the console window where the jumper
session was launched.


## A slightly more sophisticated example

Ok, so the `cpputest_basic` application is very basic and does not really show any
real C++ testing in an embedded system.  A more slightly more sophisticated example is built-up in
`spi_flash_filesystem`.  This example includes:

- a C++ wrapper for a generic SPI flash device that uses the nRF52 SDK SPI driver
- a specific instantiation of the S25FL128 SPI memory chip using C++ inheritence
- a simple file system implementation in C++ that uses the S25FL128
- unit testing of both S25FL128 and file system C++ classes using CppUTest
- a behavioural model for the S25FL128 flash device that runs with the virtual machine

DISCLAIMER: Please note that the file system implementation is not intended to be used
in commercial products "as is".  It is here purely for educational purposes.

To run this example, enter the following commands:

```
cd <this repository>/examples/spi_flash_filesystem/pca10040/blank/armgcc
make start_emulator
```

You should see the following output:

```
Loading virtual device

Virtual device is running.

..........
OK (10 tests, 10 ran, 29 checks, 0 ignored, 0 filtered out, 0 ms)
```

### How it works

The nRF52 SPI peripheral is part of the Jumper hardware virtualization and "connects"
to a peripheral model for the S25FL128 flash device which has been developed in C++
using the Jumper SDK.  The source code for the peripheral model can be
found at https://github.com/liamw9534/modeling-framework.

With a virtual flash device in place, it is then possible to test all possible file
operations on the target hardware and their interactions with the SPI flash device.

## Authors

* **Liam Wickins** [liamw9543](https://github.com/liamw9534)

## License

This project is licensed under the Apache 2.0 License - see the [LICENSE.md](LICENSE.md) file
for details.

## Acknowledgments

* This project has adopted a similar approach as outlined by Jonathan Seroussi in his article
https://medium.com/jumperiot/unity-testing-jumper-embedded-software-unit-testing-the-easy-way-8e7d77175954.
