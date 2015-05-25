
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
TEXT_TO_SPEECH_DIR="../text_to_speech_request"

./$SOUND_CAPTURE_LOOPBACK_DIR/sound_capture_loopback.exe 5 | \
    $SOUND_CAPTURE_LOOPBACK_FIXUP_DIR/sound_format_fixup.sh | \
    $TEXT_TO_SPEECH_DIR/curl_request.sh | \
    python $JSON_PARSER_DIR/json_parser.py | \
    python $TEXT_PROCESSING_DIR/text_preprocess.py | \
    python $WORDCLOUD_GEN_DIR/tkinter_script.py

