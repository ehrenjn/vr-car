import RPi.GPIO as GPIO

GPIO.setmode(GPIO.BOARD) # use BOARD pin numbering so that we can switch out pis and the same pins work (hopefully)

# pin 12 and 32 are mirrored (they always have the same output) and are 2 of the only 4 pwm pins, so they will be used for output
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
        self.forward_pin(0)
        self.backward_pin(0)

        self.forward_pin(1 if speed > 0 else 0)
        self.backward_pin(1 if speed < 0 else 0)
        self.pwm.ChangeDutyCycle(abs(speed))


    def forward(self, speed):
        self.move(speed)

    def backward(self, speed):
        self.move(-speed)



left = Wheel(24, 26, 32)
right = Wheel(18, 22, 12)

duty = 0
while duty <= 100:
    left.move(duty)
    right.move(duty)
    duty = int(input("new duty cycle (0 - 100): "))



GPIO.cleanup()