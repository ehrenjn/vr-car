# https://medium.com/@potatowagon/how-to-use-gui-apps-in-linux-docker-container-from-windows-host-485d3e1c64a3
# get camera data from https://github.com/introlab/rtabmap/wiki/Stereo-mapping#process-a-directory-of-stereo-images\

# IMPROVEMENTS: 
    # iirc theres a way to set your default directory (so you dont need to keep specifying /root/)
    # nasty that I ADD such a large directory instead of using a volume

# building: `docker build -t poc -f POC.Dockerfile .`
    # have to get camera data first from above link
# to get working with gui:
    # download vcxsrc on windows (https://sourceforge.net/projects/vcxsrv/)
    # find xlaunch.exe in program files and run it
        # choose all default settings except for "disable access control" (you want it on so any shmuck can connect)
# to run: `docker run poc <your ip here>`
    # to find ip: do ipconfig and then look under "Ethernet adapter VirtualBox Host-Only Network"
# to enter container while running: `docker exec -it {id} bash`



FROM introlab3it/rtabmap:latest

# install git for cloning example
RUN apt update \
    && apt install -y git

# build rtabmap example (source "/opt/ros/$ROS_DISTRO/setup.bash" to run in ROS or it wont work)
RUN cd /root \
    && git clone https://github.com/introlab/rtabmap.git \
    && cd ./rtabmap/examples/NoEventsExample \
    && source "/opt/ros/$ROS_DISTRO/setup.bash" \
    && cmake . \
    && make

# add script to run on startup
ADD run_example.sh /root/run_example.sh

# make data available for example code
ADD stereo_20Hz /root/rtabmap/examples/NoEventsExample/stereo_20Hz

# run example as soon as container starts
ENTRYPOINT ["/root/run_example.sh"]