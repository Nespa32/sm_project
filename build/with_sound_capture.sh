
# exit if there are any errors
set -e

# defines
SOUND_CAPTURE_DIR="../sound_capture/Debug"
SOUND_CAPTURE_LOOPBACK_DIR="../sound_capture_loopback/Debug"
SOUND_CAPTURE_LOOPBACK_FIXUP_DIR="../sound_capture_loopback/"
JSON_PARSER_DIR="../json_parser"
TEXT_PROCESSING_DIR="../text_processing"
WORDCLOUD_GEN_DIR="../wordcloud_gen"
TESTING_DIR="../testing"
SPEECH_TO_TEXT_DIR="../speech_to_text_request"

./$SOUND_CAPTURE_DIR/cap_wordcloud.exe 5 | \
    $SPEECH_TO_TEXT_DIR/curl_request.sh | \
    python $JSON_PARSER_DIR/json_parser.py | \
    python $TEXT_PROCESSING_DIR/text_preprocess.py | \
    python $WORDCLOUD_GEN_DIR/tkinter_script.py
    

