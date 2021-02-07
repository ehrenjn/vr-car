# for testing what happens when we lower FPS of the camera

import os

LEFT = "./rtabmap_source/NoEventsExample/stereo_20Hz/left/"
RIGHT = "./rtabmap_source/NoEventsExample/stereo_20Hz/right/"
NEW_RIGHT = "./rtabmap_source/NoEventsExample/stereo_20Hz/right_NEW/"
NEW_LEFT = "./rtabmap_source/NoEventsExample/stereo_20Hz/left_NEW/"

KEEP_EVERY = 10


num_jpgs = len(os.listdir(LEFT))
new_jpg_num = 1

for jpg_num in range(1, num_jpgs + 1):
    if jpg_num % KEEP_EVERY == 0:
        jpg_title = f"{jpg_num}.jpg"
        print(jpg_title)
        new_jpg_title = f"{new_jpg_num}.jpg" 
        with open(LEFT + jpg_title, 'rb') as left, open(RIGHT + jpg_title, 'rb') as right:
            with open(NEW_LEFT + new_jpg_title, 'wb') as new_left, \
            open(NEW_RIGHT + new_jpg_title, 'wb') as new_right:
                new_left.write(left.read())
                new_right.write(right.read())
        new_jpg_num += 1