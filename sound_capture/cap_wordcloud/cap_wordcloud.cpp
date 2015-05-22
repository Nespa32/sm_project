
#include <cstdio>
#include <thread>

#include <windows.h>

#include <fstream>
#include <cstdlib>
#include <mmsystem.h>

#include <assert.h> // debugging

#include <cstdlib>

// define to enable flac (if you're crazy enough)
// #define __USE_FLAC__

#ifdef __USE_FLAC__
#include "FLAC++/encoder.h"
#include "FLAC/stream_encoder.h"
#endif

/* audio settings */
// from https://msdn.microsoft.com/en-us/library/aa908934.aspx
int const NUM_CHANNELS = 1; // mono audio (stereo would need 2 channels)
int const SAMPLES_PER_SEC = 16000;
int const BITS_PER_SAMPLE = 16;
int const BYTE_PER_SAMPLE = BITS_PER_SAMPLE / 8;
int const BLOCK_ALIGN = NUM_CHANNELS * BYTE_PER_SAMPLE;
int const BYTE_RATE = SAMPLES_PER_SEC * BLOCK_ALIGN;

void CaptureSoundFor(int secs, std::string destfile);
void SaveWavFile(std::string filename, PWAVEHDR pWaveHdr);
void ReadWavFile(std::string filename);

// FLAC
void ConvertWavToFlac(std::string wavfile, std::string flacfile);

#ifdef __USE_FLAC__
static void FlacProgressCallback(const FLAC__StreamEncoder* encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void* client_data);
#endif

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Missing args. Synthax: cap_wordcloud.exe $duration $destfile");
        return 1;
    }

    int duration = atoi(argv[1]);
    std::string destfile = argv[2];
    CaptureSoundFor(duration, destfile);
    return 0;
}

void CaptureSoundFor(int secs, std::string destfile)
{
    assert(secs > 0);

    int bufferSize = SAMPLES_PER_SEC * BYTE_PER_SAMPLE * NUM_CHANNELS * secs;
    printf("bufferSize is %d\n", bufferSize);

    WAVEHDR waveHdr;
    PBYTE buffer;
    HWAVEIN hWaveIn;

    /* begin sound capture */
    buffer = (PBYTE)malloc(bufferSize);
    if (!buffer)
    {
        printf("Failed to allocate buffers\n");
        assert(false);
    }

    // Open waveform audio for input
    WAVEFORMATEX waveform;
    waveform.wFormatTag = WAVE_FORMAT_PCM;
    waveform.nChannels = NUM_CHANNELS;
    waveform.nSamplesPerSec = SAMPLES_PER_SEC;
    waveform.nAvgBytesPerSec = BYTE_RATE;
    waveform.nBlockAlign = BLOCK_ALIGN;
    waveform.wBitsPerSample = BITS_PER_SAMPLE;
    waveform.cbSize = 0;

    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, NULL, NULL, CALLBACK_NULL);

    if (result)
    {
        printf("Failed to open waveform input device\n", result);
        assert(false);
    }

    // Set up headers and prepare them
    waveHdr.lpData = reinterpret_cast<CHAR*>(buffer);
    waveHdr.dwBufferLength = bufferSize;
    waveHdr.dwBytesRecorded = 0;
    waveHdr.dwUser = 0;
    waveHdr.dwFlags = 0;
    waveHdr.dwLoops = 1;
    waveHdr.lpNext = NULL;
    waveHdr.reserved = 0;

    waveInPrepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR));

    // Insert a wave input buffer
    result = waveInAddBuffer(hWaveIn, &waveHdr, sizeof(WAVEHDR));
    if (result)
    {
        printf("Failed to read block from device\n");
        assert(false);
    }

    // Commence sampling input
    result = waveInStart(hWaveIn);
    if (result)
    {
        printf("Failed to start recording\n");
        assert(false);
    }

    time_t startTime = time(NULL);

    // Wait until finished recording
    while (waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING)
        ;

    time_t endTime = time(NULL);

    int timeDiff = int(endTime - startTime);
    if (secs != timeDiff)
        printf("WARNING: requested capture time (%d) is different from real capture time (%d)", secs, timeDiff);

    waveInClose(hWaveIn);

    SaveWavFile(destfile, &waveHdr);

