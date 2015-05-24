
import sys
import time

# file location should be in the first arg
filename = sys.argv[1]
period = int(sys.argv[2])

with open(filename, "r") as f:
    for line in f:
        print line.rstrip()
        sys.stdout.flush() # force flush so the next program immediately gets the print, in case we're in a pipe
        # sleep for the period time
        time.sleep(period)

