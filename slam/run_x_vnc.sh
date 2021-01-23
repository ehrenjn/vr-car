#!/bin/sh

Xvfb :0 -screen 0 1366x768x16 & # start X
x11vnc -display :0 -forever & # run VNC forever using X running on :0


# start ROS
source "/opt/ros/$ROS_DISTRO/setup.bash"

cd /root/rtabmap/examples/NoEventsExample \
    && xvfb-run ./noEventsExample 20 2 10 stereo_20Hz stereo_20Hz stereo_20Hz/left stereo_20Hz/right