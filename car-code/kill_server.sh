pid=$(
    ps -aux | 
    grep "$1" | # find all processes with a certain name
    grep -v grep | # remove pid of this current grep command from the list of found processes
    grep -v kill_server.sh | # remove pid of this current script
    awk '{print $2}' # extract just the 2nd column (pid) of every process found
)

if [ -z "${pid}" ]; then # if pid is an empty string
    echo "no PID for '$1' found"
elif [[ ${pid} = *[[:space:]]* ]]; then # if there are multiple pids
    echo "multiple possible processes found, not sure which one to kill"
else
    kill -9 ${pid}
    echo "killed successfully"
fi