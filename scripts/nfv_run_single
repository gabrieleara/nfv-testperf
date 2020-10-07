#!/bin/bash

set -e

################################################################################
#                                  UTILITIES                                   #
################################################################################

# Wait for the screen of name $1 to finish
# NOTICE: matches also on substrings
# NOTICE: it will wait at most for 15 seconds (or the second argument if given),
#         this is actually necessary only for snabb switch because it doesn't
#         close correctly vhost instances
function wait_screen() {

    if [ $# -ge 2 ] ; then
        max_time=$2
    else
        max_time=15
    fi

    start_time=$(date -u +%s)

    end_time=$(date -u +%s)
    elapsed=$(($end_time-$start_time))

    while (screen -list | grep -q $1) && test $elapsed -le $max_time ; do
        sleep 1
        end_time=$(date -u +%s)
        elapsed=$(($end_time-$start_time))
    done
}

# Returns 0 on success, 1 on failure
function vpp_create_vhost() {
    sleep 1

    for i in "${!LXC_CONT_NAMES[@]}" ; do
        sudo vppctl create vhost-user socket /tmp/VIO/${LXC_CONT_SOCKS[i]} server   &>/dev/null && \
        sudo vppctl set interface state VirtualEthernet0/0/$i up                    &>/dev/null && \
        sudo vppctl set interface l2 bridge VirtualEthernet0/0/$i 1                 &>/dev/null

        if [ $? -ne 0 ] ; then
            echo 1
            return
        fi
    done

    echo 0

    # hideout=$(sudo vppctl create vhost-user socket /tmp/VIO/sock0 server 2>/dev/null && \
    #     sudo vppctl create vhost-user socket /tmp/VIO/sock1 server 2>/dev/null && \
    #     sudo vppctl set interface state VirtualEthernet0/0/0 up    2>/dev/null && \
    #     sudo vppctl set interface state VirtualEthernet0/0/1 up    2>/dev/null && \
    #     sudo vppctl set interface l2 bridge VirtualEthernet0/0/0 1 2>/dev/null && \
    #     sudo vppctl set interface l2 bridge VirtualEthernet0/0/1 1 2>/dev/null )

    # if [ $? -ne 0 ] ; then
    #     echo 1
    # else
    #     echo 0
    # fi
}


function linesep() {
    printf '=%.0s' {1..80}
    printf '\n'
}

function isnumber() {
    if [[ "$1" =~ ^[0-9]+$ ]] ; then
        echo 1
    else
        echo 0
    fi
}

function contains() {
    if  [[ $1 =~ (^|[[:space:]])$2($|[[:space:]]) ]] ; then
        echo 1
    else
        echo 0
    fi
}

# function get_command_name_cont_a() {
#     case $dimension in
#     throughput )
#         echo send
#         ;;
#     latency )
#         echo client
#         ;;
#     latencyst )
#         echo clientst
#         ;;
#     esac
# }

# function get_command_name_cont_b() {
#     case $dimension in
#     throughput )
#         echo recv
#         ;;
#     latency )
#         echo server
#         ;;
#     latencyst )
#         echo server
#         ;;
#     esac
# }

pnoargformat="%-19s %s\n\n"
pargformat="%-19s %s\n                    %s\n\n"

function print_help() {
    cmd=$0
    if [ $cmd = "bash" ] ; then
        cmd="$0 $BASH_SOURCE"
    fi

    printf "%s\n" \
        "Performs tests on virtual switch technologies to exchange data between co-located containers." \
        "Usage: $cmd <args>" \
        "" \
        "All arguments are optional, default values will be used if not provided." \
        "Order does not matter." \
        "" \
        "Arguments [*=default]:" \
        ""

    printf "$pnoargformat" \
        "-h, --help" "Print this message" \

    printf "$pargformat" \
        "-d, --dimension" "[throughput*|latency|latencyst]" \
        "The dimension that shall be tested for"

    printf "$pargformat" \
        "-t, --timeout" "<number>[=$default_timeout]" \
        "The number of seconds the test should last for"

    printf "$pargformat" \
        "-f, --conf-filename" "<path>[=$default_filename]" \
        "The file in which configuration for local containers and applications to be started are kept"

    printf "$pargformat" \
        "-o, --out-dir" "<path>[=out]" \
        "The directory in which results should be written"

    printf "$pargformat" \
        "-v, --vswitch" "[pmd*|vpp|ovs|basicfwd|snabb|sriov|linux-bridge]" \
        "The virtual switch used to exchange packets"

    printf "$pargformat" \
        "-r, --rate" "<number>[=$default_rate]" \
        "The number of packets per seconds that should be sent"

    printf "$pargformat" \
        "-b, --burst-size" "<number>[=$default_bst_size]" \
        "The number of packets that should be grouped in a single burst"

    printf "$pargformat" \
        "-p, --pkt-size" "<number>[=$default_pkt_size]" \
        "The size of each packet in bytes"

    printf "$pnoargformat" \
        "-c, --consume-data" "Use this to touch all bytes sent in a packet each time is produced/received"

    printf "$pnoargformat" \
        "-T, --test-param" "Use this to test only if parameters provided are correct"

    printf "$pnoargformat" \
        "-D, --debug" "Print more info/more frequently for debug purposes"

    printf "%s\n" \
        "NOTICE: DO NOT SOURCE THIS FILE, IT USES exit COMMAND TO RETURN A VALUE TO COMMAND LINE" \
        ""
}

function toshortopt() {
    case $1 in
    --help)             echo "-h"   ;;
    --dimension)        echo "-d"   ;;
    --timeout)          echo "-t"   ;;
    --conf-filename)    echo "-f"   ;;
    --out-dir)          echo "-o"   ;;
    --vswitch)          echo "-v"   ;;
    --rate)             echo "-r"   ;;
    --burst-size)       echo "-b"   ;;
    --pkt-size)         echo "-p"   ;;
    --consume-data)     echo "-c"   ;;
    --test-param)       echo "-T"   ;;
    --debug)            echo "-D"   ;;
    --use-raw-sock)     echo "-R"   ;;
    --use-block-sock)   echo "-B"   ;;
    --remote-hostname)  echo "-H"   ;;
    --remote-cmddir)    echo "-C"   ;;
    --schedule)         echo "-S"   ;;
    --use-multi)        echo "-m"   ;;
    *)                  echo "$1"   ;;
    esac
}

