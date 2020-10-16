# Testing Scripts

This directory contain a set of `bash` scripts that are used to configure, execute, and collect test results.

Following is the description of the content of this folder. The only file that should NOT be executed on the host is the [nfv_run_in_container](nfv_run_in_container) file, which is copied by the installation script to the `rootfs` shared by all containers during the installation.

Each script should provide a `--help` command-line option to illustrate its usage. Examples of usage of the [nfv_run_multiple](nfv_run_multiple) script can be found in [nfv_test_examples.bash](nfv_test_examples.bash).

## [nfv_sriov_tools](nfv_sriov_tools)

This is an utility script that is used by the [nfv_run_local](nfv_run_local) file to configure SR-IOV devices.

## [nfv_run_local](nfv_run_local)

This script is used to run tests on the local machine.

## [nfv_run_remote](nfv_run_remote)

This script is used to start tests on a remote installation of the framework on another host.

## [nfv_run_multiple](nfv_run_multiple)

This script is used to start multiple tests, one after the other, on a single or multiple hosts, by repeatedly running the [nfv_run_local](nfv_run_local) and [nfv_run_remote](nfv_run_remote) scripts.