#ifdef __USE_FLAC__
    SaveWavFile("temp.wav", &waveHdr);
    ConvertWavToFlac("temp.wav", destfile);
#endif
}

// Read the temporary wav file
void ReadWavFile(char* filename)
{
    // random variables used throughout the function
    int length, byte_samp, byte_sec;

    FILE* file;
    // open filepointer readonly
    fopen_s(&file, filename, "rb");
    if (file == NULL)
    {
        printf("Wav:: Could not open file: %s", filename);
        return;
    }

    // declare a char buff to store some values in
    char buff[5];
    buff[4] = '\0';

    // read the first 4 bytes
    fread((void*)buff, 1, 4, file);

    // the first four bytes should be 'RIFF'
    if (strcmp((char*)buff, "RIFF") != 0)
    {
        printf("ReadWavFile:: incorrect file format? First four bytes not 'RIFF'");
        return;
    }

    // read byte 8,9,10 and 11
    fseek(file, 4, SEEK_CUR);
    fread((void *)buff, 1, 4, file);
    // this should read "WAVE"
    if (strcmp((char *)buff, "WAVE") != 0)
    {
        printf("ReadWavFile:: incorrect file format? Could not read 'WAVE'");
        return;
    }

    // read byte 12,13,14,15
    fread((void *)buff, 1, 4, file);
    // this should read "fmt "
    if (strcmp((char *)buff, "fmt ") != 0)
    {
        printf("ReadWavFile:: incorrect file format? Could not read 'fmt '");
        return;
    }

    fseek(file, 20, SEEK_CUR);
    // final one read byte 36,37,38,39
    fread((void *)buff, 1, 4, file);
    if (strcmp((char *)buff, "data") != 0)
    {
        printf("ReadWavFile:: incorrect file format? Could not read 'data'");
        return;
    }

    // Now we know it is a wav file, rewind the stream
    rewind(file);
    // now is it mono or stereo ?
    fseek(file, 22, SEEK_CUR);
    fread((void *)buff, 1, 2, file);

    // bool isMono = (buff[0] & 0x02 == 0);

    // read the sample rate
    fread((void *)&SAMPLES_PER_SEC, 1, 4, file);
    fread((void *)&byte_sec, 1, 4, file);
    fread((void *)&byte_samp, 1, 2, file);

    fread((void *)&BITS_PER_SAMPLE, 1, 2, file);
    fseek(file, 4, SEEK_CUR);
    fread((void *)&length, 1, 4, file);
}

void SaveWavFile(std::string filename, PWAVEHDR pWaveHdr)
{
    std::fstream file(filename, std::fstream::out | std::fstream::binary);

    int pcmsize = sizeof(PCMWAVEFORMAT);
    int audioFormat = WAVE_FORMAT_PCM;

    int subchunk1size = 16;
    int subchunk2size = pWaveHdr->dwBufferLength * NUM_CHANNELS;
    int chunksize = (36 + subchunk2size);
    // write the wav file per the wav file format
    file.seekp(0, std::ios::beg);
    file.write("RIFF", 4);                      // chunk id
    file.write((char*)&chunksize, 4);           // chunk size (36 + SubChunk2Size))
    file.write("WAVE", 4);                      // format
    file.write("fmt ", 4);                      // subchunk1ID
    file.write((char*)&subchunk1size, 4);       // subchunk1size (16 for PCM)
    file.write((char*)&audioFormat, 2);         // AudioFormat (1 for PCM)
    file.write((char*)&NUM_CHANNELS, 2);        // NumChannels
    file.write((char*)&SAMPLES_PER_SEC, 4);     // sample rate
    file.write((char*)&BYTE_RATE, 4);           // byte rate (SampleRate * NumChannels * BitsPerSample/8)
    file.write((char*)&BLOCK_ALIGN, 2);         // block align (NumChannels * BitsPerSample/8)
    file.write((char*)&BITS_PER_SAMPLE, 2);     // bits per sample
    file.write("data", 4);                      // subchunk2ID
    printf("subchunk2size is %d\n", subchunk2size);
    file.write((char*)&subchunk2size, 4);       // subchunk2size (NumSamples * NumChannels * BitsPerSample/8)

    file.write(pWaveHdr->lpData, pWaveHdr->dwBufferLength); // data
    file.close();
}