function tolongopt() {
    case $1 in
    -h) echo "--help"           ;;
    -d) echo "--dimension"      ;;
    -t) echo "--timeout"        ;;
    -f) echo "--conf-filename"  ;;
    -o) echo "--out-dir"        ;;
    -v) echo "--vswitch"        ;;
    -r) echo "--rate"           ;;
    -b) echo "--burst-size"     ;;
    -p) echo "--pkt-size"       ;;
    -c) echo "--consume-data"   ;;
    -T) echo "--test-param"     ;;
    -D) echo "--debug"          ;;
    -R) echo "--use-raw-sock"   ;;
    -B) echo "--use-block-sock" ;;
    -H) echo "--remote-hostname";;
    -C) echo "--remote-cmddir"  ;;
    -S) echo "--schedule"       ;;
    -m) echo "--use-multi"      ;;
    *)  echo "$1"               ;;
    esac
}

function print_configuration() {
    confformat="%29s    %-10s %s\n"

    if [ "$consume_data" = "1" ] ; then
        will_consume=true
    else
        will_consume=false
    fi

    if [ "$debug" = "1" ] ; then
        will_debug=true
    else
        will_debug=false
    fi

    if [ "$raw_sock" = "1" ] ; then
        will_raw=true
    else
        will_raw=false
    fi

    if [ "$multi_api" = "1" ] ; then
        will_mmsg=true
    else
        will_mmsg=false
    fi

    linesep
    printf "SINGLE TEST CONFIGURATION:\n"
    printf "$confformat" "Test dimension"   "$dimension"
    printf "$confformat" "Output directory" "$outdir"
    printf "$confformat" "vSwitch used"     "$vswitch"
    printf "$confformat" "Timeout"          "$timeout"  "s"
    printf "$confformat" "Rate"             "$rate"     "pps"
    printf "$confformat" "Burst size"       "$bst_size" "packets"
    printf "$confformat" "Packet size"      "$pkt_size" "bytes"
    printf "$confformat" "Debug info will be printed" "$will_debug"
    printf "$confformat" "Each byte will be touched" "$will_consume"
    printf "$confformat" "Using raw sockets" "$will_raw"
    printf "$confformat" "Using mmsg API"   "$will_mmsg"
    linesep
}

