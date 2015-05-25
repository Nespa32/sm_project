
#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>

#include <time.h>
#include <string>

// device property defines
#include <functiondiscoverykeys_devpkey.h>

// from loopback capture
#include <audioclient.h>
#include <avrt.h>

/* audio settings that we want */
// from https://msdn.microsoft.com/en-us/library/aa908934.aspx
int const NUM_CHANNELS = 2; // mono audio (stereo would need 2 channels)
int const SAMPLES_PER_SEC = 16000;
int const BITS_PER_SAMPLE = 16;
int const BYTE_PER_SAMPLE = BITS_PER_SAMPLE / 8;
int const BLOCK_ALIGN = NUM_CHANNELS * BYTE_PER_SAMPLE;
int const BYTE_RATE = SAMPLES_PER_SEC * BLOCK_ALIGN;

void LoopbackCaptureFor(IMMDevice* mmDevice, std::string filename, int secs);
HRESULT WriteWaveHeader(HMMIO file, LPCWAVEFORMATEX pwfx, MMCKINFO* packetRIFF, MMCKINFO* packetData);
HRESULT FinishWaveFile(HMMIO file, MMCKINFO* packetRIFF, MMCKINFO* packetData);
IMMDevice* GetDefaultAudioDevice();

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Missing args. Synthax: %s $period", argv[0]);
        return 1;
    }

    // initialize COM library
    HRESULT hr = CoInitialize(nullptr);
    IMMDevice* mmDevice = GetDefaultAudioDevice();

    int period = atoi(argv[1]);

    int fileID = 0;
    for (;;)
    {
        std::string filename = "loopback_capture" + std::to_string(fileID) + ".wav";
        fileID = ++fileID % 10; // no more than 10 sound files

        LoopbackCaptureFor(mmDevice, filename, period);
        printf("%s\n", filename.c_str());
    }

    CoUninitialize();
    return 0;
}

IMMDevice* GetDefaultAudioDevice()
{
    IMMDevice* mmDevice = nullptr;
    IMMDeviceEnumerator* mmDeviceEnumerator;

    // activate a device enumerator
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&mmDeviceEnumerator);
    if (FAILED(hr))
    {
        fprintf(stderr, "CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
        return mmDevice;
    }

    // get the default render endpoint
    hr = mmDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mmDevice);
    mmDeviceEnumerator->Release();
    if (FAILED(hr))
        fprintf(stderr, "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);

    return mmDevice;
}

