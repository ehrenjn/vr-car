#!/bin/sh

# start ROS (may as well)
source "/opt/ros/$ROS_DISTRO/setup.bash"

cd /root/test/ \
    && g++ -c -Wall -Wextra -o TCPCommunicator.o TCPCommunicator.cpp \
    && g++ -Wall -Wextra -o server_test server_test.cpp TCPCommunicator.o \
    && g++ -Wall -Wextra -o client_test client_test.cpp TCPCommunicator.o \
    && (./server_test &) \
    && sleep 2 \
    && ./client_test