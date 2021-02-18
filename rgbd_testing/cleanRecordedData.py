import os
from PIL import Image
import png
import numpy as np
import struct
import cv2

raw_rgb_path = "C:\\Python27\\programs\\RGB3"
raw_depth_path = "C:\\Python27\\programs\\FIXED_DEPTH3"

new_rgb_path = "C:\\Python27\\programs\\rgb_data"
new_depth_path = "C:\\Python27\\programs\\depth_data"

raw_rgbs = iter(os.listdir(raw_rgb_path))
raw_depths = enumerate(os.listdir(raw_depth_path))

rgb = raw_rgbs.next()
rgb_date = float(rgb.split("__")[1][:-4])
last_rgb = rgb
last_rgb_date = rgb_date

use480 = True
frames = 0
try:
    while True:
 
        frame, depth = raw_depths.next()
        depth_date = float(depth.split("__")[1][:-4])

        while rgb_date < depth_date:
            last_rgb = rgb
            last_rgb_date = rgb_date
            rgb = raw_rgbs.next()
            rgb_date = float(rgb.split("__")[1][:-4])

        im = cv2.imread(raw_depth_path + "\\" + depth, cv2.IMREAD_UNCHANGED)
        if use480:
            im = cv2.resize(im,(640,480), interpolation=cv2.INTER_NEAREST)
        cv2.imwrite(new_depth_path + "\\" + str(frame) + ".png", im)

        chosen_rgb = rgb
        if (rgb_date - depth_date) > (depth_date - last_rgb_date):
            chosen_rgb = last_rgb
        im = Image.open(raw_rgb_path + "\\" + chosen_rgb)
        if not use480:
            im.thumbnail((320,240), Image.ANTIALIAS)
        im.save(new_rgb_path + "\\" + str(frame) + ".png")
        frames += 1
          
except Exception as e:
    print (e)
    print "GOT TO FRAME", frames
