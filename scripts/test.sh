#!/bin/sh

set -e
# set -x

# The first argument is the testapp application to start
if [ $# -lt 1 ] ; then
    echo "BASIC USAGE: $0 [send|recv|client|clientst|server] <args>"
    echo "To print a more detailed help type: $0 help"
    exit 1
fi

# FIXME: move freely this argument before the --

command=send
use_dpdk=0
use_sriov=0
whichcont=DPDKTest

echo "ARGS: $@"
OPTIND=1

# Iterate through arguments and options until -- is reached
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    while getopts ":dsm:" opt; do
        case ${opt} in
        d)
            use_dpdk=1
            shift
            OPTIND=1
            ;;
        s)
            use_sriov=1
            shift
            OPTIND=1
            ;;
        m)
            whichcont=$OPTARG
            shift
            shift
            OPTIND=1
            ;;
        \?)
            echo "Invalid option: $OPTARG"
            exit 1
            ;;
        :)
            echo "Invalid option: $OPTARG requires an argument"
            exit 1
            ;;
        esac
    done
    # shift $((OPTIND-1))

    # While the first character is not an '-', substitute the command
    while [ $# -gt 0 ] && ! [ "`echo "$1" | cut -c1-1`" = '-' ]; do
        command="$1"
        shift
    done

    OPTIND=1
done
shift

case ${command} in
send|recv|client|clientst|server)
    ;;
help)
    # TODO: print_help
    exit 0
    ;;
*)
    echo "ERROR: unrecognized command '$command'"
    exit 1
    ;;
esac

if [ "$use_sriov" -eq "1" ] ; then
    use_dpdk=1
fi

if [ "$use_dpdk" -eq "1" ] ; then
    command=dpdk-${command}
fi

if [ ! -e /conf/${whichcont}_rc.conf ] ; then
    echo "ERROR: WRONG PARAMETER TO -m OPTION: ${whichcont}"
    echo "       USE A VALID CONTAINER NAME"
    exit 1
fi

# Load local configuration from file
source /conf/${whichcont}_rc.conf

# Translate configuration from file content to variables used in previous version
cpumask=`/cpu_list_to_mask $CONT_CPU`
cpulist=$CONT_CPU
#localmac=$CONT_MAC
#localip=$CONT_IP
pciaddr=$CONT_VFPCI
vsock=$CONT_SOCK

# They shall be provided somehow (can be given along with normal arguments after the first "--", inside command_args)
remoteip=
remotemac=

# FIXME: missing parameters:
# remoteip remotemac

command_args="${localip} ${localmac} ${remoteip} ${remotemac}"
OPTIND=1
for arg in "$@"
do
    OPTIND=$((OPTIND +1))

    if [ "$arg" = "--" ] ; then
        break
    fi

    command_args="${command_args} ${arg}"
done
shift $(($OPTIND -1))

if [ "$use_dpdk" -eq "1" ] ; then
    dpdk_args="-n 4 -l ${cpulist} --file-prefix=cont-${whichcont}"

    if [ "$use_sriov" -eq "1" ] ; then
        dpdk_args="${dpdk_args} -w ${pciaddr}"
    else
        dpdk_args="${dpdk_args} --vdev=virtio_user0,path=/var/run/VIO/${vsock} --no-pci"
    fi

    OPTIND=1
    for arg in "$@"
    do
        OPTIND=$((OPTIND +1))

        if [ "$arg" = "--" ] ; then
            break
        fi

        dpdk_args="${dpdk_args} ${arg}"
    done
    shift $(($OPTIND -1))
else
    dpdk_args=""
fi

#echo "REMAINING ARGS NUM: $#"
#echo "COMMAND: taskset ${cpumask} /testapp ${command} ${command_args} -- ${dpdk_args}"

set -x
taskset ${cpumask} /testapp ${command} ${command_args} -- ${dpdk_args}

exit 0
