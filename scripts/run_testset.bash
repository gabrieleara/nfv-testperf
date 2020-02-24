#!/bin/bash

set -e

################################################################################
#                                  UTILITIES                                   #
################################################################################

function toshortopt() {
    case $1 in
    --help)                 echo "-h"       ;;
    --dimension)            echo "-d"       ;;
    --timeout)              echo "-t"       ;;
    --out-dir)              echo "-o"       ;;
    --vswitch-list)         echo "-v"       ;;
    --rate-params)          echo "-r"       ;;
    --burst-size-params)    echo "-b"       ;;
    --pkt-size-params)      echo "-p"       ;;
    --consume-data)         echo "-c"       ;;
    --local-conf)           echo "-f"       ;;
    --remote-conf)          echo "-F"       ;;
    --remote-hostname)      echo "-H"       ;;
    --test-param)           echo "-T"       ;;
    --debug)                echo "-D"       ;;
    --dry-run)              echo "-X"       ;;
    --schedule)             echo "-S"       ;;
    *)                      echo "$1"       ;;
    esac
}

function tolongopt() {
    case $1 in
    -h) echo "--help"               ;;
    -d) echo "--dimension"          ;;
    -t) echo "--timeout"            ;;
    -o) echo "--out-dir"            ;;
    -v) echo "--vswitch-list"       ;;
    -r) echo "--rate-params"        ;;
    -b) echo "--burst-size-params"  ;;
    -p) echo "--pkt-size-params"    ;;
    -c) echo "--consume-data"       ;;
    -f) echo "--local-conf"         ;;
    -F) echo "--remote-conf"        ;;
    -H) echo "--remote-hostname"    ;;
    -T) echo "--test-param"         ;;
    -D) echo "--debug"              ;;
    -X) echo "--dry-run"            ;;
    -S) echo "--schedule"           ;;
    *)  echo "$1"                   ;;
    esac
}

pnoargformat="%-24s %s\n\n"
pargformat="%-24s %s\n                         %s\n\n"

# TODO: UPDATE
function print_help() {
    cmd=$0
    if [ $cmd = "bash" ] ; then
        cmd="$0 $BASH_SOURCE"
    fi

    printf "%s\n" \
        "Performs MULTIPLE tests on virtual switch technologies to exchange data between co-located containers." \
        "Usage: $cmd <args>" \
        "" \
        "All arguments are optional, default values will be used if not provided." \
        "Order does not matter." \
        "" \
        "An argument marked as <list|param> accepts two types of quote-surrounded string:" \
        "    - a <list> in the form '<value>[ <value> ...]'" \
        "             in this case the list of values, separated by single spaces," \
        "             that should be used during the iteration shall be provided;" \
        "    - a set of <param>, in the format '<min>..<step>..<max>'" \
        "             in this case the parameters are used to generate a list of values" \
        "             with the help of \`sep\` command, replacing dots with spaces." \
        "" \
        "Arguments [*=default]:" \
        ""

    printf "$pnoargformat" \
        "-h, --help" "Print this message" \

    printf "$pargformat" \
        "-d, --dimension" "[throughput*|latency]" \
        "The dimension that shall be tested for"

    printf "$pargformat" \
        "-t, --timeout" "<number>[=$default_timeout]" \
        "The number of seconds the test should last for"

    printf "$pargformat" \
        "-o, --out-dir" "<path>[=out]" \
        "The directory in which results should be written"

    printf "$pargformat" \
        "-v, --vswitch-list" "<list>[=$default_vswitchl]" \
        "The virtual switch used to exchange packets"

    printf "$pargformat" \
        "-r, --rate-params" "<list|param>[=$default_rate]" \
        "The number of packets per seconds that should be sent"

    printf "$pargformat" \
        "-b, --burst-size-params" "<list|param>[=$default_bst_size]" \
        "The number of packets that should be grouped in a single burst"

    printf "$pargformat" \
        "-p, --pkt-size-params" "<number>[=$default_pkt_size]" \
        "The size of each packet in bytes"

    printf "$pnoargformat" \
        "-c, --consume-data" "Use this to touch all bytes sent in a packet each time is produced/received"

    printf "$pnoargformat" \
        "-T, --test-param" "Use this to test only if parameters provided are correct"

    printf "$pnoargformat" \
        "-X, --dry-run" "Check parameters and print the commands that would be issued, without performing them"

    printf "%s\n" \
        "NOTICE: DO NOT SOURCE THIS FILE, IT USES exit COMMAND TO RETURN A VALUE TO COMMAND LINE" \
        ""
}

