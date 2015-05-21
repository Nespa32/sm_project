
import sys

# file location should be in the first arg
filename = sys.argv[1]

f = open(filename, "r")
lines = f.readlines()
f.close

lastline = lines.pop() # remove last line

# reopen the file in write mode
f = open(filename, "w")

for line in lines:
    f.write(line)
    
f.close()

print lastline.rstrip()
