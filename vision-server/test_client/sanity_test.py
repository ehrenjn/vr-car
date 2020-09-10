import socket

IP = '192.168.0.237'
PORT = 6969

client = socket.socket()
client.connect((IP, PORT))