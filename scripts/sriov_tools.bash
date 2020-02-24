#!/bin/bash

dpdk_devbind=$HOME/testbed/install/dpdk-19.05/share/dpdk/usertools/dpdk-devbind.py

escapeForwardSlashes() {
    # Validate parameters
    if [ -z "$1" ]
    then
    echo -e "Error - no parameter specified!"
    return 1
    fi

    # Perform replacement
    echo ${1} | sed -e "s#/#\\\/#g"
    return 0
}

comment_out_pattern()
{
    patt=`escapeForwardSlashes $1`
    sudo sed -i "/$patt/s/^/#/g" $2
}

uncomment_pattern()
{
    patt=`escapeForwardSlashes $1`
    sudo sed -i "/$patt/s/^#//g" $2
}

unbind_pf()
{
    sudo $dpdk_devbind --force -u 04:00.0
    sleep 2
}

unbind_all()
{
    sudo $dpdk_devbind --force -u 04:00.0

    for i in "${!LXC_CONT_NAMES[@]}" ; do
        sudo $dpdk_devbind --force -u ${LXC_CONT_VFPCIS[i]}
    done
    sleep 2
}

bind_pf()
{
    sudo $dpdk_devbind -b $1 04:00.0
    sleep 2
}

bind_vfs()
{
    # sudo $dpdk_devbind -b $1 04:02.0 04:02.1
    for i in "${!LXC_CONT_NAMES[@]}" ; do
        sudo $dpdk_devbind -b $1 ${LXC_CONT_VFPCIS[i]}
    done
    sleep 2
}

# arg: how many vfs to create
create_vfs_i40e()
{
    sudo sh -c "echo $1 > /sys/class/net/enp4s0f0/device/sriov_numvfs"
    sleep 2
}

delete_vfs_i40e()
{
    sudo sh -c "echo 0 > /sys/class/net/enp4s0f0/device/sriov_numvfs"
    sleep 2
}

# arg: how many vfs to create temporarily
reset_vfs_i40e()
{
    delete_vfs_i40e
    create_vfs_i40e $1
    delete_vfs_i40e
}

# arg: how many vfs to create
create_vfs_dpdk()
{
    sudo sh -c "echo $1 > /sys/bus/pci/devices/0000\:04\:00.0/max_vfs"
    sleep 2
}

show()
{
    $dpdk_devbind -s
}

help()
{
cat << EOF
Usage:
    $0 <filename> [-r|--reset]                  To reset the SR-IOV device to be used by DPDK
    $0 <filename> [-b|--bind] [i40e|igb_uio]    To remove all vfs and bind the pf to a certain driver
    $0 <filename> [-u|--unbind]                 To unbind all devices from any driver
                                                                (faster than bind when another driver is not needed)
    $0 [-s|--show]                              To show some info about Ethernet PCI devices
    $0 [-h|--help]                              To print this help message

The <filename> must be a valid test configuration.

EOF
}

# arg: driver to which devices should be bound
bind()
{
    unbind_pf
    bind_pf i40e
    reset_vfs_i40e ${#LXC_CONT_NAMES[@]}
    unbind_pf
    bind_pf $1
    case $1 in
        i40e)
            create_vfs_i40e ${#LXC_CONT_NAMES[@]}
            # bind_vfs i40evf # Unnecessary

            # Need to comment out lines that require
            # SR-IOV device to start containers
            # in each container configuration file

            for i in "${!LXC_CONT_NAMES[@]}" ; do
                comment_out_pattern '^\s*lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/${LXC_CONT_NAMES[i]}/config
            done
            # comment_out_pattern '^\s*lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/DPDKTest/config
            # comment_out_pattern '^\s*lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/DPDKC1/config
            ;;
        igb_uio)
            create_vfs_dpdk ${#LXC_CONT_NAMES[@]}
            bind_vfs igb_uio

            # Need to re-enable lines that mount
            # SR-IOV devices when starting containers

            for i in "${!LXC_CONT_NAMES[@]}" ; do
                uncomment_pattern 'lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/${LXC_CONT_NAMES[i]}/config
            done
            # uncomment_pattern 'lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/DPDKTest/config
            # uncomment_pattern 'lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/DPDKC1/config
            ;;
    esac
}

reset()
{
    bind igb_uio
}

unbind()
{
    unbind_all
    for i in "${!LXC_CONT_NAMES[@]}" ; do
        comment_out_pattern '^\s*lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/${LXC_CONT_NAMES[i]}/config
    done
    # comment_out_pattern '^\s*lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/DPDKTest/config
    # comment_out_pattern '^\s*lxc.mount.entry\s*=\s*/dev/uio' /var/lib/lxc/DPDKC1/config
}

# FIXME: Argument checks are not performed at all!

nextarg=1
furtherarg=2
if [ $# -ge 1 ] && [ ! ${1:0:1} == "-" ] ; then
    filename=$1
    source $filename
    nextarg=2
    furtherarg=3
fi

case ${!nextarg} in
    -r|--reset)
        if [ $nextarg != 2 ] ; then
            help
        else
            reset
        fi
        ;;
    -u|--unbind)
        if [ $nextarg != 2 ] ; then
            help
        else
            unbind
        fi
        ;;
    -b|--bind)
        if [ $nextarg != 2 ] ; then
            help
        else
            # Must have another argument
            bind ${!furtherarg}
        fi
        ;;
    -s|--show)
        show
        ;;
    -h|--help|"")
        help
        ;;
    *)
        echo "Invalid command!"
        help
        ;;
esac
