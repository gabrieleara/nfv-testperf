#!/bin/bash

set -e

################################################################################
#                               VARIABLES
################################################################################

##TODO: READ THESE VALUES FROM A FILE
remote="remote" # -H
confpath="confrc_remote.bash" # -f
remote_confpath="/tmp/confrc.bash" # leave default
remote_outdir="/tmp/tmpout" # leave default
outdir="out/remote" # -o
cmddir="/home/gara/repo/scripts/" # -C

################################################################################
#                               MAIN FLOW
################################################################################

while getopts ":d:t:f:o:v:r:b:p:H:C:chTDBRS:" opt ;
do
    case ${opt} in
    d)  ;;
    o)  outdir=$OPTARG     ;;
    r)  ;;
    b)  ;;
    p)  ;;
    v)  ;;
    t)  ;;
    c)  ;;
    T)  ;;
    D)  ;;
    R)  ;;
    B)  ;;
    H)  remote=$OPTARG      ;;
    f)  confpath=$OPTARG    ;;
    C)  cmddir=$OPTARG      ;;
    S)  ;;
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

command="${cmddir}/run_local_test.bash $@ -f $remote_confpath -o $remote_outdir"

outdir=$outdir/remote

rm -R $outdir 2>/dev/null || true
ssh $remote "rm -r ${remote_outdir} 2>/dev/null || true"
scp $confpath ${remote}:${remote_confpath}
ssh $remote $command
mkdir -p ${outdir}
scp -rp $remote:${remote_outdir}/* ${outdir}
