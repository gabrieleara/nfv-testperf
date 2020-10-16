# Installation Data Directory

This folder contains some data that is used by the [installation script](../nfv_install.bash) to generate additional content according to certain configurations or to install some other custom applications.

Following is the description of each file contained in this folder.

## [custom_switch.lua](custom_switch.lua)

A custom implementation of a simple learning bridge is set in the middle among all the ports.
This is actually the source code of the switch is the one that is used with Snabb framework to actually implement the forwarding of packets to their destination.

## [lxcbase.config](lxcbase.config)

This is the base file that is used to generate actual LXC configuration files, starting from the content of the [lxcconfrc.bash](lxcconfrc.bash) file.

## [lxcconfrc.bash](lxcconfrc.bash) and [lxcconfrc-remote.bash](lxcconfrc-remote.bash)

These two files contain the configuration for two different hosts in which the framework should be installed. Feel free to change these configurations to match your system before installing.

The [lxcconfrc-remote.bash](lxcconfrc-remote.bash) file is not used by the installation script at installation time, but it can be useful to maintain both configurations inside a single folder. When installing the system on the second host, remember to swap the configuration files, something similar to:
```sh
mv lxcconfrc.bash lxcconfrc-tmp.bash
mv lxcconfrc-remote.bash lxcconfrc.bash
mv lxcconfrc-tmp.bash lxcconfrc-remote.bash
```

## [vppconfig.conf](vppconfig.conf)

This is the configuration file used to run VPP on the current system. Feel free to adapt it to your configuration before installation.
