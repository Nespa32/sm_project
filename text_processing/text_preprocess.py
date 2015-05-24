
import sys

# list of possible things that could be done:
# - filter words
# - duplicate specific words to increase their size in the resulting wordcloud
# - attach a timestamp in a temporary side file, delete old words?

lines = ""

while True:
    line = sys.stdin.readline()
    lines += " " + line.rstrip()
    print lines
    sys.stdout.flush() # force flush so the next program immediately gets the print, in case we're in a pipe