
#include <cstdio>
#include <thread>

#include <windows.h>

#include <fstream>
#include <cstdlib>
#include <mmsystem.h>

#include <assert.h> // debugging

#include <cstdlib>

int NUM_CHANNELS = 1; // mono audio (stereo would need 2 channels)
int SAMPLES_PER_SEC = 11025;
int BITS_PER_SAMPLE = 8;

#define INP_BUFFER_SIZE SAMPLES_PER_SEC * 10

/* Declare procedures */
void SaveWavFile(char* filename, PWAVEHDR pWaveHdr);
void ReadWavFile(char* filename);

int main(int /*argc*/, char** /*argv*/)
{
    printf("test\n");

    WAVEHDR waveHdr;
    PBYTE buffer;
    HWAVEIN hWaveIn;

    /* begin sound capture */
    buffer = reinterpret_cast<PBYTE>(malloc(INP_BUFFER_SIZE));
    if (!buffer)
    {
        printf("Failed to allocate buffers\n");
        return 1;
    }

    // Open waveform audio for input
    WAVEFORMATEX waveform;
    waveform.wFormatTag = WAVE_FORMAT_PCM;
    waveform.nChannels = NUM_CHANNELS;
    waveform.nSamplesPerSec = SAMPLES_PER_SEC;
    waveform.nAvgBytesPerSec = SAMPLES_PER_SEC;
    waveform.nBlockAlign = 1;
    waveform.wBitsPerSample = BITS_PER_SAMPLE;
    waveform.cbSize = 0;

    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, NULL, NULL, CALLBACK_WINDOW);

    if (result)
    {
        // todo: properly print this error - WCHAR is unicode garbage
        /*
        WCHAR fault[256];
        waveInGetErrorText(result, fault, 256);
        printf("Failed to open waveform input device (fault: %s)", result);
        */

        printf("Failed to open waveform input device\n", result);
        return 1;
    }

    // Set up headers and prepare them
    waveHdr.lpData = reinterpret_cast<CHAR*>(buffer);
    waveHdr.dwBufferLength = INP_BUFFER_SIZE;
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
        return 1;
    }

    // Commence sampling input
    result = waveInStart(hWaveIn);
    if (result)
    {
        printf("Failed to start recording\n");
        return 1;
    }

    // Wait until finished recording
    do { } while (waveInUnprepareHeader(hWaveIn, &waveHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING);

    waveInClose(hWaveIn);

    SaveWavFile("temp.wav", &waveHdr);

    return 0;
}

// Read the temporary wav file
void ReadWavFile(char* filename)
{
    // random variables used throughout the function
    int length, byte_samp, byte_sec;
    bool mono = TRUE;

    FILE* file;
    // open filepointer readonly
    fopen_s(&file, filename, "r");
    if (file == NULL)
        printf("Wav:: Could not open file: %s", filename);
    else
    {
        // declare a char buff to store some values in
        char *buff = new char[5];
        buff[4] = '\0';
        // read the first 4 bytes
        fread((void *)buff, 1, 4, file);
        // the first four bytes should be 'RIFF'
        if (strcmp((char *)buff, "RIFF") == 0)
        {
            // read byte 8,9,10 and 11
            fseek(file, 4, SEEK_CUR);
            fread((void *)buff, 1, 4, file);
            // this should read "WAVE"
            if (strcmp((char *)buff, "WAVE") == 0)
            {
                // read byte 12,13,14,15
                fread((void *)buff, 1, 4, file);
                // this should read "fmt "
                if (strcmp((char *)buff, "fmt ") == 0)
                {
                    fseek(file, 20, SEEK_CUR);
                    // final one read byte 36,37,38,39
                    fread((void *)buff, 1, 4, file);
                    if (strcmp((char *)buff, "data") == 0)
                    {
                        // Now we know it is a wav file, rewind the stream
                        rewind(file);
                        // now is it mono or stereo ?
                        fseek(file, 22, SEEK_CUR);
                        fread((void *)buff, 1, 2, file);

                        // bool isMono = (buff[0] & 0x02 == 0);

                        // read the sample rate
                        fread((void *)&SAMPLES_PER_SEC, 1, 4, file);
                        fread((void *)&byte_sec, 1, 4, file);
                        byte_samp = 0;
                        fread((void *)&byte_samp, 1, 2, file);

                        fread((void *)&BITS_PER_SAMPLE, 1, 2, file);
                        fseek(file, 4, SEEK_CUR);
                        fread((void *)&length, 1, 4, file);
                    }
                }
            }
        }

        delete buff;
    }
}

void SaveWavFile(char* filename, PWAVEHDR pWaveHdr)
{
    std::fstream file(filename, std::fstream::out | std::fstream::binary);

    int pcmsize = sizeof(PCMWAVEFORMAT);
    int audioFormat = WAVE_FORMAT_PCM;

    int subchunk1size = 16;
    int byteRate = SAMPLES_PER_SEC * NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int blockAlign = NUM_CHANNELS * BITS_PER_SAMPLE / 8;
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
    file.write((char*)&byteRate, 4);            // byte rate (SampleRate * NumChannels * BitsPerSample/8)
    file.write((char*)&blockAlign, 2);          // block align (NumChannels * BitsPerSample/8)
    file.write((char*)&BITS_PER_SAMPLE, 2);     // bits per sample
    file.write("data", 4);                      // subchunk2ID
    file.write((char*)&subchunk2size, 4);       // subchunk2size (NumSamples * NumChannels * BitsPerSample/8)

    file.write(pWaveHdr->lpData, pWaveHdr->dwBufferLength); // data
    file.close();
}
