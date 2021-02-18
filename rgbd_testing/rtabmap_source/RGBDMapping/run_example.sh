#!/bin/sh
echo "SH FOUND"
# start ROS
source "/opt/ros/$ROS_DISTRO/setup.bash"

# tell noEventsExample how to talk to X server
export DISPLAY="${1}:0.0"


cd /root/rgbd_mapping/RGBDMapping \
    && cmake . \
    && make \
    && ./rgbd_mapping 12
    # args: camera_rate (fps) odom_update (frames per odometry update) map_update (how many odometry updates to wait before doing one map update) calibration_dir calibration_name path_left_images path_right_images
    # default values: 20 2 10
    # good values for 2 fps data: 2 1 2