################################################################################
#                                  FUNCTIONS                                   #
################################################################################

function parse_parameters() {
    # First convert them to short parameters,
    # so we can use getopts instead of getopt
    for arg in "$@" ; do
        shift
        set -- "$@" "`toshortopt "$arg"`"
    done

    # Then use getopts in a loop with short options only
    OPTIND=1
    while getopts ":d:t:f:o:v:r:b:p:H:C:chTDBRmS:" opt ;
    do
        case ${opt} in
        d)  dimension=$OPTARG           ;;
        o)  outdir=$OPTARG              ;;
        r)  rate=$OPTARG                ;;
        b)  bst_size=$OPTARG            ;;
        p)  pkt_size=$OPTARG            ;;
        v)  vswitch=$OPTARG             ;;
        t)  timeout=$OPTARG             ;;
        f)  filename=$OPTARG            ;;
        c)  consume_data=1              ;;
        T)  test_params=1               ;;
        D)  debug=1                     ;;
        R)  raw_sock=1                  ;;
        B)  block_sock=1                ;;
        H)  ;; # ignore, option for run_remote_test script
        C)  ;; # ignore, option for run_remote_test script
        S)  schedule=$OPTARG            ;;
        m)  multi_api=1                 ;;
        help|h )
            print_help
            exit 0
            ;;
        \? )
            echo "Invalid option: -$OPTARG"
            exit 1
            ;;
        : )
            echo "Invalid option: -$OPTARG (`tolongopt -$OPTARG`) requires an argument"
            exit 1
            ;;
        esac
    done
    shift $((OPTIND -1))
    OPTIND=1
}

function check_parameters() {
    if [ "`contains \"$dimension_list\" \"$dimension\"`" = "0" ] ; then
        echo "Invalid test dimension selection \"$dimension\", select one from the following list:"
        echo -e "\t$dimension_list"
        exit 1
    fi

    if [ "`contains \"$vswitch_list\" \"$vswitch\"`" = "0" ] ; then
        echo "Invalid virtual switch selection \"$vswitch\", select one from the following list:"
        echo -e "\t$vswitch_list"
        exit 1
    fi

    if [ "`isnumber $rate`" = "0" ] ; then
        echo "Invalid parameter rate \"$rate\", must be an integer number."
        exit 1
    fi

    if [ "`isnumber $pkt_size`" = "0" ] ; then
        echo "Invalid parameter packet size \"$pkt_size\", must be an integer number."
        exit 1
    fi

    if [ "`isnumber $bst_size`" = "0" ] ; then
        echo "Invalid parameter burst size \"$bst_size\", must be an integer number."
        exit 1
    fi

    # TODO: test timeout
}

function silent_mode_opt {
    if [ ! $debug = 1 ] ; then
        echo "-s"
    fi
}

function log_vswitch_option {
    if [ $debug = 1 ] ; then
        echo "-L" "-Logfile" "$outdir/vswitch.log"
    fi
}