function contains() {
    if  [[ $1 =~ (^|[[:space:]])$2($|[[:space:]]) ]] ; then
        echo 1
    else
        echo 0
    fi
}

function paramstosequence() {
    regex_list='^([0-9]+[ ]*)+$'
    if [[ "$1" =~ $regex_list ]] ; then
        echo "$1"
        return
    fi

    # Else build a list using parameters, which need exactly to be in the
    # following scheme
    regex_params='^([0-9]+)\.\.([0-9]+)\.\.([0-9]+)$'
    if [[ "$1" =~ $regex_params ]] ; then
        echo `seq ${BASH_REMATCH[1]} ${BASH_REMATCH[2]} ${BASH_REMATCH[3]}`
    else
        echo ""
    fi
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
    while getopts ":d:t:o:v:r:b:p:f:F:H:chTDXS:" opt ;
    do
        case ${opt} in
        d)  dimension=$OPTARG           ;;
        o)  outdir=$OPTARG              ;;
        r)  rate_p=$OPTARG              ;;
        b)  bst_size_p=$OPTARG          ;;
        p)  pkt_size_p=$OPTARG          ;;
        v)  vswitchl=$OPTARG            ;;
        t)  timeout=$OPTARG             ;;
        c)  consume_data=1              ;;
        f)  local_conf=$OPTARG          ;;
        F)  remote_conf=$OPTARG         ;;
        H)  remote_hostname=$OPTARG     ;;
        T)  test_params=1               ;;
        D)  debug=1                     ;;
        S)  schedule=$OPTARG            ;;
        X)  dry_run=1                   ;;
        help|h )
            print_help
            exit 0
            ;;
        \? )
            echo "Invalid option: -"$OPTARG""
            exit 1
            ;;
        : )
            echo "Invalid option: -"$OPTARG" (`tolongopt -"$OPTARG"`) requires an argument"
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

    for vsw in $vswitchl ; do
        if [ "`contains \"$vswitch_list\" \"$vsw\"`" = "0" ] ; then
            echo "Invalid virtual switch selection \"$vswitch\", select one from the following list:"
            echo -e "\t$vswitch_list"
            exit 1
        fi
    done

    rate_list=$(paramstosequence "$rate_p")
    if [ "$rate_list" = "" ] ; then
        printf 'Invalid parameter %s (%s) "%s", use --help for more details.\n' "-r" "`tolongopt -r`" "$rate_p"
        exit 1
    fi

    bst_size_list=$(paramstosequence "$bst_size_p")
    if [ "$bst_size_list" = "" ] ; then
        printf 'Invalid parameter %s (%s) "%s", use --help for more details.\n' "-b" "`tolongopt -b`" "$bst_size_p"
        exit 1
    fi

    pkt_size_list=$(paramstosequence "$pkt_size_p")
    if [ "$pkt_size_list" = "" ] ; then
        printf 'Invalid parameter %s (%s) "%s", use --help for more details.\n' "-p" "`tolongopt -p`" "$pkt_size_p"
        exit 1
    fi

    if [ ! -f "$local_conf" ] ; then
        printf 'Invalid parameter %s (%s) "%s", file does not exist.\n' "-f" "`tolongopt -f`" "$local_conf"
        exit 1
    fi

    if [[ "`file -b $local_conf`" != "Bourne-Again shell script, ASCII text executable" ]] ; then
        printf 'Invalid parameter %s (%s) "%s", must be a valid shell script.\n' "-f" "`tolongopt -f`" "$local_conf"
        exit 1
    fi

    # If a remote hostname is supplied, then a remote conf file must be provided too
    # And vice versa

    if [ -n "$remote_hostname" ] && [ ! -n "$remote_conf" ] ; then
        printf 'If a remote hostname is supplied,\nthen a remote conf file must be provided too\nand vice versa!\n'
        exit 1
    fi

    if [ ! -n "$remote_hostname" ] && [ -n "$remote_conf" ] ; then
        printf 'If a remote hostname is supplied,\nthen a remote conf file must be provided too\nand vice versa!\n'
        exit 1
    fi

    if [ -n "$remote_hostname" ] ; then
        # Remote configuration is optional, if it is non zero it must be good

        if [ ! -f "$remote_conf" ] ; then
            printf 'Invalid parameter %s (%s) "%s", file does not exist.\n' "-F" "`tolongopt -F`" "$remote_conf"
            exit 1
        fi

        if [[ "`file -b $remote_conf`" != "Bourne-Again shell script, ASCII text executable" ]] ; then
            printf 'Invalid parameter %s (%s) "%s", must be a valid shell script.\n' "-F" "`tolongopt -F`" "$remote_conf"
            exit 1
        fi

    fi
}

