
# get camera data from https://drive.google.com/file/d/1bQfJomAxhR8xOkl1HphyxP41ZjE7gS_w/view?usp=sharing
# put the recordings folder in rtabmap_source\RGBDMapping

# NOTES: 
    # right now to choose 240p or 480p you have to modify line in this file to be `&& git checkout 640x480` or `&& git checkout 320x240 \` which is nasty
	# line ADD "https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h" skipcache used to force custom rtabmap to always rebuild since caching was always causing me issues

# pre build:
    # have to get camera data first from above link
# building: `docker build -t rgbd_mapping -f rgbd_mapping.Dockerfile .`
# pre run:
    # get xserver so the gui works
        # download vcxsrc on windows (https://sourceforge.net/projects/vcxsrv/)
        # find xlaunch.exe in program files and run it
            # choose all default settings except for "disable access control" (you want it on so any shmuck can connect)
    # find your docker IP 
        # do ipconfig and then look under "Ethernet adapter VirtualBox Host-Only Network", "Ethernet adapter vEthernet (WSL)", etc.
# to run: `docker run --volume <absolute path to vr-car/rgbd_testing/rtabmap_source/>:/root/rgbd_mapping rgbd_mapping <your docker ip here>`
    # make sure you use the *absolute path* or it won't work
    # if your absolute path has spaces in it, put the whole path in quotes
    # even though the absolute path is on windows, you should use forward slashes
# to enter container while running: `docker exec -it {id} bash`
# also remember to docker prune both containers and volumes when you're done with em



FROM introlab3it/rtabmap:xenial

RUN git clone https://github.com/opencv/opencv opencv \
	&& cd opencv \
	&& mkdir build \
	&& cd build \
	&& cmake -DCMAKE_BUILD_TYPE=Release .. \
	&& make -j4 \
	&& sudo make install 

ADD "https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h" skipcache

RUN sudo apt-get install libsqlite3-dev libpcl-dev git cmake libproj-dev libqt5svg5-dev \
	&& git clone https://github.com/TrevorBivi/rtabmap.git rtabmap \
	&& rm -rf rtabmap/examples/RGBDMapping \
	&& cd rtabmap \
	&& git checkout 640x480 \
	&& cd build \
	&& cmake .. \
	&& make -j4 \
	&& make install \
	&& ldconfig
	
ENTRYPOINT ["/root/rgbd_mapping/RGBDMapping/run_example.sh"]