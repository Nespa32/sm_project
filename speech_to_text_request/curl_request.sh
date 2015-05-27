
while read -r line
do
    filename=$line
    curl -X POST --data-binary @$filename \
        --user-agent 'Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/535.2 (KHTML, like Gecko) Chrome/15.0.874.121 Safari/535.2' \
        --header 'Content-Type: audio/l16; rate=16000;' \
        -s \
        'https://www.google.com/speech-api/v2/recognize?output=json&lang=en-us&key=AIzaSyDetj9qHeESaxrEi_7YBC1OcQYLy7xf_E0&client=chromium&maxresults=6&pfilter=2'
done
