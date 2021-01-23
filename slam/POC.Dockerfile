# https://qxf2.com/blog/view-docker-container-display-using-vnc-viewer/

# building: `docker build -t poc -f POC.Dockerfile .`
# running: `docker run -p 5900:5900 poc`
# then connect to localhost:5900 in VNC
# to enter container: `docker exec -it {id} bash`

# NEED TO SOMEHOW RUN EVERYTHING IN ROS... EASIEST WAY TO DO THAT FOR NOW IS TO PUT ALL THE COMMANDS YOU WANT TO RUN INTO /ros_entrypoint.sh IN THE CONTAINER (and get rid of the `exec "$@"` line)

FROM introlab3it/rtabmap:latest

# install X and vnc server for X, as well as git for cloning example
RUN apt update \
    && apt install -y xvfb x11vnc git

# copy script to run X and VNC
ADD run_x_vnc.sh /root/run_x_vnc.sh

# build rtabmap example (source "/opt/ros/$ROS_DISTRO/setup.bash" to run in ROS or it wont work)
RUN cd /root \
    && git clone https://github.com/introlab/rtabmap.git \
    && cd ./rtabmap/examples/NoEventsExample \
    && source "/opt/ros/$ROS_DISTRO/setup.bash" \
    && cmake . \
    && make

# make data available for example
ADD stereo_20Hz /root/rtabmap/examples/NoEventsExample/stereo_20Hz

# expose port used by vnc
EXPOSE 5900

# actually start X and run vnc
ENTRYPOINT ["/root/run_x_vnc.sh"]