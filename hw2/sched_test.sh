#!/bin/bash

if [ "$EUID" -ne 0 ] ; then
    echo "Please run as root"
    exit 1
fi

# You can add your own test case here
testcase=(  " -n 1 -t 0.5 -s NORMAL -p -1"
            " -n 2 -t 0.5 -s FIFO,FIFO -p 10,20"
            " -n 3 -t 1.0 -s NORMAL,FIFO,FIFO -p -1,10,30")

demo=$1
test=$2

# record original value
original=`cat /proc/sys/kernel/sched_rt_runtime_us`

# set the test condition
sudo sysctl -w kernel.sched_rt_runtime_us=1000000 > /dev/null

case_count=${#testcase[@]}
for (( j = 0; j < $case_count; j += 1 ));
do
    case=${testcase[$j]}
    echo "Running testcase" $j ":" $demo $case
    diff_out=`diff <($demo $case) <($test $case)`
    if [ $? -eq 0 ] ; then
        echo "Result: Success!"
    else
        echo "$diff_out"
        echo "Result: Failed..."
        break
    fi
done

# revert the test condition
sudo sysctl -w kernel.sched_rt_runtime_us=$original > /dev/null
