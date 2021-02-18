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