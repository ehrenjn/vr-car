#! /bin/bash


set -e # exit on failure 

vision_server_folder="~/vr-car/car-code/vision-server/"


if [ "${2}" == "run" ]; then
    echo -e "\nexecuting..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "cd ${vision_server_folder}/src/ && make run"

elif [ "${2}" == "kill" ]; then
    echo -e "\nkilling server..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "cd ${vision_server_folder}/src/ && make kill"

else
    echo -e "\nuploading..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "rm -rf ${vision_server_folder} && mkdir ${vision_server_folder}"
    scp -i ~/.ssh/pi_vr_car_key -r "$(dirname $0)/src" pi@${1}:${vision_server_folder}/src

    echo -e "\ncompiling..."
    ssh -i ~/.ssh/pi_vr_car_key pi@${1} "cd ${vision_server_folder}/src/ && make"

fi