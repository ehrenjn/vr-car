# docker build -t unity_comm_test -f unity_comm_test.Dockerfile .
# docker run -p 7787:7787 --volume  <absolute path to vr-car\slam\communicator>:/root/test unity_comm_test
# hit play on unity project soon after (container only runs for 20 seconds)

FROM introlab3it/rtabmap:latest

# run example as soon as container starts
#CMD ["sleep", "10000"]
ENTRYPOINT ["/root/test/run_test.sh"]