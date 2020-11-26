import RPi.GPIO as GPIO

GPIO.setmode(GPIO.BOARD) # use BOARD pin numbering so that we can switch out pis and the same pins work (hopefully)

# pin 12 and 13 are mirrored (they always have the same output) and are 2 of the only 4 pwm pins, so they will be used for output
GPIO.setup(12, GPIO.OUT)
GPIO.setup(33, GPIO.OUT)

pwm = GPIO.PWM(12, 500) # start pwm at 500hz


pwm.start(0)
duty = 0
while duty <= 100:
    pwm.ChangeDutyCycle(duty)
    duty = int(input("new duty cycle (0 - 100): "))

pwm.stop()
GPIO.cleanup()