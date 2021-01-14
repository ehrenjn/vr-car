import socket
import re

DISABLE_GPIO = False # change this to True force disable GPIO

if not DISABLE_GPIO:
    try:
        import RPi.GPIO as GPIO
    except ImportError:
        print("WARNING: RPi.GPIO COULD NOT BE IMPORTED, disabling GPIO")
        DISABLE_GPIO = True
    else:
        GPIO.setmode(GPIO.BOARD) # use BOARD pin numbering so that we can switch out pis and the same pins work (hopefully)
else:
    print("GPIO is disabled because DISABLE_GPIO is set to true")

IP = '0.0.0.0'
PORT = 55555



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



class WheelMock(Wheel):

    def __init__(*args): pass

    def move(self, speed):
        print("wheel changing to {} speed".format(speed))



class StreamLineParser:

    def __init__(self):
        self._data = []


    def get_lines(self, message):
        self._data.extend(message)
        num_lines = message.count(b'\n')
        line_start = 0

        for _ in range(num_lines):
            line_end = self._data.index(ord('\n'), line_start) # ord because data is stored as ints now and not byte strings, also start searching at line_start
            line = self._data[line_start:line_end]
            yield bytes(line)
            line_start = line_end + 1

        if line_start != 0:
            self._data = self._data[line_start:]



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
            self._stop_car() # make sure car has stopped when client disconnects
        print("server shutting down")


    def _serve(self, client):
        message_splitter = StreamLineParser()
        while True:
            try:
                message = client.recv(TCPServer.MAX_MESSAGE_LENGTH)
            except socket.timeout:
                print("stopping car due to timeout")
                self._stop_car()
            except ConnectionResetError: # this means a client has disconnected
                return True
            else: # recv was successful
                if message == b'': # an empty string also means the client has disconnected
                    return True 
                else:
                    for line in message_splitter.get_lines(message):
                        self._handle_message(line)


    def _handle_message(self, message):
        command = re.search(b"^(?P<left>-?\d+),(?P<right>-?\d+)$", message)
        if command is not None:
            left_speed = int(command.group('left'))
            right_speed = int(command.group('right'))
            print("received command - left:", left_speed, ", right:", right_speed)
            self.left_wheel.move(left_speed)
            self.right_wheel.move(right_speed)
        else:
            print("got non command message: ", message)
    

    def _stop_car(self):
        self.left_wheel.move(0)
        self.right_wheel.move(0) 



if __name__ == "__main__":
    make_wheel = WheelMock if DISABLE_GPIO else Wheel
    left = make_wheel(24, 26, 32)
    right = make_wheel(18, 22, 12)
    server = TCPServer(IP, PORT, left, right)
    server.serve_until_shutdown()


if not DISABLE_GPIO:
    GPIO.cleanup()