# TODO: make generic when moving to multi-computer commands
# TODO: add virtual switch log in debug
function start_vswitch() {
    echo "Attempting to start the vswitch..."
    case $vswitch in
    pmd)
        pmd_cmd="sudo $PMD -l $SWITCH_CPUS -n 2 --socket-mem 1024,0 --file-prefix=host"
        for i in "${!LXC_CONT_NAMES[@]}"; do
            pmd_cmd="$pmd_cmd --vdev eth_vhost$i,iface=/tmp/VIO/${LXC_CONT_SOCKS[i]}"
        done
        pmd_cmd="$pmd_cmd --no-pci -- --auto-start"

        screen -d -S screen_vswitch `log_vswitch_option` -m $pmd_cmd

        # screen -d -S screen_vswitch `log_vswitch_option` -m \
        #     sudo $PMD -l $SWITCH_CPUS -n 2 --socket-mem 1024,1024 \
        #     --vdev 'eth_vhost0,iface=/tmp/VIO/sock0' \
        #     --vdev 'eth_vhost1,iface=/tmp/VIO/sock1' \
        #     --file-prefix=host --no-pci -- --auto-start
        ;;
    basicfwd)
        pmd_cmd="sudo $BASICFWD -l $SWITCH_CPUS -n 2 --socket-mem 1024,0 --file-prefix=host"
        for i in "${!LXC_CONT_NAMES[@]}"; do
            pmd_cmd="$pmd_cmd --vdev eth_vhost$i,iface=/tmp/VIO/${LXC_CONT_SOCKS[i]}"
        done
        pmd_cmd="$pmd_cmd --no-pci -- --auto-start"

        screen -d -S screen_vswitch `log_vswitch_option` -m $pmd_cmd

        # screen -d -S screen_vswitch `log_vswitch_option` -m \
        # sudo $BASICFWD -l $SWITCH_CPUS -n 2 --socket-mem 1024,1024 \
        #     --vdev 'eth_vhost0,iface=/tmp/VIO/sock0' \
        #     --vdev 'eth_vhost1,iface=/tmp/VIO/sock1' \
        #     --file-prefix=host --no-pci
        ;;
    vpp)
        # Bind SR-IOV port to right driver
        $SCRIPT_DIR/sriov_tools.bash $filename -b igb_uio

        # This command should be executed from fdio directory
        #pushd $FDIO_DIR > /dev/null
        screen -d -S screen_vswitch `log_vswitch_option` -m \
            sudo vpp -c ~/testbed/conf/vpp.conf
        #popd > /dev/null

        # TODO: CHANGE
        #sudo vpp -c ~/testbed/conf/vpp.conf

        # Configure virtual ports
        # P=$FDIO_DIR/build-root/install-vpp-native/vpp/bin/

        # Creating vhost ports required the switch to be fully loaded,
        # otherwise it will fail immediately, thus we wait in a loop
        # until it is ready (assuming configuration called
        # mystartup.conf exists

        res=$(vpp_create_vhost)

        while [ $res -ne 0 ] ; do
            res=$(vpp_create_vhost)
        done

        sudo vppctl set interface state TenGigabitEthernet4/0/0 up && \
        sudo vppctl set interface l2 bridge TenGigabitEthernet4/0/0 1

        # TODO: add the external port

        ;;
    ovs)
        # OVS does not print anything on command line, so we do not need a
        # separate terminal to run it
        sudo mount --bind /usr/local/var/run/openvswitch /tmp/VIO
        sudo $OVS_DIR/ovs-ctl start

        # Open vSwitch DPDK options [DPDK can run only on core 0 and 2]
        #FIXME: memory requirements
        sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-init=true
        sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-lcore-mask=0x5
        sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-socket-mem="1024,0" # FIXME: CORRECT AMOUNT OF MEMORY!

        # Can execute only on cores 0 and 2
        sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:pmd-cpu-mask=0x5

        # Drop configuration for any existing OVS bridge
        for bridge in `sudo ovs-vsctl list-br`; do
            sudo ovs-vsctl del-br $bridge
        done

        # Now let's create a new bridge
        sudo ovs-vsctl add-br dpdk-ovs-br0 \
            -- set Bridge dpdk-ovs-br0 datapath_type=netdev \
            -- br-set-external-id dpdk-ovs-br0 bridge-id dpdk-ovs-br0 \
            -- set bridge dpdk-ovs-br0 fail-mode=standalone

        # Delete all present flows for the given bridge
        sudo ovs-ofctl del-flows dpdk-ovs-br0

        # Bind SR-IOV port to right driver
        $SCRIPT_DIR/sriov_tools.bash $filename -b igb_uio

        # Add the sriov port to OVS
        sudo ovs-vsctl add-port dpdk-ovs-br0 sriov_dev \
            -- set Interface sriov_dev type=dpdk options:dpdk-devargs=0000:04:00.0

        # Add one dpdkvhostuser port per local VM
        for i in "${!LXC_CONT_NAMES[@]}" ; do
            sudo ovs-vsctl add-port dpdk-ovs-br0 ${LXC_CONT_SOCKS[i]} \
                -- set Interface ${LXC_CONT_SOCKS[i]} type=dpdkvhostuser ofport_request=$((i+1))
        done

        # TODO: add the external port

        # NOTICE: UNCOMMENT THESE LINES TO ENABLE STATICALLY CONFIGURED PAIRS BASED ON PORT ORDERING (LIKE PMD)
        # for i in "${!LXC_CONT_NAMES[@]}" ; do
        #     # Port 1 (index 0) is connected to port 2 (index 1) and vice versa (and so on)
        #     if [ $(((i)%2)) -eq 0 ] ; then
        #         # Even
        #         otheri=$((i+2))
        #     else
        #         # Odd
        #         otheri=$((i))
        #     fi
        #     sudo ovs-ofctl add-flow dpdk-ovs-br0 in_port=$((i+1)),action=output:$otheri
        # done

        # In NORMAL mode, OVS acts as a simple MAC learning switch (no OpenFlow custom pipeline is used)
        sudo ovs-ofctl add-flow dpdk-ovs-br0 action=NORMAL

        # TODO: SCALE
        # sudo ovs-vsctl add-port dpdk-ovs-br0 sock0 \
        #     -- set Interface sock0 type=dpdkvhostuser ofport_request=1
        # sudo ovs-vsctl add-port dpdk-ovs-br0 sock1 \
        #     -- set Interface sock1 type=dpdkvhostuser ofport_request=2
        # FIXME: FOR NOW WE SETUP MANUALLY FLOWS BETWEEN PORTS
        # IDEA: STATICALLY CONFIGURED FLOWS MAY BE A GOOD CONFIGURATION (MAYBE)
        # sudo ovs-ofctl del-flows dpdk-ovs-br0

        # Following commands create manually bidirectional flows between sock0 and sock1
        # THIS OPERATION IS MANDATORY, OTHERWISE PACKETS ARE NOT SENT TO ANYBODY
        #sudo ovs-ofctl add-flow dpdk-ovs-br0 in_port=1,idle_timeout=0,action=output:2
        #sudo ovs-ofctl add-flow dpdk-ovs-br0 in_port=2,idle_timeout=0,action=output:1

        # sudo ovs-ofctl add-flow dpdk-ovs-br0 in_port=1,action=output:2
        # sudo ovs-ofctl add-flow dpdk-ovs-br0 in_port=2,action=output:1

        ;;
    snabb)
        num_local_ports="${#LXC_CONT_NAMES[@]}"

        snabb_cmd="taskset -c $SWITCH_CPUS sudo $SNABB $num_local_ports"
        for i in "${!LXC_CONT_NAMES[@]}" ; do
            snabb_cmd="$snabb_cmd /tmp/VIO/${LXC_CONT_SOCKS[i]}"
        done
        # TODO: add the external port

        screen -d -S screen_vswitch `log_vswitch_option` -m $snabb_cmd

        ;;
    sriov)
        # TODO: SCALE SR-IOV
        # Binds SR-IOV devices to DPDK driver and resets them, takes a while
        $SCRIPT_DIR/sriov_tools.bash $filename -b igb_uio

        sudo rm $SCRIPT_DIR/sriov.conf 2>/dev/null || true
        for i in "${!LXC_CONT_NAMES[@]}"; do
            echo "set vf mac addr 0 $i ${LXC_CONT_MACS[i]}" >> $SCRIPT_DIR/sriov.conf
        done
        echo "" >> $SCRIPT_DIR/sriov.conf

        # Start PF handler for PF0
        screen -d -S screen_vswitch `log_vswitch_option` -m \
            sudo $PMD -l $SWITCH_CPUS -n 2 --socket-mem 1024,0 --file-prefix=host \
            -w 04:00.0 -- --cmdline-file=$SCRIPT_DIR/sriov.conf

        # Some sleep needed, mabe not 80s, but better safe than sorry
        sleep 80s
        ;;
    linux-bridge)
        # This is not an actual switch, it is just used to start tests using normal kernel sockets
        # Hence this does nothing
        ;;
    *)
        echo "NOT A VALID VSWITCH NAME!"
        exit 1
        ;;
    esac
}

