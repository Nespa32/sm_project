
import json
import sys

# each input line should be a valid json object
# just parse them one by one, discard json objects without the fields we need, and print out the useful info
while True:
    (transcript, confidence) = ("", 0.0)
    jsonline = sys.stdin.readline()
    values = json.loads(jsonline)

    try:
        transcript = values['result'][0]['alternative'][0]['transcript']
        confidence = values['result'][0]['alternative'][0]['confidence']
    except KeyError:
        pass
    except IndexError:
        pass

    # we don't actually use the confidence for anything, but it's an interesting value
    # print confidence
    print transcript
    sys.stdout.flush() # force flush so the next program immediately gets the print, in case we're in a pipe
