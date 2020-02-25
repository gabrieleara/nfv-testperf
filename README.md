# NFV-TestPerf: A Performance Testing Framework for High-Performance Inter-Container Communications

NFV-TestPerf is a framework that can be used to evaluate the performance of various networking solutions for high-performance communications among Linux containers.

## Installation

To install this framework you have to clone this repository first.
Currently, the installation process of the framework requires a Debian-based Linux distribution (it uses `apt` manager to install many dependencies) and it is currently developed and tested on top of an Ubuntu distribution.

Before running the [install.bash](install.bash) script, make sure that the configuration file [lxcconfrc.bash](installdata/lxcconfrc.bash) is compatible with your system and provides everything you need.

Once you are done, you can install the whole framework by running
```sh
./install.bash
```

The following parameters are accepted, in no particular order:
 - `-i` : install command, default action;
 - `-d` : delete command, removes the installed components of the framework;
 - `-y` : accept all default parameters. The installation script will prompt a few question at the beginning, but they can be skipped with this option.

Of course, `-i` and `-d` options are in conflict with each other.

A few additional notes on the installation script:
 - It uses multiple times the `sudo` command, so make sure you have `sudo` privileges and insert your password when prompted; you can check the script source code if you are worried about trusting some random script with your password.
 - There is a point in which the installation script may fail without a decent reason. This is a well-known issue and it can be easily fixed by running a second time the installation script with the same parameters. The script will skip all the parts that are already installed and resume from the interruption point. If you want you can even run the installation script the first time using the following command, which will automatically run a second time if an error occurs:
 ```sh
    ./install.bash || ./install.bash
 ```

## Usage

For more details about how to use this framework refer to the [scripts](scripts) directory (which is NOT installed to a specific directory on the system, only framework dependencies and tools are installed).

## Extending and Contributing

If you are interested into extending this project, please refer to the [Extending](../wiki/Extending) wiki page. If you wish to contribute contact me on GitHub.
