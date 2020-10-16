# Testing Application (MAINTAINED VERSION)

This directory contains the source code for the testing application, used to generate/consume synthetic network traffic.

## Dependencies

To build this application, DPDK must be installed in the system and the `RTE_SDK` exported environment variable must be set.

In addition, to install this application in the desired `rootfs` the `NFV_PATH` exported environment variable must be set too.

## Application structure

This application is actually a sort of a [BusyBox](https://en.wikipedia.org/wiki/BusyBox)-like application, in the sense that it is composed by a set of multiple applications that are all linked together in a single executable file.

Each application represents one of the testing applications that can be used to generate/consume synthetic network traffic:
 - `send`: Sender application
 - `recv`: Receiver application
 - `server`: Server application
 - `client`: Multi-threaded client application
 - `clientst`: Single-threaded client application

To be more precise, DPDK-based applications and POSIX-based applications are actually separate applications, thus the following applications are also available (they are virtually equivalent to their POSIX counterparts):
 - `dpdk-send`: Sender application
 - `dpdk-recv`: Receiver application
 - `dpdk-server`: Server application
 - `dpdk-client`: Multi-threaded client application
 - `dpdk-clientst`: Single-threaded client application
