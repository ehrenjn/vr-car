# https://medium.com/@potatowagon/how-to-use-gui-apps-in-linux-docker-container-from-windows-host-485d3e1c64a3
# get camera data from https://github.com/introlab/rtabmap/wiki/Stereo-mapping#process-a-directory-of-stereo-images\

# IMPROVEMENTS: 
    # iirc theres a way to set your default directory (so you dont need to keep specifying /root/)

# pre build:
    # have to get camera data first from above link and make sure it is in a folder called stereo_20Hz
    # put stero_20Hz folder in vr-car/slam/rtabmap_source/NoEventsExample
# building: `docker build -t poc -f POC.Dockerfile .`
# pre run:
    # get xserver so the gui works
        # download vcxsrc on windows (https://sourceforge.net/projects/vcxsrv/)
        # find xlaunch.exe in program files and run it
            # choose all default settings except for "disable access control" (you want it on so any shmuck can connect)
    # find your docker IP 
        # do ipconfig and then look under "Ethernet adapter VirtualBox Host-Only Network"
# to run: `docker run --volume <absolute path to vr-car/slam/rtabmap_source/>:/root/poc poc <your docker ip here>`
    # make sure you use the *absolute path* or it won't work
    # if your absolute path has spaces in it, put the whole path in quotes
    # even though the absolute path is on windows, you should use forward slashes
# to enter container while running: `docker exec -it {id} bash`
# also remember to docker prune both containers and volumes when you're done with em



FROM introlab3it/rtabmap:latest

# add script to run on startup
ADD run_example.sh /root/run_example.sh

# run example as soon as container starts
ENTRYPOINT ["/root/run_example.sh"]