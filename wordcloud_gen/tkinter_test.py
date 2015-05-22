
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
    master.state('zoomed') # maximize the window

    canvas = Canvas(master, width=screensize[0], height=screensize[1])
    canvas.pack()

    img = generateImage()
    putImageOnCanvas(img)
    
    master.after(1000, doUpdate, img)
    master.mainloop()

old_wc = None

def generateImage():
    with open(sys.argv[1]) as f: 
        text = f.read()

    wc = WordCloud(font_path='C:\Windows\Fonts\Verdana.ttf', width=750, height=400).generate(text)
    wc = wc.recolor(color_func=grey_color_func, random_state=3)
    img = wc.to_image()
    
    scale = 0.5 # % of the whole screen
    size = tuple(int(i * scale) for i in screensize)
    img = img.resize(size, Image.ANTIALIAS)
    return img

def putImageOnCanvas(img):
    global image_ref # used to keep python's GC from screwing with our images
    image_ref = ImageTk.PhotoImage(img)
    canvas.create_image((screensize[0]/2, screensize[1]/2), image=image_ref, state="normal")
    
def DoShit():
    print "Actual DoShit call"
    global old_wc
    wc_img = generateImage()
    (x, y) = old_wc
    old_wc = (x, wc_img)
    
    
    putImageOnCanvas(old_wc[0])
    master.after(4000, DoShit2)
    
alpha = 0.0

def doUpdate(oldImg):
    newImg = generateImage()
    
    master.after(1, doImageTransition, (oldImg, newImg), 0.0)

def doImageTransition((oldImg, newImg), alpha):
    img = Image.blend(oldImg, newImg, alpha)
    putImageOnCanvas(img)
    
    if alpha >= 0.99:
        master.after(5000, doUpdate, newImg)
    else:
        master.after(50, doImageTransition, (oldImg, newImg), alpha + 0.05)
    
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
