import socket
import time

IP = '192.168.0.237'
PORT = 6969
FRAME_SIZE = 921600 + 614400


client = socket.socket()
client.connect((IP, PORT))

total = 0
start = time.time()
for _ in range(1000):
    val = len(client.recv(1000000))
    #print(val)
    total += val
    #print("total:", total)
    #input('> ')

time_passed = time.time() - start
print(f"speed: {total/time_passed/FRAME_SIZE} frames per second")