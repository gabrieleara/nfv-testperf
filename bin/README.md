# Testing Scripts

This directory contain a set of `bash` scripts that are used to configure, execute, and collect test results.

Following is the description of the content of this folder. The only file that should NOT be executed on the host is the [test.sh](test.sh) file, which is copied by the installation script to the rootfs shared by all containers during the installation.

Each script should provide a `--help` command-line option to illustrate its usage. Examples of usage of the [run_testset.bash](run_testset.bash) script can be found in [test_examples.bash](test_examples.bash).

## [sriov_tools.bash](sriov_tools.bash)

This is an utility script that is used by the [run_local_test.bash](run_local_test.bash) file to configure SR-IOV devices.

## [run_local_test.bash](run_local_test.bash)

This script is used to run tests on the local machine.

## [run_remote_test.bash](run_remote_test.bash)

This script is used to start tests on a remote installation of the framework on another host.

## [run_testset.bash](run_testset.bash)

This script is used to start multiple tests, one after the other, on a single or multiple hosts, by repeatedly running the [run_local_test.bash](run_local_test.bash) and [run_remote_test.bash](run_remote_test.bash) scripts.
