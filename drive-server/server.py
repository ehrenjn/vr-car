import RPi.GPIO as GPIO
import socket
import re


IP = '0.0.0.0'
PORT = 55555

GPIO.setmode(GPIO.BOARD) # use BOARD pin numbering so that we can switch out pis and the same pins work (hopefully)



class Wheel:

    def __init__(self, forward_pin, backward_pin, pwm_pin):
        self.forward_pin = forward_pin
        self.backward_pin = backward_pin
        self.pwm_pin = pwm_pin

        GPIO.setup(self.forward_pin, GPIO.OUT)
        GPIO.setup(self.backward_pin, GPIO.OUT)
        GPIO.setup(self.pwm_pin, GPIO.OUT)
        self.pwm = GPIO.PWM(self.pwm_pin, 500) # start pwm at 500hz
        self.pwm.start(0) # start unmoving


    def move(self, speed):
        # set both pins to 0 because idk what happens if they're both high at once
        GPIO.output(self.forward_pin, GPIO.LOW)
        GPIO.output(self.backward_pin, GPIO.LOW)

        forward_pin_value = GPIO.HIGH if speed > 0 else GPIO.LOW
        backward_pin_value = GPIO.HIGH if speed < 0 else GPIO.LOW
        GPIO.output(self.forward_pin, forward_pin_value)
        GPIO.output(self.backward_pin, backward_pin_value)
        self.pwm.ChangeDutyCycle(abs(speed))


    def forward(self, speed):
        self.move(speed)

    def backward(self, speed):
        self.move(-speed)



class TCPServer:

    MAX_MESSAGE_LENGTH = 100
    HEARTBEAT_INTERVAL = 2

    def __init__(self, ip, port, left_wheel, right_wheel):
        self.left_wheel = left_wheel
        self.right_wheel = right_wheel
        self.socket = socket.socket()
        self.socket.bind((ip, port))
    

    def serve_until_shutdown(self):
        serving = True
        while serving:
            self.socket.listen()
            print("listening for client...")
            client, _ = self.socket.accept()
            print("connected to client")
            client.settimeout(TCPServer.HEARTBEAT_INTERVAL)
            serving = self._serve(client)
            print("client disconnected")
        print("server shutting down")


    def _serve(self, client):
        while True:
            try:
                message = client.recv(TCPServer.MAX_MESSAGE_LENGTH)
                if message == b'': # an empty string means the client has disconnected
                    return True 
                self._handle_message(message)
            except socket.timeout:
                print("stopping car due to timeout")
                self.left_wheel.move(0)
                self.right_wheel.move(0) 
            except ConnectionResetError: # this also means a client has disconnected
                return True


    def _handle_message(self, message):
        command = re.search(b"^(?P<left>-?\d+),(?P<right>-?\d+)$", message)
        if command is not None:
            left_speed = int(command.group('left'))
            right_speed = int(command.group('right'))
            print("received command - left:", left_speed, ", right:", right_speed)
            self.left_wheel.move(left_speed)
            self.right_wheel.move(right_speed)



if __name__ == "__main__":
    left = Wheel(24, 26, 32)
    right = Wheel(18, 22, 12)
    server = TCPServer(IP, PORT, left, right)
    server.serve_until_shutdown()


GPIO.cleanup()