function stop_vswitch() {
    echo "Stopping vswitch..."
    case $vswitch in
    pmd)
        # Signal it to stop with Ctrl+D
        screen -S screen_vswitch -p 0 -X stuff "^D" || true

        # Wait for it to finish
        wait_screen screen_vswitch
        ;;
    basicfwd)
        # Signal it to stop with Ctrl+C
        screen -S screen_vswitch -p 0 -X stuff "^C" || true

        # Wait for it to finish
        wait_screen screen_vswitch

        # Extra cleanup required
        sudo rm /tmp/VIO/sock* || true
        ;;
    vpp)
        # Signal it to stop with Ctrl+C
        #screen -S screen_vswitch -p 0 -X stuff "^C" || true
        sudo pkill vpp

        # Wait for it to finish
        wait_screen screen_vswitch

        # Binds SR-IOV devices to no driver and resets them, takes a while
        $SCRIPT_DIR/sriov_tools.bash $filename -u
        ;;
    ovs)
        # Simply stop it
        sudo $OVS_DIR/ovs-ctl stop
        sudo umount /tmp/VIO

        # Binds SR-IOV devices to no driver and resets them, takes a while
        $SCRIPT_DIR/sriov_tools.bash $filename -u
        ;;
    snabb)
        # Signal it to stop with Ctrl+C
        screen -S screen_vswitch -p 0 -X stuff "^C" || true

        # Wait for it to finish
        wait_screen screen_vswitch

        # Extra cleanup required sometimes
        sudo rm /tmp/VIO/sock* 2>/dev/null || true
        ;;
    sriov)
        # Signal it to stop with Ctrl+D
        screen -S screen_vswitch -p 0 -X stuff "^D" || true

        # Wait for it to finish
        wait_screen screen_vswitch

        # Binds SR-IOV devices to no driver and resets them, takes a while
        $SCRIPT_DIR/sriov_tools.bash $filename -u
        ;;
    linux-bridge)
        # This is not an actual switch, it is just used to start tests using normal kernel sockets
        # Hence this does nothing
        ;;
    *)
        echo "NOT A VALID VSWITCH NAME!"
        exit 1
        ;;
    esac
}

