
import sys

# file location should be in the first arg
filename = sys.argv[1]
cachefilename = sys.argv[2]

# list of possible things that could be done:
# - filter words
# - duplicate specific words to increase their size in the resulting wordcloud
# - attach a timestamp in a temporary side file, delete old words?

# new text
with open(filename, 'r') as f:
    newtext = f.read()

# append new text to cache file
with open(cachefilename, "a") as f:
    f.write(newtext)

# print everything, yolo
with open(cachefilename, "r") as f:
    for line in f:
        print line.rstrip()
