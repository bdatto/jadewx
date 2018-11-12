#! /bin/bash
#
pids=`ps -u wx -o pid,args |grep jadewx |grep -v "grep" |awk '{print $1}'`
for pid in ${pids[@]}; do
  kill -9 $pid
done
