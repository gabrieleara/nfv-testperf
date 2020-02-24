# Configurations folder

This directory contains a set of example configuration files that can be used to start multiple tests.

These configuration files are basically scripts (or run-commands configurations, if you prefer), which fill certain bash variables with the desired values. These variables are then used by other scripts to start each test according to the supplied configuration.

These variables are all list of values, which means that values at index i in each list represent the attributes of the i-th application to be deployed by the framework.

Following is the list of all the variables and their meaning:
 - `LXC_CONT_NAMES`: The names of each container to be started for each testing application.
 - `LXC_CONT_VFS`: The SR-IOV virtual functions to assign to each container.
 - `LXC_CONT_MACS`: The MAC address to be used/assigned to each application running inside a container.
 - `LXC_CONT_IPS`: The IP address to be used/assigned to each application running inside a container.
 - `LXC_CONT_NMASKS`: The networking mask to be used/assigned to each application running inside a container.
 - `LXC_CONT_VFDEVS`: The device names for the UIO driver that will be used for each SR-IOV virtual function.
 - `LXC_CONT_VFPCIS`: The PCI address of each SR-IOV virtual function.
 - `LXC_CONT_SOCKS`: The socket name for the `virtio` ports assigned to each application running inside a container.
 - `LXC_CONT_OTHER_IPS`: The IP address of the other application. This list is accessed with the same indexes as `LXC_CONT_IPS` to get matches between sender/receiver or client/server application IPs.
 - `LXC_CONT_OTHER_MACS`: The MAC address of the other application. This list is accessed with the same indexes as `LXC_CONT_MACS` to get matches between sender/receiver or client/server application MACs.
 - `LXC_CONT_CMDNAMES`: Name of the testing application to be executed within the given container. Can be either:
    - `send`
    - `recv`
    - `client`
    - `clientst`
    - `server`

## Small note on multiple hosts scenarios:

When applications should be deployed on multiple hosts, a pair of configuration files should be provided. You can find some examples that are marked `host1` and `host2`. Usually, the first is the configuration of the machine that is in charge of performing the tests, while the second corresponds to the other machine.

During test configuration, the `host2` file (or equivalent, one supplied through command-line arguments) will be copied via SSH to the other host and used by local scripts to configure the system for the test.
