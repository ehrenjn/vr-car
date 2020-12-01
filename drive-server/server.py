import RPi.GPIO as GPIO

GPIO.setmode(GPIO.BOARD) # use BOARD pin numbering so that we can switch out pis and the same pins work (hopefully)

# pin 12 and 32 are mirrored (they always have the same output) and are 2 of the only 4 pwm pins, so they will be used for output
GPIO.setup(12, GPIO.OUT)
GPIO.setup(32, GPIO.OUT)

pwm1 = GPIO.PWM(12, 500) # start pwm at 500hz
pwm2 = GPIO.PWM(32, 500) # start pwm at 500hz


duty = 0
pwm1.start(duty)
pwm2.start(duty)
while duty <= 100:
    pwm1.ChangeDutyCycle(duty)
    pwm2.ChangeDutyCycle(duty)
    duty = int(input("new duty cycle (0 - 100): "))

pwm1.stop()
pwm2.stop()
GPIO.cleanup()
