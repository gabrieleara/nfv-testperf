#!/bin/bash

set -e

################## FUNCTIONS ##################

parse_parameters() {
    args=''
    for opt in "$@"
    do
        case $opt in
        -i)
            # Install command, default action
            cmd=i
            ;;
        -d)
            # Delete command
            cmd=d
            ;;
        -y)
            # Accept all default parameters
            default=y
            ;;
        *)
            # Other argument
            args="${args}_${opt}_"
            ;;
        esac
    done

    # NOTICE: underscores are important
    if [ -z "$args" ] || [[ "$args" == *_all_* ]] ; then
        args="_all_rootfs_lxc_dpdk_ovs_vpp_snabb_app_"
    fi
}

# Ask the location of a folder to the user, with an optional default value
# Args: $1 is the prompt, $2 is the default value
# It will ask until a valid new location is provided
asklocation() {
    local response
    response=""

    read -rep "$1 [$2]: " response

    if [ -z "$response" ]; then
        response=$2
    fi

    # Create a directory and at the same time test whether it is a valid
    # potential location
    mkdir -p $response 2> /dev/null

    while [ $? -ne 0 ]; do
        read -rep $'The specified location does not exist!\n'"$1 [$2]: " response
        mkdir -p $response 2> /dev/null
    done

    echo "$response"
}

echo_sep_line() {
    printf '+'
    printf '%0.s-' $(seq 1 $1)
    printf '+'
    printf '\n'
}