function raw_mode_opt() {
    if [ $raw_sock == 1 ] ; then
        echo "-R eth0"
    fi
}

function noblock_mode_opt() {
    if [ $block_sock == 1 ] ; then
        echo "-B"
    fi
}

function multi_msg_opt() {
    if [ $multi_api == 1 ] ; then
        echo "-m"
    fi
}

function vswitch_cmdline_option() {
    case $vswitch in
    linux-bridge*)
        ;;
    sriov )
        echo "-s"
        ;;
    * )
        # Use simple DPDK
        echo "-d"
        ;;
    esac
}

function consume_data_option() {
    if [ "$consume_data" = "1" ] ; then
        echo "-c"
    fi
}

# TODO: other options
# TODO: raw socket option somehow
# NOTICE: -m option forces programs that support mmsg api to use it
# NOTICE: -s option enables silent mode for any application
function fill_commands() {
    for i in "${!LXC_CONT_NAMES[@]}"; do
        COMMANDS[$i]="/test.sh -m ${LXC_CONT_NAMES[i]} `vswitch_cmdline_option` ${LXC_CONT_CMDNAMES[i]} -- ${LXC_CONT_IPS[i]} ${LXC_CONT_MACS[i]} ${LXC_CONT_OTHER_IPS[i]} ${LXC_CONT_OTHER_MACS[i]} `silent_mode_opt` `raw_mode_opt` `noblock_mode_opt` `multi_msg_opt` -r $rate -b $bst_size -p $pkt_size `consume_data_option`"
    done
}

function load_conf_file() {
    source $filename
    fill_commands
}

# function build_commands() {
#     command_cont_a="/test.sh -m $CONT_A"
#     command_cont_b="/test.sh -m $CONT_B"
#     case $vswitch in
#     linux-bridge*)
#         ;;
#     sriov )
#         command_cont_a="$command_cont_a -s"
#         command_cont_b="$command_cont_b -s"
#         ;;
#     * )
#         # Use simple DPDK
#         command_cont_a="$command_cont_a -d"
#         command_cont_b="$command_cont_b -d"
#         ;;
#     esac
#     command_cont_a="$command_cont_a `get_command_name_cont_a`"
#     command_cont_b="$command_cont_b `get_command_name_cont_b`"
#     # TODO: other options
#     # TODO: raw socket option somehow
#     # NOTICE: -m option forces programs that support mmsg api to use it
#     # NOTICE: -s option enables silent mode for both applications
#     command_cont_a="$command_cont_a -- ${CONT_B_IP} ${CONT_B_MAC} `silent_mode_opt` `raw_mode_opt` `noblock_mode_opt` -m -r $rate -b $bst_size -p $pkt_size"
#     command_cont_b="$command_cont_b -- ${CONT_A_IP} ${CONT_A_MAC} `silent_mode_opt` `raw_mode_opt` `noblock_mode_opt` -m -r $rate -b $bst_size -p $pkt_size"
#     if [ "$consume_data" = "1" ] ; then
#         command_cont_a="$command_cont_a -c"
#         command_cont_b="$command_cont_b -c"
#     fi
#     # TODO: Additional dpdk options?
# }