function linesep() {
    printf '_%.0s' {1..80}
    printf '\n'
}

function numwords() {
    wc -w <<< "$@"
}

function estimate_testnum() {
    num_vswitches=`numwords $vswitchl`
    num_rates=`numwords $rate_list`
    num_bursts=`numwords $bst_size_list`
    num_sizes=`numwords $pkt_size_list`

    echo $(( $num_vswitches * $num_rates * $num_bursts * $num_sizes ))
}

function time_to_termination {
    # echo $(($1 * 3.5))
    echo "scale=0; $1 * 2.8 * $2 / 60" | bc
}

function displaytime {
  local T=$1
  local D=$((T/60/24))
  local H=$((T/60%24))
  local M=$((T%60))

  printf '%02d:%02d:%02d' $D $H $M
}

function print_configuration() {
    confformat="%25s %-11s %-10s\n"

    if [ "$consume_data" = "1" ] ; then
        will_consume=true
    else
        will_consume=false
    fi

    printf '\n'
    linesep
    printf "MULTIPLE TEST CONFIGURATION:\n"
    printf "$confformat" "Test dimension"           ""          "$dimension"
    printf "$confformat" "Output base directory"    ""          "$outdir"
    printf "$confformat" "vSwitch list"             ""          "$vswitchl"
    printf "$confformat" "Timeout"                  "[s]"       "$timeout"
    printf "$confformat" "Rates"                    "[pps]"     "$rate_list"
    printf "$confformat" "Burst sizes"              "[packets]" "$bst_size_list"
    printf "$confformat" "Packet sizes"             "[bytes]"   "$pkt_size_list"
    printf "$confformat" "Each byte will be touched" ""         "$will_consume"

    printf '\n'
    printf "$confformat" "Local configuration file" ""          "$local_conf"
    printf '\n'

    if [ -n "$remote_hostname" ] ; then
        printf "$confformat" "Remote hostname"              ""  "$remote_hostname"
        printf "$confformat" "Remote configuration file"    ""  "$remote_conf"
        printf '\n'
    fi

    estimation=`estimate_testnum`
    rem_time=`time_to_termination $estimation $timeout`
    disp_time=`displaytime $rem_time`

    printf "$confformat" "Estimated number of tests"    ""      "$estimation"
    printf "$confformat" "Estimated time"   "[DD:HH:MM]"        "$disp_time"

    linesep
    printf '\n'
}

function print_test_configuration() {
    confformat="%25s %-11s %-10s\n"

    if [ "$consume_data" = "1" ] ; then
        will_consume=true
    else
        will_consume=false
    fi

    printf '\n'
    linesep
    printf "EXECUTING TEST WITH CONFIGURATION:\n"
    printf "$confformat" "Test dimension"           ""          "$dimension"
    printf "$confformat" "Output directory"         ""          "$curr_outdir"
    printf "$confformat" "vSwitch list"             ""          "$vswitch"
    printf "$confformat" "Timeout"                  "[s]"       "$timeout"
    printf "$confformat" "Rates"                    "[pps]"     "$rate"
    printf "$confformat" "Burst sizes"              "[packets]" "$bst_size"
    printf "$confformat" "Packet sizes"             "[bytes]"   "$pkt_size"
    printf "$confformat" "Each byte will be touched" ""         "$will_consume"
    printf '\n'

    rem_time=`time_to_termination $((remaining_tests+1)) $timeout`
    disp_time=`displaytime $rem_time`

    printf "$confformat" "Number of tests remaining"    ""      "$remaining_tests"
    printf "$confformat" "Estimated time"   "[DD:HH:MM]"        "$disp_time"

    linesep
    printf '\n'
}

function generatestatsfiles() {
    case $dimension in
    throughput)
        grep $curr_outdir/send.log -e "^Tx-pps\:\s\+" | tail -25 | head -20 | awk '{ print $2 " " $3 " " $4; }' > $curr_outdir/send_rates.dat
        grep $curr_outdir/recv.log -e "^Rx-pps\:\s\+" | tail -25 | head -20 | awk '{ print $2; }'               > $curr_outdir/recv_rates.dat
        ;;
    latency)
        # TODO:
        ;;
    esac
}

