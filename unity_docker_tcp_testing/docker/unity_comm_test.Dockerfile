# docker build -t unity_comm_test -f unity_comm_test.Dockerfile .

# TO CONNECT TO SERVER ON DOCKER FROM UNITY
# 1. remove `&& ./client_test` line from run_test.sh
# 2. Make sure only a TestTCPClient is enabled on the tcp game object in unity
# 3. docker run -p 7787:7787 --volume  <absolute path to vr-car\slam\communicator>:/root/test unity_comm_test
# 4. hit play on unity project soon after (container only runs for 20 seconds)

# TO CONNECT TO SERVER IN UNITY FROM DOCKER
# 1. remove `(./server_test &) \`, `&& sleep 20 \` lines from run_test.sh (only if you modified it to work as a client)
# 2. Set ip of client_test.cpp to your host computer's IPv4 addres (ex. `uint8_t ip[] = {192,168,1,105};` for me)
# 3. Make sure only a TestTCPServer script is enabled on the tcp game object in unity (should be by default)
# 4. hit play on unity project
# 5. docker run --net=host --volume  <absolute path to vr-car\slam\communicator>:/root/test unity_comm_test

FROM introlab3it/rtabmap:latest

# run example as soon as container starts
#CMD ["sleep", "10000"]
ENTRYPOINT ["/root/test/run_test.sh"]