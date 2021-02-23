#!/bin/sh

# start ROS
source "/opt/ros/$ROS_DISTRO/setup.bash"

# tell noEventsExample how to talk to X server
export DISPLAY="${1}:0.0"

cd /root/no_events/NoEventsExample \
    && cmake . \
    && make \
    && (./noEventsExample 20 2 10 stereo_20Hz stereo_20Hz stereo_20Hz/left stereo_20Hz/right) # change this line to && (./noEventsExample 20 2 10 stereo_20Hz stereo_20Hz stereo_20Hz/left stereo_20Hz/right &) to run the test client
    # args: camera_rate (fps) odom_update (frames per odometry update) map_update (how many odometry updates to wait before doing one map update) calibration_dir calibration_name path_left_images path_right_images
    # default values: 20 2 10
    # good values for 2 fps data: 2 1 2


echo "sleeping before client compilation..."
sleep 20
cd /root/no_events/NoEventsExample \
    && g++ -Wall -Wextra -o no_events_test_client no_events_test_client.cpp communicator/TCPCommunicator.cpp \
    && ./no_events_test_client