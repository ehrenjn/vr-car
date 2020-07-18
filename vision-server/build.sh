#! /bin/bash


set -e # exit on failure 


if [ "${2}" == "run" ]; then
    echo -e "\nexecuting..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "cd ~/vr-car/vision-server/src/ && make run"

else
    echo -e "\nuploading..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "rm -rf ~/vr-car/vision-server && mkdir ~/vr-car/vision-server"
    scp -i ~/.ssh/pi_vr_car_key -r "$(dirname $0)/src" pi@${1}:~/vr-car/vision-server/src

    echo -e "\ncompiling..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "cd ~/vr-car/vision-server/src/ && make"

fi