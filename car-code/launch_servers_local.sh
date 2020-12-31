vr_car_home="~/vr-car/"

# kill servers first
echo "killing running servers"
cd "${vr_car_home}/car-code/vision-server" && make kill
/bin/bash ${vr_car_home}/car-code/kill_server.sh "python3 server.py"

# then run servers
echo "restarting servers"
cd "${vr_car_home}/car-code/drive-server" && python3 ./server.py &
cd "${vr_car_home}/car-code/vision-server" && ./vision_server &