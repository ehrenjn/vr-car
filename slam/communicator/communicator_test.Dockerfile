# docker build -t communicator_test -f communicator_test.Dockerfile .
# docker run --volume <absolute path to vr-car/slam/communicator/>:/root/test communicator_test

FROM introlab3it/rtabmap:latest

# run example as soon as container starts
ENTRYPOINT ["/root/test/run_test.sh"]