################################################################################
#                              CONSTANTS AND DIRS                              #
################################################################################

vswitch_list="pmd vpp ovs basicfwd snabb sriov linux-bridge"
dimension_list="throughput latency"

SCREEN_LOCAL_NAME="screen_local"
SCREEN_REMOTE_NAME="screen_remote"

SCRIPT_DIR=`realpath $(dirname $0)`
RUN_LOCAL_TEST="$SCRIPT_DIR/run_local_test.bash"
RUN_REMOTE_TEST="$SCRIPT_DIR/run_remote_test.bash"


################################################################################
#                              DEFAULT PARAMETERS                              #
################################################################################

default_timeout=60
default_vswitchl="pmd"
default_rate="1000000..1000000..10000000"
default_bst_size="16 32 64"
default_pkt_size="`echo $(seq 64 64 512)` 1024 1500"
default_local_conf="confrc.bash"
default_remote_conf=""
default_remote_hostname=""
default_schedule=0

################################################################################
#                                  VARIABLES                                   #
################################################################################

dimension=throughput
outdir=`realpath $(pwd)/out`
timeout="$default_timeout"
rate_p="$default_rate"
bst_size_p="$default_bst_size"
pkt_size_p="$default_pkt_size"
vswitchl="$default_vswitchl"
local_conf="$default_local_conf"
remote_conf="$default_remote_conf"
remote_hostname="$default_remote_hostname"
schedule=$default_schedule
consume_data=0
test_params=0
dry_run=0
debug=0

################################################################################
#                               MAIN SCRIPT FLOW                               #
################################################################################

parse_parameters "$@"
check_parameters
print_configuration

if [ "$test_params" = "1" ] ; then
    printf "Parameters were correct. Terminating without performing tests.\n"
    linesep
    printf '\n'
    exit 0;
fi

outdir=$outdir/$direction

printf "               Test begin time `date`\n"
linesep
printf '\n'


function wait_screen_indefinitely() {
    while (screen -list | grep -q $1) ; do
        sleep 1
    done
}

remaining_tests=$((`estimate_testnum`-1))

for vswitch in $vswitchl ; do
	for bst_size in $bst_size_list ; do
        for pkt_size in $pkt_size_list ; do
            for rate in $rate_list ; do
                curr_outdir="${outdir}/${bst_size}/${vswitch}/${pkt_size}/${rate}"

                local_log="${curr_outdir}/local.log"
                remote_log="${curr_outdir}/remote.log"

                cmd_local="screen -d -S ${SCREEN_LOCAL_NAME} -L -Logfile ${local_log} -m ${RUN_LOCAL_TEST} -d $dimension -v $vswitch -r $rate -p $pkt_size -b $bst_size -o $curr_outdir -t $timeout -S $schedule -f $local_conf "
                cmd_remote="screen -d -S ${SCREEN_REMOTE_NAME} -L -Logfile ${remote_log} -m ${RUN_REMOTE_TEST} -d $dimension -v $vswitch -r $rate -p $pkt_size -b $bst_size -o $curr_outdir -t $timeout -S $schedule -f $remote_conf -H $remote_hostname "

                if [ "$debug" = "1" ] ; then
                    cmd_local="$cmd_local -D"
                    cmd_remote="$cmd_remote -D"
                fi

                if [ "$consume_data" = "1" ] ; then
                    cmd_local="$cmd_local -c"
                    cmd_remote="$cmd_remote -c"
                fi

                print_test_configuration

                if [ "$dry_run" = "0" ] ; then
                    mkdir -p $curr_outdir
                    rm -rf $curr_outdir/*

                    # Run the commands
                    $cmd_local
                    if [ -n "$remote_hostname" ] ; then
                        $cmd_remote
                    fi

                    wait_screen_indefinitely $SCREEN_LOCAL_NAME
                    wait_screen_indefinitely $SCREEN_REMOTE_NAME

                    # Calculate stats files
                    # TODO: GENERATE STATS FILES
                    generatestatsfiles
                else
                    # Print which command would be executed
                    printf " + %s\n" "$cmd_local"
                    if [ -n "$remote_hostname" ] ; then
                        printf " + %s\n" "$cmd_remote"
                    fi

                    printf '\n'
                fi

                remaining_tests=$(($remaining_tests-1))
            done
        done
    done
done

linesep
printf '\n'
printf "               Test end time `date`\n"

linesep
printf '\n'
printf "All tests are done! Check the directories within $outdir !\n"
linesep
printf '\n'
exit 0