function prepare_system_resources() {
    # Create VIO folder in tmp if it does not exist, reset its content
    # Insert igb_uio module if not present, skip errors for module already existing
    mkdir -p /tmp/VIO
    sudo umount /tmp/VIO 2>/dev/null || true
    sudo rm /tmp/VIO/sock* 2>/dev/null || true
    sudo modprobe uio || true
    sudo insmod $HOME/testbed/install/dpdk-19.05/lib/modules/`uname -r`/extra/dpdk/igb_uio.ko 2>/dev/null || true
    # TODO: add insmod to sriov_tools

    # Set processor to high performance, disable turbo boost
    for CPU_FREQ_GOVERNOR in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor;
    do [ -f $CPU_FREQ_GOVERNOR ] || continue;
        sudo echo -n performance | sudo tee $CPU_FREQ_GOVERNOR > /dev/null;
    done
    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null
}

function prepare_outdir() {
    mkdir -p $outdir

    rm $outdir/vswitch.log 2>/dev/null || true

    for i in "${!LXC_CONT_NAMES[@]}"; do
        rm $outdir/${LXC_CONT_NAMES[i]}_${LXC_CONT_CMDNAMES[i]}.log 2>/dev/null || true
    done

    # rm $outdir/${CONT_A}_`get_command_name_cont_a`.log 2>/dev/null || true
    # rm $outdir/${CONT_B}_`get_command_name_cont_b`.log 2>/dev/null || true
}

function start_applications() {
    echo "Starting the two applications, one per each container..."

    echo "Starting containers..."

    for i in "${!LXC_CONT_NAMES[@]}"; do
        # Start Container
        sudo lxc-start -n ${LXC_CONT_NAMES[i]}

        # Launch application inside container
        screen -d -S screen_${LXC_CONT_NAMES[i]} -L -Logfile ${outdir}/${LXC_CONT_NAMES[i]}_${LXC_CONT_CMDNAMES[i]}.log -m \
            sudo lxc-attach -n ${LXC_CONT_NAMES[i]} -- \
            ${COMMANDS[i]}
    done

    # sudo lxc-start -n $CONT_A
    # sudo lxc-start -n $CONT_B

    # # Start $CONT_A application
    # screen -d -S screen_cont_a -L -Logfile $outdir/${CONT_A}_`get_command_name_cont_a`.log -m \
    #     sudo lxc-attach -n $CONT_A -- \
    #     $command_cont_a

    # # Start $CONT_B application
    # screen -d -S screen_cont_b -L -Logfile $outdir/${CONT_B}_`get_command_name_cont_b`.log -m \
    #     sudo lxc-attach -n $CONT_B -- \
    #     $command_cont_b
}

function stop_applications() {
    # Stop the tests in containers
    echo "Stopping applications..."

    # Send Ctrl-C to each application in containers
    for i in "${!LXC_CONT_NAMES[@]}"; do
        screen -S screen_${LXC_CONT_NAMES[i]} -p 0 -X stuff "^C" &>/dev/null || true
    done

    # Wait for each container to terminate execution (up to a certain timeout)
    for i in "${!LXC_CONT_NAMES[@]}"; do
        wait_screen screen_${LXC_CONT_NAMES[i]}
    done

    # Stop containers
    for i in "${!LXC_CONT_NAMES[@]}"; do
        sudo lxc-stop -n ${LXC_CONT_NAMES[i]}
    done

    # screen -S screen_cont_a -p 0 -X stuff "^C" &>/dev/null || true
    # screen -S screen_cont_b -p 0 -X stuff "^C" &>/dev/null || true

    # # Wait for them to finish (or fixed a timeout)
    # wait_screen screen_cont_a
    # wait_screen screen_cont_b

    # # Stop containers
    # sudo lxc-stop -n $CONT_B
    # sudo lxc-stop -n $CONT_A
}

