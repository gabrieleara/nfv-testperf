# NFV-TestPerf: A Performance Testing Framework for High-Performance Inter-Container Communications

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

NFV-TestPerf is a framework that can be used to evaluate the performance of various networking solutions for high-performance communications among Linux containers.
 
## Description

Detailed information about this framework, the motivations behind its development, and sample output from its application can be found in the published research paper:

> G. Ara, T. Cucinotta, L. Abeni, C. Vitucci. "**Comparative Evaluation of Kernel Bypass Mechanisms for High-Performance Inter-Container Communications**," (to appear) in Proceedings of the 10th International Conference on Cloud Computing and Services Science (CLOSER 2020), May 7-9, 2020, Prague, Czech Republic.

## Installation

To install this framework you have to clone this repository first.
Currently, the installation process of the framework requires a Debian-based Linux distribution (it uses `apt` manager to install many dependencies) and it is currently developed and tested on top of an Ubuntu distribution.

Before running the [install.bash](install.bash) script, make sure that the configuration file [lxcconfrc.bash](installdata/lxcconfrc.bash) is compatible with your system and provides everything you need.

Once you are done, you can install the whole framework by running
```sh
./nfv_install.bash
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
    ./nfv_install.bash || ./nfv_install.bash
 ```

## Usage

For more details about how to use this framework refer to the [scripts](scripts) directory (which is NOT installed to a specific directory on the system, only framework dependencies and tools are installed).

## Extending and Contributing

If you are interested into extending this project, please refer to the [Extending](../../wiki/Extending) wiki page. If you wish to contribute contact me on GitHub.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [LICENSE.txt](LICENSE.txt) for more info.
