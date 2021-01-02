vr_car_home="${HOME}/vr-car/" # have to use $HOME instead of ~ because it won't expand properly otherwise

# kill servers first
echo "killing running servers"
cd "${vr_car_home}/car-code/vision-server/src" && make kill
/bin/bash ${vr_car_home}/car-code/kill_server.sh "server.py"

# then run servers
echo "restarting drive server"
cd "${vr_car_home}/car-code/drive-server" && python3 ./server.py &
echo "restarting vision server"
cd "${vr_car_home}/car-code/vision-server/src" && ./vision_server &

sleep 2 # wait a bit to see some output