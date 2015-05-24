
# exit if there are any errors
set -e

# defines
SOUND_CAPTURE_DIR="../sound_capture/Debug"
JSON_PARSER_DIR="../json_parser"
TEXT_PROCESSING_DIR="../text_processing"
WORDCLOUD_GEN_DIR="../wordcloud_gen"
TESTING_DIR="../testing"
TEXT_TO_SPEECH_DIR="../text_to_speech_request"

./$SOUND_CAPTURE_DIR/cap_wordcloud.exe 5 | \
    $TEXT_TO_SPEECH_DIR/curl_request.sh | \
    python $JSON_PARSER_DIR/json_parser.py | \
    python $TEXT_PROCESSING_DIR/text_preprocess.py | \
    python $WORDCLOUD_GEN_DIR/tkinter_test.py
    

