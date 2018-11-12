#! /bin/bash
#
tmux has-session -t jadewx
status=$?
if [ $status != 0 ]; then
  /home/wx/wxstart.sh
fi
