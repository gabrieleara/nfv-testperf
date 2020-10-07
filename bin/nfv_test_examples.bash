#!/bin/bash

# Following are some examples of test runs that can be copied and pasted to do some performance evaluations

############### TESTS THAT USE ONLY ONE HOST ###############

#./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 256 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp basicfwd snabb" -c -f configurations/confrc-throughput-local-host1-1vs1.bash -o out-local-1v1-th
#./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 256 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp basicfwd snabb" -c -f configurations/confrc-throughput-local-host1-2vs2.bash -o out-local-2v2-th
#./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 256 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp basicfwd snabb" -c -f configurations/confrc-throughput-local-host1-4vs4.bash -o out-local-4v4-th
#./run_testset.bash -r "20000000" -b 32 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp basicfwd snabb" -c -f configurations/confrc-latency-local-host1-1vs1.bash -o out-local-1v1-lat
# ./run_testset.bash -r "1000" -b "4 8 16 32 64 128 256 512" -p "64 128 256 512 1024 1500" -v "ovs sriov vpp basicfwd snabb" -c -f configurations/confrc-latency-local-host1-1vs1-threeth.bash -o out-local-1v1-lat
# ./run_testset.bash -r "1000" -b "4 8 16 32 64 128 256 512" -p "64 128 256 512 1024 1500" -v "ovs sriov vpp basicfwd snabb" -c -f configurations/confrc-latency-remote-host1-1vs1-threeth.bash -H host2 -F configurations/confrc-latency-remote-host2-1vs1-threeth.bash -o out-remote-1v1-lat

############## TESTS THAT USE MULTIPLE HOSTS ###############

# ./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 32 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp" -c -f configurations/confrc-throughput-remote-host1-1vs1.bash -o out-remote-1v1-th -H host2 -F configurations/confrc-throughput-remote-host2-1vs1.bash
# ./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 32 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp" -c -f configurations/confrc-throughput-remote-host1-2vs2.bash -o out-remote-2v2-th -H host2 -F configurations/confrc-throughput-remote-host2-2vs2.bash
# ./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 32 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp" -c -f configurations/confrc-throughput-remote-host1-4vs4.bash -o out-remote-4v4-th -H host2 -F configurations/confrc-throughput-remote-host2-4vs4.bash
#./run_testset.bash -r "1000000 2500000 5000000 7500000 10000000 12500000 15000000 17500000 20000000" -b 32 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp" -c -f configurations/confrc-throughput-remote-host1-8vs8.bash -o out-remote-8vs8-th -H host2 -F configurations/confrc-throughput-remote-host2-8vs8.bash
#./run_testset.bash -r "20000000" -b 32 -p "64 128 256 512 1024 1500" -v "ovs sriov vpp" -c -f configurations/confrc-latency-remote-host1-1vs1.bash -o out-remote-1v1-lat -H host2 -F configurations/confrc-latency-remote-host2-1vs1.bash
