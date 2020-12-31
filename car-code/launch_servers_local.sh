vr_car_home="~/vr-car/"

# kill servers first
cd "${vr_car_home}/car-code/vision-server" && make kill
/bin/bash ${vr_car_home}/car-code/kill_server.sh "python3 server.py"

# then run servers
cd "${vr_car_home}/car-code/drive-server" && python3 ./server.py &
cd "${vr_car_home}/car-code/vision-server" && ./vision_server &