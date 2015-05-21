import json

jsonFile = open('output.json', 'r')
lines = jsonFile.readlines()

# lines[1] since the first line is an empty result
# the 2 lines combined do not form a valid json structure, but each line itself is a json structure
values = json.loads(lines[1])

transcript = values['result'][0]['alternative'][0]['transcript']
# confidence = values['result'][0]['alternative'][0]['confidence']

jsonFile.close

print transcript
# print confidence
