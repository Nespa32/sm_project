
import json
import sys

(transcript, confidence) = ("", 0.0)

with open(sys.argv[1], 'r') as jsonfile:
    lines = jsonfile.readlines()

    values = { }
    # response can be empty
    if len(lines) > 1:
        # lines[1] since the first line is an empty result
        # the 2 lines combined do not form a valid json structure, but each line itself is a json structure
        values = json.loads(lines[1])

    try:
        transcript = values['result'][0]['alternative'][0]['transcript']
        confidence = values['result'][0]['alternative'][0]['confidence']
    except KeyError:
        pass

# we don't actually use the confidence for anything, but it's an interesting value
# print confidence
print transcript