#ifdef __USE_FLAC__
static unsigned totalSamples = 0; /* can use a 32-bit number due to WAVE size limitations */

void ConvertWavToFlac(std::string wavfile, std::string flacfile)
{
    FLAC__StreamEncoder* encoder = 0;
    FLAC__StreamEncoderInitStatus initStatus;

    FILE* file;
    fopen_s(&file, wavfile.c_str(), "rb");

    if (file == NULL)
    {
        printf("ConvertWavToFlac:: couldn't open input file\n", wavfile);
        return;
    }

    int const READ_BUFFER_SIZE = 4096;
    FLAC__byte buffer[READ_BUFFER_SIZE * BYTE_PER_SAMPLE * NUM_CHANNELS]; /* we read the WAVE data into here */
    FLAC__int32 pcm[READ_BUFFER_SIZE * NUM_CHANNELS];

    // some consistency checks on the string fields of the file
    if (fread(buffer, 1, 44, file) != 44 ||
        memcmp(buffer, "RIFF", 4) ||
        memcmp(buffer + 8, "WAVE", 4) ||
        memcmp(buffer + 12, "fmt ", 4) ||
        memcmp(buffer + 36, "data", 4))
    {
        printf("ConvertWavToFlac:: file %s doesn't look like a wav file", wavfile);
        fclose(file);
        return;
    }

    // @todo: find better way to read this value, this looks ugly
    totalSamples = (((((((unsigned)buffer[43] << 8) | buffer[42]) << 8) | buffer[41]) << 8) | buffer[40]) / BYTE_PER_SAMPLE;

    /* allocate the encoder */
    encoder = FLAC__stream_encoder_new();
    if (encoder == NULL)
    {
        printf("ConvertWavToFlac:: couldn't allocate encoder\n");
        fclose(file);
        return;
    }

    FLAC__bool ok = true;
    ok &= FLAC__stream_encoder_set_verify(encoder, true);
    ok &= FLAC__stream_encoder_set_compression_level(encoder, 5);
    ok &= FLAC__stream_encoder_set_channels(encoder, NUM_CHANNELS);
    ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, BITS_PER_SAMPLE);
    ok &= FLAC__stream_encoder_set_sample_rate(encoder, SAMPLES_PER_SEC);
    ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, totalSamples);

    /* initialize encoder */
    if (ok)
    {
        initStatus = FLAC__stream_encoder_init_file(encoder, flacfile.c_str(), FlacProgressCallback, nullptr);
        if (initStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        {
            printf("ConvertWavToFlac:: error while initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[initStatus]);
            ok = false;
        }
    }

    /* read blocks of samples from WAVE file and feed to encoder */
    if (ok)
    {
        size_t left = (size_t)totalSamples;
        while (ok && left)
        {
            size_t need = (left > READ_BUFFER_SIZE ? (size_t)READ_BUFFER_SIZE : (size_t)left);
            if (fread(buffer, NUM_CHANNELS * (BITS_PER_SAMPLE / 8), need, file) != need)
            {
                printf("ConvertWavToFlac:: error reading from WAVE file\n");
                ok = false;
            }
            else
            {
                /* convert the packed little-endian 16-bit PCM samples from WAVE into an interleaved FLAC__int32 buffer for libFLAC */
                size_t i;
                for (i = 0; i < need * NUM_CHANNELS; i++)
                    /* inefficient but simple and works on big- or little-endian machines */
                    pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)buffer[2 * i + 1] << 8) | (FLAC__int16)buffer[2 * i]);

                /* feed samples to encoder */
                ok = FLAC__stream_encoder_process_interleaved(encoder, pcm, need);
            }
            left -= need;
        }
    }

    ok &= FLAC__stream_encoder_finish(encoder);

    printf("ConvertWavToFlac:: %s\n", ok ? "Success" : "FAILED");
    if (!ok)
        printf("* State: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);

    FLAC__stream_encoder_delete(encoder);
    fclose(file);
}

void FlacProgressCallback(FLAC__StreamEncoder const* /*encoder*/, FLAC__uint64 bytes_written,
    FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void* /*client_data*/)
{
    printf("FlacProgressCallback:: Wrote %llu bytes, %llu/%u samples, %u/%u frames\n",
        bytes_written, samples_written, totalSamples, frames_written, total_frames_estimate);
}

#endif // __USE_FLAC__