void LoopbackCaptureFor(IMMDevice* mmDevice, std::string filename, int secs)
{
    // open new file
    MMIOINFO mi = { 0 };

    // some flags cause mmioOpen write to this buffer
    // but not any that we're using
    std::wstring wsFilename(filename.begin(), filename.end()); // mmioOpen wants a wstring
    HMMIO file = mmioOpen(const_cast<LPWSTR>(wsFilename.c_str()), &mi, MMIO_WRITE | MMIO_CREATE);

    time_t startTime = time(nullptr);

    // activate an IAudioClient
    IAudioClient* audioClient;
    HRESULT hr = mmDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
    if (FAILED(hr))
    {
        fprintf(stderr, "IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        return;
    }

    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = audioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, nullptr);
    if (FAILED(hr))
    {
        fprintf(stderr, "IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
        audioClient->Release();
        return;
    }

    // get the default device format
    WAVEFORMATEX* waveform;
    hr = audioClient->GetMixFormat(&waveform);
    if (FAILED(hr))
    {
        fprintf(stderr, "IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
        CoTaskMemFree(waveform);
        audioClient->Release();
        return;
    }

    // coerce int-16 wave format
    // can do this in-place since we're not changing the size of the format
    // also, the engine will auto-convert from float to int for us
    switch (waveform->wFormatTag)
    {
        case WAVE_FORMAT_IEEE_FLOAT:
            waveform->wFormatTag = WAVE_FORMAT_PCM;
            waveform->wBitsPerSample = BITS_PER_SAMPLE;
            waveform->nBlockAlign = BLOCK_ALIGN;
            waveform->nAvgBytesPerSec = BYTE_RATE;
            break;
        case WAVE_FORMAT_EXTENSIBLE:
        {
            // naked scope for case-local variable
            PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(waveform);
            if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
            {
                pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                pEx->Samples.wValidBitsPerSample = BITS_PER_SAMPLE;
                waveform->wBitsPerSample = BITS_PER_SAMPLE;
                waveform->nBlockAlign = waveform->nChannels * BYTE_PER_SAMPLE;
                waveform->nAvgBytesPerSec = waveform->nBlockAlign * waveform->nSamplesPerSec;
            }
            break;
        }
    }

    MMCKINFO ckRIFF = { 0 };
    MMCKINFO ckData = { 0 };
    hr = WriteWaveHeader(file, waveform, &ckRIFF, &ckData);

    // create a periodic waitable timer
    HANDLE hWakeUp = CreateWaitableTimer(nullptr, FALSE, nullptr);
    UINT32 nBlockAlign = waveform->nBlockAlign;

    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, waveform, 0);
    if (FAILED(hr))
    {
        fprintf(stderr, "IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
        CloseHandle(hWakeUp);
        audioClient->Release();
        return;
    }

    // free up waveform
    CoTaskMemFree(waveform);

    // activate an IAudioCaptureClient
    IAudioCaptureClient* audioCaptureClient;
    hr = audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&audioCaptureClient);

    // register with MMCSS
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
    if (hTask == nullptr)
    {
        DWORD dwErr = GetLastError();
        fprintf(stderr, "AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
        audioCaptureClient->Release();
        CloseHandle(hWakeUp);
        audioClient->Release();
        return;
    }

    // set the waitable timer
    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    if (!SetWaitableTimer(hWakeUp, &liFirstFire, lTimeBetweenFires, nullptr, nullptr, FALSE))
    {
        DWORD dwErr = GetLastError();
        fprintf(stderr, "SetWaitableTimer failed: last error = %u\n", dwErr);
        AvRevertMmThreadCharacteristics(hTask);
        audioCaptureClient->Release();
        CloseHandle(hWakeUp);
        audioClient->Release();
        return;
    }

    // call IAudioClient::Start
    hr = audioClient->Start();

    // loopback capture loop
    DWORD dwWaitResult;

    UINT32 frames = 0;
    for (UINT32 passes = 0; ; passes++)
    {
        // drain data while it is available
        UINT32 nextPacketSize;
        for (hr = audioCaptureClient->GetNextPacketSize(&nextPacketSize);
            SUCCEEDED(hr) && nextPacketSize > 0;
            hr = audioCaptureClient->GetNextPacketSize(&nextPacketSize))
        {
            // get the captured data
            BYTE* data;
            UINT32 framesToRead;
            DWORD dwFlags;

            hr = audioCaptureClient->GetBuffer(&data, &framesToRead, &dwFlags, nullptr, nullptr);
            if (FAILED(hr))
            {
                fprintf(stderr, "IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", passes, frames, hr);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return;
            }

            // this type of error seems to happen often, ignore it
            if (dwFlags == AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                ;
            else if (dwFlags != 0) {
                fprintf(stderr, "IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, passes, frames);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return;
            }

            if (framesToRead == 0)
            {
                fprintf(stderr, "IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", passes, frames);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return;
            }

            LONG lBytesToWrite = framesToRead * nBlockAlign;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
            LONG lBytesWritten = mmioWrite(file, reinterpret_cast<PCHAR>(data), lBytesToWrite);
            if (lBytesToWrite != lBytesWritten)
            {
                fprintf(stderr, "mmioWrite wrote %u bytes on pass %u after %u frames: expected %u bytes\n", lBytesWritten, passes, frames, lBytesToWrite);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return;
            }

            frames += framesToRead;

            hr = audioCaptureClient->ReleaseBuffer(framesToRead);
        }

        dwWaitResult = WaitForSingleObject(hWakeUp, INFINITE);

        if (time(nullptr) - startTime > secs)
            break;
    }

    FinishWaveFile(file, &ckData, &ckRIFF);
    audioClient->Stop();
    CancelWaitableTimer(hWakeUp);
    AvRevertMmThreadCharacteristics(hTask);
    audioCaptureClient->Release();
    CloseHandle(hWakeUp);
    audioClient->Release();


    // everything went well... fixup the fact chunk in the file
    MMRESULT result = mmioClose(file, 0);
    file = nullptr;
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioClose failed: MMSYSERR = %u\n", result);
        return;
    }

    // reopen the file in read/write mode
    mi = { 0 };
    file = mmioOpen(const_cast<LPWSTR>(wsFilename.c_str()), &mi, MMIO_READWRITE);
    if (file == nullptr)
    {
        fprintf(stderr, "mmioOpen(\"%ls\", ...) failed. wErrorRet == %u\n", filename, mi.wErrorRet);
        return;
    }

    // descend into the RIFF/WAVE chunk
    ckRIFF = { 0 };
    ckRIFF.ckid = MAKEFOURCC('W', 'A', 'V', 'E'); // this is right for mmioDescend
    result = mmioDescend(file, &ckRIFF, nullptr, MMIO_FINDRIFF);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioDescend(\"WAVE\") failed: MMSYSERR = %u\n", result);
        return;
    }

    // descend into the fact chunk
    MMCKINFO ckFact = { 0 };
    ckFact.ckid = MAKEFOURCC('f', 'a', 'c', 't');
    result = mmioDescend(file, &ckFact, &ckRIFF, MMIO_FINDCHUNK);
    if (result != MMSYSERR_NOERROR) {
        fprintf(stderr, "mmioDescend(\"fact\") failed: MMSYSERR = %u\n", result);
        return;
    }

    // write the correct data to the fact chunk
    LONG lBytesWritten = mmioWrite(file, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
    if (lBytesWritten != sizeof(frames))
    {
        fprintf(stderr, "Updating the fact chunk wrote %u bytes; expected %u\n", lBytesWritten, (UINT32)sizeof(frames));
        return;
    }

    // ascend out of the fact chunk
    result = mmioAscend(file, &ckFact, 0);
    if (result != MMSYSERR_NOERROR)
        fprintf(stderr, "mmioAscend(\"fact\") failed: MMSYSERR = %u\n", result);
}

HRESULT WriteWaveHeader(HMMIO file, LPCWAVEFORMATEX pwfx, MMCKINFO* packetRIFF, MMCKINFO* packetData)
{
    MMRESULT result;

    // make a RIFF/WAVE chunk
    packetRIFF->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
    packetRIFF->fccType = MAKEFOURCC('W', 'A', 'V', 'E');

    result = mmioCreateChunk(file, packetRIFF, MMIO_CREATERIFF);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioCreateChunk(\"RIFF/WAVE\") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'fmt ' chunk (within the RIFF/WAVE chunk)
    MMCKINFO chunk;
    chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
    result = mmioCreateChunk(file, &chunk, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // write the WAVEFORMATEX data to it
    LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx->cbSize;
    LONG lBytesWritten = mmioWrite(file, reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)), lBytesInWfx);
    if (lBytesWritten != lBytesInWfx)
    {
        fprintf(stderr, "mmioWrite(fmt data) wrote %u bytes; expected %u bytes\n", lBytesWritten, lBytesInWfx);
        return E_FAIL;
    }

    // ascend from the 'fmt ' chunk
    result = mmioAscend(file, &chunk, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioAscend(\"fmt \" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'fact' chunk whose data is (DWORD)0
    chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
    result = mmioCreateChunk(file, &chunk, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // write (DWORD)0 to it
    // this is cleaned up later
    DWORD frames = 0;
    lBytesWritten = mmioWrite(file, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
    if (lBytesWritten != sizeof(frames))
    {
        fprintf(stderr, "mmioWrite(fact data) wrote %u bytes; expected %u bytes\n", lBytesWritten, (UINT32)sizeof(frames));
        return E_FAIL;
    }

    // ascend from the 'fact' chunk
    result = mmioAscend(file, &chunk, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioAscend(\"fact\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'data' chunk and leave the data pointer there
    packetData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
    result = mmioCreateChunk(file, packetData, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT FinishWaveFile(HMMIO file, MMCKINFO* packetRIFF, MMCKINFO* packetData)
{
    MMRESULT result;

    result = mmioAscend(file, packetData, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioAscend(\"data\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    result = mmioAscend(file, packetRIFF, 0);
    if (result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}
