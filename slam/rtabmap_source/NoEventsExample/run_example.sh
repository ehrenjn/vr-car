#!/bin/sh

# start ROS
source "/opt/ros/$ROS_DISTRO/setup.bash"

# tell noEventsExample how to talk to X server
export DISPLAY="${1}:0.0"

cd /root/no_events/NoEventsExample \
    && cmake . \
    && make \
    && ./noEventsExample 2 1 2 stereo_20Hz stereo_20Hz stereo_20Hz/left_NEW stereo_20Hz/right_NEW
    # args: camera_rate (fps) odom_update (frames per update) map_update (hz) calibration_dir calibration_name path_left_images path_right_images
    # default values: 20 2 10
    # good values for 2 fps data: 2 1 2