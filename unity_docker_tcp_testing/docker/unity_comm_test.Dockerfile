# docker build -t communicator_test -f communicator_test.Dockerfile .
# docker run --volume <absolute path to vr-car/slam/communicator/>:/root/test communicator_test
# docker run --volume C:\Users\Monchy\Documents\GitHub\vr-car\slam\communicator:/root/test communicator_test
FROM introlab3it/rtabmap:latest



# run example as soon as container starts
#CMD ["sleep", "10000"]
ENTRYPOINT ["/root/test/run_test.sh"]