# Prints the title of the next section being executed, which is the first argument
echo_section() {
    boxsize=80

    str=$1
    len=$((${#str}+2)) # Two vertical lines, before and after

    if [ $len -lt $boxsize ] ; then
        pad=$((boxsize - len))
        lpad=$((pad / 2))
        rpad=$((pad - lpad))

        if [ $lpad -ne 0 ] ; then
            lpad=$(printf '%.0s ' $(seq 1 $lpad))
        else
            lpad=''
        fi

        rpad=$(printf '%.0s ' $(seq 1 $rpad))
    else
        boxsize=$len
        lpad=''
        rpad=''
    fi

    printf "${COLOR}"
    echo_sep_line $boxsize
    printf '| %s%s%s |\n' "$lpad" "$str" "$rpad"
    echo_sep_line $boxsize
    printf "${NC}"
}

# Prints an highlighted message on the screen
echo_message() {
    printf "${COLOR}"
    printf "##\n"
    printf "## %s\n" $1
    printf "##\n"
    printf "${NC}"
}

# Prints an highlighted action on a single line
echo_action() {
    printf "${COLOR}  -- $1${NC}\n"
}

echo_warning() {
    printf "${COLOR_WARN}  -- $1${NC}\n"
}

get_make_cpus() {
    local cpunum
    local makecpus

    # To parallelize better make command, let's get the number of CPUs (including hyperthr.)
    cpunum=$(grep -c ^processor /proc/cpuinfo)

    # Set MAKE_AGGRESSIVE environment variable to 1 to fully utilize the system
    # in make commands
    if [ ! -z "$MAKE_AGGRESSIVE" ] && [ "$MAKE_AGGRESSIVE" -eq 1 ] ; then
        makecpus=$(echo $cpunum*1.5 | bc)
        makecpus=$(printf "%.0f\n" $makecpus)
    else
        makecpus=$(echo $cpunum*0.7 | bc)
        makecpus=$(printf "%.0f\n" $makecpus)
    fi

    echo $makecpus
}

############ DIRECTORIES FUNCTIONS ############

# Creates basic directory structure
directories_install() {
    echo_section "Installing directories"

    echo_action "MKDIR $DIR_BASE" ;     mkdir -p $DIR_BASE
    echo_action "MKDIR $DIR_SRC" ;      mkdir -p $DIR_SRC
    echo_action "MKDIR $DIR_BUILD" ;    mkdir -p $DIR_BUILD
    echo_action "MKDIR $DIR_INSTALL" ;  mkdir -p $DIR_INSTALL
    echo_action "MKDIR $DIR_CONFIG" ;   mkdir -p $DIR_CONFIG
    mkdir -p /tmp/VIO
}

# Deletes all the directories
directories_remove() {
    echo_section "Removing directories"
    echo_action "RM $DIR_BASE" ;        rm -rf $DIR_BASE
}

############# PACKAGES FUNCTIONS ##############

packages_install() {
    echo_section "Checking if required packages are installed"

    echo_action "APT INSTALL DEP"
    sudo apt update && sudo apt install git build-essential libnuma-dev lxc pkg-config python3-pip python-pip automake -y
}

############# BUILDCORE FUNCTIONS #############

rootfs_install() {
    echo_section "Installing the rootfs"

    # IF ALREADY INSTALLED, SKIP
    if [ -e $ROOTFS_CHECK_FILE ] ; then
        echo_action "ALREADY INSTALLED -- SKIPPING"
        return 0
    fi

    echo_action "CLONING BUILDCORE"
    git clone https://gitlab.retis.santannapisa.it/l.abeni/BuildCore $DIR_BUILDCORE || true

    echo_action "BUILDING BUILDCORE"
    mkdir -p $DIR_BUILDCORE_BUILD
    pushd $DIR_BUILDCORE_BUILD
        rm -rf $DIR_BUILDCORE_BUILD/*
        sh $DIR_BUILDCORE/buildcore.sh $DIR_BUILDCORE_BUILD/test.gz

        echo_action "RM UNUSED ARCHIVES"
        rm $DIR_BUILDCORE_BUILD/test.gz $DIR_BUILDCORE_BUILD/busybox-1.29.3.tar.bz2  $DIR_BUILDCORE_BUILD/sudo-1.8.26.tar.gz
        # TODO: remove other stuff too ?
    popd

    # Copy rootfs into install directory
    echo_action "CP ROOTFS TO INSTALLDIR"
    sudo rm -rf $DIR_ROOTFS
    cp -a $DIR_BUILDCORE_BUILD/bb_build-1.29.3/_install $DIR_ROOTFS

    # Copy libnuma library into container filesystem
    cp /usr/lib/x86_64-linux-gnu/libnuma.so.1 $DIR_ROOTFS/lib

    # VERY IMPORTANT: Remove ttyS0 from inittab file, otherwise you get rogue
    # GETTY processes consuming 100% CPU after the container is shut down
    sed -e 's/^ttyS0/#&/g' -i $DIR_ROOTFS/etc/inittab

    # Create some necessary directories
    sudo mkdir -p $DIR_ROOTFS/rootfs/var/run
    sudo mkdir -p $DIR_ROOTFS/rootfs/var/run/VIO
    sudo mkdir -p $DIR_ROOTFS/rootfs/var/run/hugepages

    # TODO: a directory where to put user-config for each container

    # Put location of rootfs in a file configuration
    echo "DIR_ROOTFS=$DIR_ROOTFS" > $CONF_ROOTFS_RC

    # This is used to check whether it is installed successfully or not
    touch $ROOTFS_CHECK_FILE
}

rootfs_remove() {
    echo_section "Removing the rootfs"

    rm -f $ROOTFS_CHECK_FILE

    echo_action "RM $DIR_BUILDCORE" ;       rm -rf $DIR_BUILDCORE
    echo_action "RM $DIR_BUILDCORE_BUILD" ; rm -rf $DIR_BUILDCORE_BUILD
    echo_action "RM $DIR_ROOTFS" ;          sudo rm -rf $DIR_ROOTFS
}

################ LXC FUNCTIONS ################

# IDEA: containers created and destroyed 'on the go' by the testbed

# Requires as input:
# - <container_name>
# - <cpu_set>
# - <name_sriov_vf> (expressed as '????' - unused for now but still required)
# - <mac_address>
# - <ip_address>
# - <netmask>
# - <sriov_dev_name> (expressed as 'uioN')
lxc_create_container() {
    local container_name=$1
    local cpu_set=$2
    local name_sriov_vf=$3
    local mac_address=$4
    local ip_address=$5
    local netmask=$6
    local sriov_dev_name=$7

    sudo lxc-create $container_name -t none || true

    local FILE_BASE_CONFIG=$DIR_INSTALL_DATA/lxcbase.config
    local FILE_CONT_CONFIG=/tmp/${container_name}.config

    cp $FILE_BASE_CONFIG $FILE_CONT_CONFIG
    sed -i -e "s#<rootfs_absolute_path>#${DIR_ROOTFS}#g"    $FILE_CONT_CONFIG
    sed -i -e "s#<container_name>#${container_name}#g"      $FILE_CONT_CONFIG
    sed -i -e "s#<cpu_set>#${cpu_set}#g"                    $FILE_CONT_CONFIG
    sed -i -e "s#<name_sriov_vf>#${name_sriov_vf}#g"        $FILE_CONT_CONFIG
    sed -i -e "s#<mac_address>#${mac_address}#g"            $FILE_CONT_CONFIG
    sed -i -e "s#<ip_address>#${ip_address}#g"              $FILE_CONT_CONFIG
    sed -i -e "s#<netmask>#${netmask}#g"                    $FILE_CONT_CONFIG
    sed -i -e "s#<sriov_dev_name>#${sriov_dev_name}#g"      $FILE_CONT_CONFIG

    sudo cp $FILE_CONT_CONFIG /var/lib/lxc/${container_name}/config
}

lxc_install() {
    echo_section "Creating LXC Containers"

    # FIXME: change to finalize installation part to copy stuff again to rootfs
    echo_action "LXC INSTALL DEP [ROOTFS]"
    [ ! -e $ROOTFS_CHECK_FILE ] && rootfs_install

    # File may be non existent, no containers will be created
    source $DIR_INSTALL_DATA/lxcconfrc.bash || true

    # If no container is specified, do not install anything
    if [ "${#LXC_CONT_NAMES[@]}" -lt 1 ] ; then
        return 0
    fi

    # Now we have some configuration variables that we can iterate to create LXC
    # containers beforehand, to use them later during tests

    # First clear a few configuration files
    truncate -s 0 $CONF_SRIOV_DEVLIST
    truncate -s 0 $CONF_SRIOV_TESTPMD

    # Create a directory for containers rc configuration files
    rm -rf $DIR_ROOTFS/conf
    mkdir -p $DIR_ROOTFS/conf

    # Iterate through containers specification
    for i in "${!LXC_CONT_NAMES[@]}"; do
        echo_action "LXC-CREATE ${LXC_CONT_NAMES[i]}"
        lxc_create_container        \
            ${LXC_CONT_NAMES[i]}   \
            ${LXC_CONT_CPUS[i]}    \
            ${LXC_CONT_VFS[i]}     \
            ${LXC_CONT_MACS[i]}    \
            ${LXC_CONT_IPS[i]}     \
            ${LXC_CONT_NMASKS[i]}  \
            ${LXC_CONT_VFDEVS[i]}

# Create a configuration file with these variables for each container
cat > $DIR_ROOTFS/conf/${LXC_CONT_NAMES[i]}_rc.conf << EOF
CONT_NAME=${LXC_CONT_NAMES[i]}
CONT_CPU=${LXC_CONT_CPUS[i]}
CONT_VF=${LXC_CONT_VFS[i]}
CONT_MAC=${LXC_CONT_MACS[i]}
CONT_IP=${LXC_CONT_IPS[i]}
CONT_NMASK=${LXC_CONT_NMASKS[i]}
CONT_VFDEV=${LXC_CONT_VFDEVS[i]}
CONT_VFPCI=${LXC_CONT_VFPCIS[i]}
CONT_SOCK=${LXC_CONT_SOCKS[i]}
EOF

        # Add to the two run commands configuration files for SR-IOV the specs
        # of the current container
        # FIXME: check whether a dynamic SRIOV_TESTPMD_CONF file is necessary
        echo ${LXC_CONT_VFPCIS[i]}                          >> $CONF_SRIOV_DEVLIST
        echo "set vf mac addr 0 $i ${LXC_CONT_VFPCIS[i]}"   >> $CONF_SRIOV_TESTPMD
    done

    # MAGIC WAIT - DO NOT REMOVE, OTHERWISE NEXT OPERATION MAY FAIL
    # IF IT DOES, JUST REPEAT THE INSTALLATION COMMAND, IT WILL SKIP ALL THE
    # SKIPPABLE STUFF WHILE COMPLETING CORRECTLY
    sleep 4s

    echo_warning "NOTICE: Next sequence of commands may lead to failure of install command"
    echo_warning "        If that happens, just execute again install command with the same parameters as before"
    echo_warning "        In that case, it is guaranteed to succeed"

    # When all containers are created, change 'myself' user password to no password
    # The command is repeated because usually the first time fails.
    # If the last attempt fails, the installation fails
    # TODO: check whether this works
    sudo lxc-start      -n ${LXC_CONT_NAMES[0]}
    (sudo lxc-attach    -n ${LXC_CONT_NAMES[0]} -- passwd -d myself) || true
    (sudo lxc-attach    -n ${LXC_CONT_NAMES[0]} -- passwd -d myself) || true
    (sudo lxc-attach    -n ${LXC_CONT_NAMES[0]} -- passwd -d myself)
    sudo lxc-stop       -n ${LXC_CONT_NAMES[0]}
}

lxc_remove() {
    echo_section "Destroying LXC Containers"

    # File may be non existent, no containers will be created
    source $DIR_INSTALL_DATA/lxcconfrc.bash || true

    # If no container is specified, do not remove anything
    if [ "${#LXC_CONT_NAMES[@]}" -lt 1 ] ; then
        return 0
    fi

    # Iterate through containers specification to delete them
    for i in "${!LXC_CONT_NAMES[@]}"; do
        echo_action "LXC-DESTROY ${LXC_CONT_NAMES[i]}"
        local cont_name=${LXC_CONT_NAMES[i]}

        # Create dummy rootfs that will be deleted by the destroy command
        sudo mkdir -p /var/lib/lxc/${cont_name}/rootfs

        # Chanhge rootfs in config of each container to point to the dummy one
        sudo sed -i -e "s#${DIR_ROOTFS}#/var/lib/lxc/${cont_name}/rootfs#g" \
            /var/lib/lxc/${cont_name}/config || true

        # Stop container if running
        sudo lxc-stop ${cont_name} || true

        # Destroy container
        sudo lxc-destroy ${cont_name} || true

        # Remove all container if directory still exists
        sudo rm -rf /var/lib/lxc/${cont_name}
    done
}

############### DPDK FUNCTIONS ################

dpdk_install() {
    echo_section "Installing DPDK"

    # IF ALREADY INSTALLED, SKIP
    if [ -e $DPDK_CHECK_FILE ] ; then
        echo_action "ALREADY INSTALLED -- SKIPPING"
        return 0
    fi

    # Downloading and extracting DPDK sources
    echo_action "DOWNLOAD DPDK $DPDK_VERSION"
    pushd $DIR_BASE
        wget http://fast.dpdk.org/rel/dpdk-${DPDK_VERSION}.tar.xz -O dpdk.tar.xz

        rm -rf $DIR_DPDK_SRC
        mkdir -p $DIR_DPDK_SRC
        tar xvf dpdk.tar.xz -C $DIR_DPDK_SRC --strip 1
        rm dpdk.tar.xz
    popd

    # Changing a configuration setting for i40e driver
    sed -i -E "s/CONFIG_RTE_LIBRTE_I40E_16BYTE_RX_DESC=n/CONFIG_RTE_LIBRTE_I40E_16BYTE_RX_DESC=y/" $DIR_DPDK_SRC/config/common_base

    rm -rf $DIR_DPDK_BUILD
    rm -rf $DIR_DPDK_INSTALL

    # Build and install
    echo_action "BUILD AND INSTALL DPDK $DPDK_VERSION (-j $MAKECPUS)"
    make -C $DIR_DPDK_SRC -j $MAKECPUS install \
        RTE_SDK=$DIR_DPDK_SRC \
        RTE_OUTPUT=$DIR_DPDK_BUILD \
        DESTDIR=$DIR_DPDK_INSTALL \
        T=x86_64-native-linuxapp-gcc \
        EXTRA_CFLAGS=-g \

    # Pushing all environment variables for DPDK into a single file
    # TODO: is there anything else to be pushed into this file?
cat > $CONF_DPDK_RC << EOF
export RTE_SDK=${DIR_DPDK_INSTALL}/share/dpdk/
export RTE_TARGET=x86_64-native-linuxapp-gcc
export TESTPMD=${DIR_DPDK_INSTALL}/bin/testpmd
EOF

    # Source it # TODO: STANDARD WAY TO FIND THIS FILE
    source $CONF_DPDK_RC

    echo_action "BUILD DPDK EXAMPLES (-j $MAKECPUS)"
    pushd $DIR_DPDK_INSTALL/share/dpdk
        make -C examples -j $MAKECPUS EXTRA_CFLAGS=-g
    popd

    touch $DPDK_CHECK_FILE
}

dpdk_remove() {
    echo_section "Removing DPDK $DPDK_VERSION"

    rm -f $DPDK_CHECK_FILE

    echo_action "RM $DIR_DPDK_SRC" ;        rm -rf $DIR_DPDK_SRC
    echo_action "RM $DIR_DPDK_BUILD" ;      rm -rf $DIR_DPDK_BUILD
    echo_action "RM $DIR_DPDK_INSTALL" ;    rm -rf $DIR_DPDK_INSTALL
}

################ VPP FUNCTIONS ################

vpp_install() {
    echo_section "Installing VPP from APT [Latest version] "

    # Setting up the repository and adding key
    sudo sh -c "echo 'deb [trusted=yes] https://packagecloud.io/fdio/release/ubuntu bionic main' > /etc/apt/sources.list.d/99fd.io.list"
    curl -L https://packagecloud.io/fdio/release/gpgkey | sudo apt-key add -

    echo_action "APT INSTALL VPP-REQUIRED"

    # Install Mandatory Packages
    sudo apt update
    sudo apt install vpp vpp-plugin-core vpp-plugin-dpdk -y

    echo_action "APT INSTALL VPP-OPTIONAL"

    # Install Optional Packages
    sudo apt install vpp-api-python python3-vpp-api vpp-dbg vpp-dev -y

    # Disable automatic VPP execution as a service
    sudo systemctl stop vpp || true
    sudo systemctl disable vpp

    # Copy startup configuration for VPP in install location
    cp $DIR_INSTALL_DATA/vppconfig.conf $CONF_VPP
}

vpp_remove() {
    echo_section "Removing VPP using APT"

    sudo apt remove --purge vpp* python3-vpp-api -y
    sudo apt autoremove -y
}

# To run VPP use:
# sudo service vpp start
# Or better, use
# sudo vpp -c startup.conf
# and kill it later with sudo pkill vpp
# TODO: create this startup.conf file somewhere
#
# NOTICE: To check whether VPP is running use
# ps -eaf | grep vpp | grep -v "grep" | wc -l
# the output should be one
#
# To stop VPP use:
# sudo service vpp stop
#

################ OVS FUNCTIONS ################

ovs_install() {
    echo_section "Installing Open vSwitch"

    echo_action "OVS INSTALL DEP [DPDK]"
    [ ! -e $DPDK_CHECK_FILE ] && dpdk_install

    # IF ALREADY INSTALLED, SKIP
    if [ -e $OVS_CHECK_FILE ] ; then
        echo_action "ALREADY INSTALLED -- SKIPPING"
        return 0
    fi

    # Downloading and extracting OVS sources
    echo_action "DOWNLOAD OVS $OVS_VERSION"
    pushd $DIR_BASE
        wget https://www.openvswitch.org/releases/openvswitch-${OVS_VERSION}.tar.gz -O ovs.tar.gz

        rm -rf $DIR_OVS_SRC
        mkdir -p $DIR_OVS_SRC
        tar xvf ovs.tar.gz -C $DIR_OVS_SRC --strip 1
        rm ovs.tar.gz

        # More recent version of DPDK changed the name of a constant, value/purpose is the same
        find $DIR_OVS_SRC -type f | xargs sed -i 's/e_RTE_METER_GREEN/RTE_COLOR_GREEN/g'
    popd

    echo_action "PIP INSTALL DEP"
    sudo pip install -I ovs-sphinx-theme
    sudo pip install -I docutils

    pushd $DIR_OVS_SRC
        echo_action "OVS CONFIGURE BUILD"
        ./configure --with-dpdk=$DIR_DPDK_BUILD

        echo_action "OVS BUILD (-j $MAKECPUS)"
        make -j $MAKECPUS

        echo_action "OVS INSTALL"
        sudo make install # TODO: change to another destination?
    popd

    # Configuring OVS

    # Kill potential processes still running
    sudo killall ovsdb-server ovs-vswitchd || true

    # Creating the DB
    echo_action "OVS CREATE DB"
    sudo mkdir -p /usr/local/var/run/openvswitch
    sudo rm -f /usr/local/var/run/openvswitch/conf.db
    sudo ovsdb-tool create /usr/local/var/run/openvswitch/conf.db $DIR_OVS_SRC/vswitchd/vswitch.ovsschema

    # Starting the server
    sudo ovsdb-server /usr/local/var/run/openvswitch/conf.db \
        --remote=punix:/usr/local/var/run/openvswitch/db.sock \
        --remote=db:Open_vSwitch,Open_vSwitch,manager_options \
        --pidfile --detach --log-file

    # Initializing the DB
    sudo ovs-vsctl --no-wait init

    # FIXME: SET THIS AT RUNTIME
    # Open vSwitch DPDK options [DPDK can run only on core 0 and 2]
    #sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-init=true
    #sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-lcore-mask=0x5
    #sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-socket-mem="1024,1024"

    # Can execute only on cores 0 and 2
    #sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:pmd-cpu-mask=0x5

    # TODO: port configuration at run time (should also ignore PCI devices
    # when running on single host)

    # TODO: THIS SHOULD BE IN A .ovsrc FILE
    export PATH="$PATH:/usr/local/share/openvswitch/scripts"

    # Stop the service after configuration
    # The which is necessary because sudo ignores user PATH
    sudo `which ovs-ctl` stop

    touch $OVS_CHECK_FILE
}

ovs_remove() {
    echo_section "Removing OVS $OVS_VERSION"

     # IF NOT INSTALLED, SKIP
    if [ ! -e $OVS_CHECK_FILE ] ; then
        echo_action "NOT INSTALLED -- SKIPPING"
        return 0
    fi

    if [ ! -e $DPDK_CHECK_FILE ] ; then
        echo_action "UNINSTALLING OVS REQUIRES DPDK INSTALLED"
        echo_action "INSTALL DPDK BEFORE REMOVING OVS,"
        echo_action "THEN UNINSTALL DPDK AGAIN"
        return 0
    fi

    # Kill potential processes still running
    sudo killall ovsdb-server ovs-vswitchd || true

    rm -f $OVS_CHECK_FILE

    if [ ! -d $DIR_OVS_SRC ] ; then
        echo_action "UNINSTALLING OVS REQUIRES ITS SOURCES IN $DIR_OVS_SRC DIRECTORY!"
        return 0
    fi

    pushd $DIR_OVS_SRC
        echo_action "OVS CONFIGURE BUILD"
        ./configure --with-dpdk=$DIR_DPDK_BUILD

        echo_action "OVS UNINSTALL"
        sudo make uninstall
    popd

    echo_action "RM $DIR_OVS_SRC" ; rm -rf $DIR_OVS_SRC
}

############### SNABB FUNCTIONS ###############

snabb_install() {
    echo_section "Installing Snabb"

    # IF ALREADY INSTALLED, SKIP
    if [ -e $SNABB_CHECK_FILE ] ; then
        echo_action "ALREADY INSTALLED -- SKIPPING"
        return 0
    fi

    rm -rf $DIR_SNABB_SRC
    git clone https://github.com/snabbco/snabb.git $DIR_SNABB_SRC

    mkdir -p $DIR_SNABB_PROGRAM
    cp $SNABB_INPUT_FILE $SNABB_PROGRAM_FILE

    pushd $DIR_SNABB_SRC
        echo_action "SNABB BUILD (-j $MAKECPUS)"
        make -j $MAKECPUS
    popd

    touch $SNABB_CHECK_FILE
}

snabb_remove() {
    echo_section "Removing Snabb"

    rm -f $SNABB_CHECK_FILE

    echo_action "RM $DIR_SNABB_SRC" ;   rm -rf $DIR_SNABB_SRC
}

############## TESTAPP FUNCTIONS ##############

# TODO:
testapp_install() {
    echo_section "Installing testing application in ROOTFS"

    echo_action "TESTAPP INSTALL DEP [ROOTFS]"
    [ ! -e $ROOTFS_CHECK_FILE ] && rootfs_install

    echo_action "TESTAPP INSTALL DEP [DPDK]"
    [ ! -e $DPDK_CHECK_FILE ] && dpdk_install

    source $CONF_DPDK_RC

    # Installation is performed automatically by make command
    echo_action "MAKE INSTALL TESTAPP (-j $MAKECPUS)"
    make -C $DIR_TESTAPP -j $MAKECPUS

    make -C $DIR_CPUMASK -j $MAKECPUS
}


############### FINAL FUNCTIONS ###############

finalize_install() {
    echo_section "Finalizing Installation"

    # Copy Test script to rootfs
    if [ -e $ROOTFS_CHECK_FILE ] ; then
        # TODO: OTHER VARIABLE
        cp -T $DIR_SCRIPT/scripts/test.sh $DIR_ROOTFS/test.sh
    fi

    # Copy Testpmd app to rootfs
    if [ -e $ROOTFS_CHECK_FILE ] && [ -e $DPDK_CHECK_FILE ] ; then
        source $CONF_DPDK_RC
        cp $TESTPMD $DIR_ROOTFS
    fi

    # Configure the machine to use hugepages
    echo_action "Configuring HUGEPAGES on Linux kernel options [REBOOT TO APPLY CHANGES]"

    HUGEPAGE_CMDLINE_OPTION="hugepagesz=1G hugepages=32 intel_iommu=on iommu=pt"
    sudo sed -i "s#\(GRUB_CMDLINE_LINUX_DEFAULT\s*=\s*\)\"\(.*\)\"#\1\"\2 $HUGEPAGE_CMDLINE_OPTION\"#g" /etc/default/grub

    # Remove potential duplicates
    sudo sed -i "s#$HUGEPAGE_CMDLINE_OPTION\(.*$HUGEPAGE_CMDLINE_OPTION\)#\1#g" /etc/default/grub

    echo_action "UPDATE-GRUB2"
    sudo update-grub2

    # TODO: FIX, WORKS BUT CAN BE BETTER
    # LINE MUST BE: hugetlbfs /dev/hugepages hugetlbfs rw,relatime,pagesize=1024M 0 0
    HUGEPAGE_TLB_FSTAB="hugetlbfs   /dev/hugepages  hugetlbfs rw,relatime,pagesize=1G 0 0"
    if [ `grep /etc/fstab -e 'hugetlbfs' | wc -l` -lt 1  ] ; then
        echo_action "ADD HUGEPAGETLB TO FSTAB"
        sudo sh -c "echo \"$HUGEPAGE_TLB_FSTAB\" >> /etc/fstab"
    fi

}

################## CONSTANTS ##################

COLOR='\033[1;36m'
COLOR_WARN='\033[1;35m'
NC='\033[0m'

DPDK_VERSION=19.05
OVS_VERSION=2.11.1

################## VARIABLES ##################

DIR_BASE=
DIR_SRC=
DIR_BUILD=
DIR_INSTALL=

cmd=i
args=
default=n

################## MAIN BODY ##################

parse_parameters $@

DIR_SCRIPT="$( cd "$(dirname "$0")" ; pwd -P )"
DIR_INSTALL_DATA=$DIR_SCRIPT/installdata
DIR_TESTAPP=$DIR_SCRIPT/programs/testapp
DIR_CPUMASK=$DIR_SCRIPT/programs/cpu_list_to_mask
#DIR_INSTALL_DATA=$HOME/repo/installdata

if [ $default == y ] ; then
    DIR_BASE=$HOME/testbed
else
    DIR_BASE=$(asklocation "Select where the testbed should be installed" $HOME/testbed)
fi
DIR_BASE=$(realpath $DIR_BASE)
DIR_SRC=$DIR_BASE/src
DIR_BUILD=$DIR_BASE/build
DIR_INSTALL=$DIR_BASE/install
DIR_CONFIG=$DIR_BASE/conf

DIR_BUILDCORE=$DIR_SRC/buildccore
DIR_BUILDCORE_BUILD=$DIR_BUILD/buildcore
DIR_ROOTFS=$DIR_INSTALL/rootfs

DIR_DPDK_SRC=$DIR_SRC/dpdk-${DPDK_VERSION}
DIR_DPDK_BUILD=$DIR_BUILD/dpdk-${DPDK_VERSION}
DIR_DPDK_INSTALL=$DIR_INSTALL/dpdk-${DPDK_VERSION}

DIR_OVS_SRC=$DIR_SRC/ovs
DIR_SNABB_SRC=$DIR_SRC/snabb

ROOTFS_CHECK_FILE=$DIR_INSTALL/.rootfs-installed
DPDK_CHECK_FILE=$DIR_INSTALL/.dpdk-${DPDK_VERSION}-installed
OVS_CHECK_FILE=$DIR_INSTALL/.ovs-${OVS_VERSION}-installed
SNABB_CHECK_FILE=$DIR_INSTALL/.snabb-installed

SNABB_PROGRAM_NAME=custom_switch
DIR_SNABB_PROGRAM=$DIR_SNABB_SRC/src/program/$SNABB_PROGRAM_NAME
SNABB_INPUT_FILE=$DIR_INSTALL_DATA/$SNABB_PROGRAM_NAME.lua
SNABB_PROGRAM_FILE=$DIR_SNABB_PROGRAM/$SNABB_PROGRAM_NAME.lua

# Configuration files and run commands
CONF_ROOTFS_RC=$DIR_CONFIG/rootfsrc.conf
CONF_DPDK_RC=$DIR_CONFIG/dpdkrc.conf
CONF_VPP=$DIR_CONFIG/vpp.conf
CONF_SRIOV_DEVLIST=$DIR_CONFIG/sriov_devlist.conf
CONF_SRIOV_TESTPMD=$DIR_CONFIG/sriov_testpmd.conf

# Get number of threads for make command
MAKECPUS=`get_make_cpus`

case $cmd in
i)
    # INSTALL COMMAND
    directories_install
    packages_install

    [[ "$args" == *_rootfs_*    ]]  && rootfs_install
    [[ "$args" == *_lxc_*       ]]  && lxc_install
    [[ "$args" == *_dpdk_*      ]]  && dpdk_install
    [[ "$args" == *_ovs_*       ]]  && ovs_install
    [[ "$args" == *_vpp_*       ]]  && vpp_install
    [[ "$args" == *_snabb_*     ]]  && snabb_install
    [[ "$args" == *_app_*       ]]  && testapp_install

    finalize_install

    ;;
d)
    # REMOVE COMMANDS
    # NOTICE: ORDER MATTERS FOR SOME OF THEM

    [[ "$args" == *_ovs_*       ]]  && ovs_remove
    [[ "$args" == *_dpdk_*      ]]  && dpdk_remove
    [[ "$args" == *_vpp_*       ]]  && vpp_remove
    [[ "$args" == *_snabb_*     ]]  && snabb_remove
    # [[ "$args" == *_app_*       ]]  && testapp_remove
    [[ "$args" == *_lxc_*       ]]  && lxc_remove
    [[ "$args" == *_rootfs_* ]]  && rootfs_remove
    [[ "$args" == *_all_*       ]]  && directories_remove

    ;;
esac

exit 0
