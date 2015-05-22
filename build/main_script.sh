
set -e # exit if there are any errors

SOUND_CAPTURE_DIR="../sound_capture/Debug"
JSON_PARSER_DIR="../json_parser"
TEXT_PROCESSING_DIR="../text_processing"
WORDCLOUD_GEN_DIR="../wordcloud_gen"
TESTING_DIR="../testing"

# test file that will feed the script
cat a_new_hope.txt > text_feed.txt
echo "" > text_cache.txt
echo "empty" > wordcloud_text.txt

# python $WORDCLOUD_GEN_DIR/tkinter_test.py wordcloud_text.txt &

while true; do
    echo "STAGE: SOUND CAPTURE"

    # records for 10 seconds, puts into test_flac.flac
    # also creates a temp.wav debug file
    # ./$SOUND_CAPTURE_DIR/cap_wordcloud.exe test_flac.flac

    echo "STAGE: SPEECH-TO-TEXT"

    # uses curl, which is available via git bash
    # save json output
    # for 44100 bps flac sample
    #curl -X POST --data-binary @good-morning-google.flac \
    #    --user-agent 'Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/535.2 (KHTML, like Gecko) Chrome/15.0.874.121 Safari/535.2' \
    #    --header 'Content-Type: audio/x-flac; rate=44100;' \
    #    -s \
    #    'https://www.google.com/speech-api/v2/recognize?output=json&lang=en-us&key=AIzaSyDetj9qHeESaxrEi_7YBC1OcQYLy7xf_E0&client=chromium&maxresults=6&pfilter=2' \
    #    > output.json

    # for 16000 PCM sample
    # hello-16bit-PCM.wav
    # example-16bit-pcm.wav
    #curl -X POST --data-binary @example-16bit-pcm.wav \
    #    --user-agent 'Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/535.2 (KHTML, like Gecko) Chrome/15.0.874.121 Safari/535.2' \
    #    --header 'Content-Type: audio/l16; rate=16000;' \
    #    -s \
    #    'https://www.google.com/speech-api/v2/recognize?output=json&lang=en-us&key=AIzaSyDetj9qHeESaxrEi_7YBC1OcQYLy7xf_E0&client=chromium&maxresults=6&pfilter=2' \
    #    > output.json

    echo "STAGE: JSON PARSE"

    # parse output.json and append to raw_text.txt
    # python $JSON_PARSER_DIR/json_parser.py > raw_text.txt
    
    # text_feed_script.py $input_file
    python $TESTING_DIR/text_feed_script.py text_feed.txt > raw_text.txt
    sleep 5 # sleep for 1 sec, our 'capture' time

    echo "STAGE: TEXT PRE-PROCESSING"

    # text_preprocess.py $input_file
    # python $TEXT_PROCESSING_DIR/text_preprocess.py raw_text.txt text_cache.txt > wordcloud_text.txt
    # TextProcessor needs to be in the same folder, because of java classpath issues
    java TextProcessor raw_text.txt text_cache.txt > wordcloud_text.txt

    echo "STAGE: WORDCLOUD GEN"

    # run it asynchronously so the next sound capture can begin
    # python $WORDCLOUD_GEN_DIR/wordcloud_gen.py wordcloud_text.txt

done

# prevent the bash from immediately exiting
read -p "Press any key"
