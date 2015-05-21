
from os import path
from wordcloud import WordCloud
import matplotlib.pyplot as plt
import random
import sys

def grey_color_func(word, font_size, position, orientation, random_state=None):
    return "hsl(0, 0%%, %d%%)" % random.randint(60, 100)

filename = sys.argv[1]

# Read the whole text.
text = open(filename).read()
wc = WordCloud(font_path='C:\Windows\Fonts\Verdana.ttf').generate(text)

# store default colored image
# default_colors = wc.to_array()
# plt.title("Custom colors")
plt.imshow(wc.recolor(color_func=grey_color_func, random_state=3))
plt.axis("off")
# plt.figure()
# plt.title("Default colors")
# plt.imshow(default_colors)
# plt.axis("off")
plt.show() # this halts the script until the user closes the image
