
import random
import ctypes
import math
from Tkinter import *
from wordcloud import WordCloud
import matplotlib.pyplot as plt
import struct
from PIL import Image, ImageTk
import sys

def main():
    global screensize
    global master
    global canvas

    user32 = ctypes.windll.user32
    # (width, height)
    screensize = user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)

    master = Tk()
    master.state('zoomed')

    canvas = Canvas(master, width=screensize[0], height=screensize[1])
    canvas.pack()
            
    master.after(1000, task)
    master.mainloop()

def task():
    global image_ref # used to keep python's GC from screwing with our images
    
    text = open(sys.argv[1]).read()
    wc = WordCloud(font_path='C:\Windows\Fonts\Verdana.ttf', width=750, height=400).generate(text)
    wc = wc.recolor(color_func=grey_color_func, random_state=3)
    wc_img = wc.to_image()
    
    scale = 0.5 # % of the whole screen
    size = tuple(int(i * scale) for i in screensize)
    wc_img = wc_img.resize(size, Image.ANTIALIAS)

    image_ref = ImageTk.PhotoImage(wc_img)
    canvas.create_image((screensize[0]/2, screensize[1]/2), image=image_ref, state="normal")

    master.after(5000, task)

def grey_color_func(word, font_size, position, orientation, random_state=None):
    return "hsl(0, 0%%, %d%%)" % 75

# for testing the canvas
def put_test_squares():
    for i in range(1, 10):
        x = random.random() * screensize[0]
        y = random.random() * screensize[1]
        
        points = []
        points.append((x - 25, y - 25))
        points.append((x + 25, y - 25))
        points.append((x + 25, y + 25))
        points.append((x - 25, y + 25))

        canvas.create_polygon(points, outline='#2A97B5', fill='blue', width=3)
        
if __name__ == "__main__":
    main()
