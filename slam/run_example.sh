#!/bin/sh

# start ROS
source "/opt/ros/$ROS_DISTRO/setup.bash"

# tell noEventsExample how to talk to X server
export DISPLAY="${1}:0.0"

cd /root/poc/NoEventsExample \
    && cmake . \
    && make \
    && ./noEventsExample 20 2 10 stereo_20Hz stereo_20Hz stereo_20Hz/left stereo_20Hz/right