function wait_timeout() {
    echo "Waiting for $timeout..."
    sleep $timeout
}

function wait_schedule() {
    local current_epoch=$(date +%s)

    # Minimum wait is until the next minute o'clock (with a spare of 10 seconds)
    local current_mins=$(((current_epoch) / 60))
    local target_mins=$(((current_epoch+10) / 60 + 1))
    local reminder=$((target_mins % schedule))

    if [ ! "$reminder" = "0" ] ; then
        target_mins=$((target_mins + schedule - reminder))
    fi

    local delay_mins=$(((target_mins - current_mins)))

    local target_epoch=`date --date="+$delay_mins minutes" +%s`
    target_epoch=$(((target_epoch/60) * 60))
    local sleep_seconds=$((target_epoch - current_epoch))

    local target_epoch_print=`date --date="+$sleep_seconds seconds" '+%Y-%m-%d %H:%M:%S'`

    echo "Wait for scheduled time $target_epoch_print"
    sleep $sleep_seconds
}

################################################################################
#                              CONSTANTS AND DIRS                              #
################################################################################

vswitch_list="pmd vpp ovs basicfwd snabb sriov linux-bridge"
dimension_list="throughput latency latencyst"

# TODO: READ AN ENVIRONMENT VARIABLE
SCRIPT_DIR=`realpath $(dirname $0)`
PMD=$HOME/testbed/install/dpdk-19.05/bin/testpmd
BASICFWD=$HOME/testbed/install/dpdk-19.05/share/dpdk/examples/skeleton/x86_64-native-linuxapp-gcc/basicfwd
FDIO_DIR=$HOME/testbed/src/vpp
OVS_DIR=/usr/local/share/openvswitch/scripts
SNABB="$HOME/testbed/src/snabb/src/snabb custom_switch"

SWITCH_CPUS=0,2

#FIXME: MAKE DYNAMIC
# CONT_A=dpdk_c0
# CONT_B=dpdk_c1
# CONT_A_IP=10.0.3.5
# CONT_A_MAC=02:00:00:00:00:05
# CONT_B_IP=10.0.3.6
# CONT_B_MAC=02:00:00:00:00:06

################################################################################
#                              DEFAULT PARAMETERS                              #
################################################################################

default_timeout=60
default_filename=confrc.bash
default_rate=1000000
default_bst_size=32
default_pkt_size=64

################################################################################
#                                  VARIABLES                                   #
################################################################################

dimension=throughput
outdir=`realpath $(pwd)/out`
timeout=$default_timeout
filename=$default_filename
rate=$default_rate
bst_size=$default_bst_size
pkt_size=$default_pkt_size
vswitch=pmd
consume_data=0
test_params=0
debug=0
raw_sock=0
block_sock=0
schedule=0
multi_api=0

# command_cont_a=
# command_cont_b=

################################################################################
#                               MAIN SCRIPT FLOW                               #
################################################################################

parse_parameters "$@"
check_parameters
print_configuration

if [ "$test_params" = "1" ] ; then
    printf "Parameters were correct. Terminating without performing tests.\n"
    linesep
    exit 0;
fi

prepare_system_resources
load_conf_file
#build_commands
prepare_outdir

#start_processor_stats
start_vswitch

# If scheduled, wait at least 1 minute and for the next multiple of $schedule minutes
if [ ! "$schedule" = "0" ] ; then
    wait_schedule
fi

printf "STARTING TEST AT TIME: `date`\n"

start_applications

wait_timeout

stop_applications
stop_vswitch
#stop_processor_stats

# Test finished
printf "Test completed! Check the following files for test results:\n"

for i in "${!LXC_CONT_NAMES[@]}" ; do
    printf "%s\n" "$outdir/${LXC_CONT_NAMES[i]}_${LXC_CONT_CMDNAMES[i]}.log"
done
    # "$outdir/${CONT_A}_`get_command_name_cont_a`.log" \
    # "$outdir/${CONT_B}_`get_command_name_cont_b`.log"

linesep
printf '\n'
exit 0
