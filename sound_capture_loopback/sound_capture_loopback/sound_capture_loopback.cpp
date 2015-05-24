
#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>

#include <time.h>

#include <functiondiscoverykeys_devpkey.h>

class CPrefs {
public:
    IMMDevice *m_pMMDevice;
    HMMIO m_hFile;
    PWAVEFORMATEX m_pwfx;
    LPCWSTR m_szFilename;

    // set hr to S_FALSE to abort but return success
    CPrefs(int argc, LPCWSTR argv[], HRESULT &hr);
    ~CPrefs();

};

// from loopback capture
#include <audioclient.h>
#include <avrt.h>

/* audio settings */
// from https://msdn.microsoft.com/en-us/library/aa908934.aspx
int const NUM_CHANNELS = 2; // mono audio (stereo would need 2 channels)
int const SAMPLES_PER_SEC = 16000;
int const BITS_PER_SAMPLE = 16;
int const BYTE_PER_SAMPLE = BITS_PER_SAMPLE / 8;
int const BLOCK_ALIGN = NUM_CHANNELS * BYTE_PER_SAMPLE;
int const BYTE_RATE = SAMPLES_PER_SEC * BLOCK_ALIGN;

int do_everything(int argc, LPCWSTR argv[]);
HRESULT LoopbackCapture(IMMDevice *pMMDevice, HMMIO hFile, PUINT32 pnFrames);

HRESULT WriteWaveHeader(HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO *pckRIFF, MMCKINFO *pckData);
HRESULT FinishWaveFile(HMMIO hFile, MMCKINFO *pckRIFF, MMCKINFO *pckData);

int _cdecl wmain(int argc, LPCWSTR argv[]) {

    // initialize COM library
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        printf("CoInitialize failed: hr = %u", hr);
        return -__LINE__;
    }

    int result = do_everything(argc, argv);

    CoUninitialize();
    return result;
}

int do_everything(int argc, LPCWSTR argv[]) {

    HRESULT hr = S_OK;

    // parse command line
    CPrefs prefs(argc, argv, hr);
    if (FAILED(hr)) {
        printf("CPrefs::CPrefs constructor failed: hr = 0x%08x\n", hr);
        return -__LINE__;
    }

    if (S_FALSE == hr) // nothing to do
        return 0;

    UINT32 nFrames = 0;

    LoopbackCapture(prefs.m_pMMDevice, prefs.m_hFile, &nFrames);
    
    // everything went well... fixup the fact chunk in the file
    MMRESULT result = mmioClose(prefs.m_hFile, 0);
    prefs.m_hFile = NULL;
    if (MMSYSERR_NOERROR != result) {
        printf("mmioClose failed: MMSYSERR = %u\n", result);
        return -__LINE__;
    }

    // reopen the file in read/write mode
    MMIOINFO mi = { 0 };
    prefs.m_hFile = mmioOpen(const_cast<LPWSTR>(prefs.m_szFilename), &mi, MMIO_READWRITE);
    if (NULL == prefs.m_hFile) {
        printf("mmioOpen(\"%ls\", ...) failed. wErrorRet == %u\n", prefs.m_szFilename, mi.wErrorRet);
        return -__LINE__;
    }

    // descend into the RIFF/WAVE chunk
    MMCKINFO ckRIFF = { 0 };
    ckRIFF.ckid = MAKEFOURCC('W', 'A', 'V', 'E'); // this is right for mmioDescend
    result = mmioDescend(prefs.m_hFile, &ckRIFF, NULL, MMIO_FINDRIFF);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioDescend(\"WAVE\") failed: MMSYSERR = %u\n", result);
        return -__LINE__;
    }

    // descend into the fact chunk
    MMCKINFO ckFact = { 0 };
    ckFact.ckid = MAKEFOURCC('f', 'a', 'c', 't');
    result = mmioDescend(prefs.m_hFile, &ckFact, &ckRIFF, MMIO_FINDCHUNK);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioDescend(\"fact\") failed: MMSYSERR = %u\n", result);
        return -__LINE__;
    }

    // write the correct data to the fact chunk
    LONG lBytesWritten = mmioWrite(
        prefs.m_hFile,
        reinterpret_cast<PCHAR>(&nFrames),
        sizeof(nFrames)
        );
    if (lBytesWritten != sizeof(nFrames)) {
        printf("Updating the fact chunk wrote %u bytes; expected %u\n", lBytesWritten, (UINT32)sizeof(nFrames));
        return -__LINE__;
    }

    // ascend out of the fact chunk
    result = mmioAscend(prefs.m_hFile, &ckFact, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"fact\") failed: MMSYSERR = %u\n", result);
        return -__LINE__;
    }

    // let prefs' destructor call mmioClose

    return 0;
}


#define DEFAULT_FILE L"loopback-capture.wav"

void usage(LPCWSTR exe);
HRESULT get_default_device(IMMDevice **ppMMDevice);
HRESULT list_devices();
HRESULT get_specific_device(LPCWSTR szLongName, IMMDevice **ppMMDevice);
HRESULT open_file(LPCWSTR szFileName, HMMIO *phFile);

void usage(LPCWSTR exe) {
    printf(
        "%ls -?\n"
        "%ls --list-devices\n"
        "%ls [--device \"Device long name\"] [--file \"file name\"] [--int-16]\n"
        "\n"
        "    -? prints this message.\n"
        "    --list-devices displays the long names of all active playback devices.\n"
        "    --device captures from the specified device (default if omitted)\n"
        "    --file saves the output to a file (%ls if omitted)\n"
        "    --int-16 attempts to coerce data to 16-bit integer format\n",
        exe, exe, exe, DEFAULT_FILE
        );
}

CPrefs::CPrefs(int argc, LPCWSTR argv[], HRESULT &hr)
: m_pMMDevice(NULL)
, m_hFile(NULL)
, m_pwfx(NULL)
, m_szFilename(NULL)
{
    switch (argc) {
    case 2:
        if (0 == _wcsicmp(argv[1], L"-?") || 0 == _wcsicmp(argv[1], L"/?")) {
            // print usage but don't actually capture
            hr = S_FALSE;
            usage(argv[0]);
            return;
        }
        else if (0 == _wcsicmp(argv[1], L"--list-devices")) {
            // list the devices but don't actually capture
            hr = list_devices();

            // don't actually play
            if (S_OK == hr) {
                hr = S_FALSE;
                return;
            }
        }
        // intentional fallthrough

    default:
        // loop through arguments and parse them
        for (int i = 1; i < argc; i++) {

            // --device
            if (0 == _wcsicmp(argv[i], L"--device")) {
                if (NULL != m_pMMDevice) {
                    printf("Only one --device switch is allowed\n");
                    hr = E_INVALIDARG;
                    return;
                }

                if (i++ == argc) {
                    printf("--device switch requires an argument\n");
                    hr = E_INVALIDARG;
                    return;
                }

                hr = get_specific_device(argv[i], &m_pMMDevice);
                if (FAILED(hr)) {
                    return;
                }

                continue;
            }

            // --file
            if (0 == _wcsicmp(argv[i], L"--file")) {
                if (NULL != m_szFilename) {
                    printf("Only one --file switch is allowed\n");
                    hr = E_INVALIDARG;
                    return;
                }

                if (i++ == argc) {
                    printf("--file switch requires an argument\n");
                    hr = E_INVALIDARG;
                    return;
                }

                m_szFilename = argv[i];
                continue;
            }

            printf("Invalid argument %ls\n", argv[i]);
            hr = E_INVALIDARG;
            return;
        }

        // open default device if not specified
        if (NULL == m_pMMDevice) {
            hr = get_default_device(&m_pMMDevice);
            if (FAILED(hr)) {
                return;
            }
        }

        // if no filename specified, use default
        if (NULL == m_szFilename) {
            m_szFilename = DEFAULT_FILE;
        }

        // open file
        hr = open_file(m_szFilename, &m_hFile);
        if (FAILED(hr)) {
            return;
        }
    }
}

CPrefs::~CPrefs() {
    if (NULL != m_pMMDevice) {
        m_pMMDevice->Release();
    }

    if (NULL != m_hFile) {
        mmioClose(m_hFile, 0);
    }

    if (NULL != m_pwfx) {
        CoTaskMemFree(m_pwfx);
    }
}

HRESULT get_default_device(IMMDevice **ppMMDevice) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    // activate a device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
        );
    if (FAILED(hr)) {
        printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
        return hr;
    }

    // get the default render endpoint
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, ppMMDevice);
    pMMDeviceEnumerator->Release();
    if (FAILED(hr)) {
        printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
        return hr;
    }

    return S_OK;
}

HRESULT list_devices() {
    HRESULT hr = S_OK;

    // get an enumerator
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
        );
    if (FAILED(hr)) {
        printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
        return hr;
    }

    IMMDeviceCollection *pMMDeviceCollection;

    // get all the active render endpoints
    hr = pMMDeviceEnumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection
        );
    pMMDeviceEnumerator->Release();
    if (FAILED(hr)) {
        printf("IMMDeviceEnumerator::EnumAudioEndpoints failed: hr = 0x%08x\n", hr);
        return hr;
    }

    UINT count;
    hr = pMMDeviceCollection->GetCount(&count);
    if (FAILED(hr)) {
        pMMDeviceCollection->Release();
        printf("IMMDeviceCollection::GetCount failed: hr = 0x%08x\n", hr);
        return hr;
    }
    printf("Active render endpoints found: %u\n", count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice *pMMDevice;

        // get the "n"th device
        hr = pMMDeviceCollection->Item(i, &pMMDevice);
        if (FAILED(hr)) {
            pMMDeviceCollection->Release();
            printf("IMMDeviceCollection::Item failed: hr = 0x%08x\n", hr);
            return hr;
        }

        // open the property store on that device
        IPropertyStore *pPropertyStore;
        hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
        pMMDevice->Release();
        if (FAILED(hr)) {
            pMMDeviceCollection->Release();
            printf("IMMDevice::OpenPropertyStore failed: hr = 0x%08x\n", hr);
            return hr;
        }

        // get the long name property
        PROPVARIANT pv; PropVariantInit(&pv);
        hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &pv);
        pPropertyStore->Release();
        if (FAILED(hr)) {
            pMMDeviceCollection->Release();
            printf("IPropertyStore::GetValue failed: hr = 0x%08x\n", hr);
            return hr;
        }

        if (VT_LPWSTR != pv.vt) {
            printf("PKEY_Device_FriendlyName variant type is %u - expected VT_LPWSTR", pv.vt);

            PropVariantClear(&pv);
            pMMDeviceCollection->Release();
            return E_UNEXPECTED;
        }

        printf("    %ls\n", pv.pwszVal);

        PropVariantClear(&pv);
    }
    pMMDeviceCollection->Release();

    return S_OK;
}

HRESULT get_specific_device(LPCWSTR szLongName, IMMDevice **ppMMDevice) {
    HRESULT hr = S_OK;

    *ppMMDevice = NULL;

    // get an enumerator
    IMMDeviceEnumerator *pMMDeviceEnumerator;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pMMDeviceEnumerator
        );
    if (FAILED(hr)) {
        printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
        return hr;
    }

    IMMDeviceCollection *pMMDeviceCollection;

    // get all the active render endpoints
    hr = pMMDeviceEnumerator->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection
        );
    pMMDeviceEnumerator->Release();
    if (FAILED(hr)) {
        printf("IMMDeviceEnumerator::EnumAudioEndpoints failed: hr = 0x%08x\n", hr);
        return hr;
    }

    UINT count;
    hr = pMMDeviceCollection->GetCount(&count);
    if (FAILED(hr)) {
        pMMDeviceCollection->Release();
        printf("IMMDeviceCollection::GetCount failed: hr = 0x%08x\n", hr);
        return hr;
    }

    for (UINT i = 0; i < count; i++) {
        IMMDevice *pMMDevice;

        // get the "n"th device
        hr = pMMDeviceCollection->Item(i, &pMMDevice);
        if (FAILED(hr)) {
            pMMDeviceCollection->Release();
            printf("IMMDeviceCollection::Item failed: hr = 0x%08x\n", hr);
            return hr;
        }

        // open the property store on that device
        IPropertyStore *pPropertyStore;
        hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
        if (FAILED(hr)) {
            pMMDevice->Release();
            pMMDeviceCollection->Release();
            printf("IMMDevice::OpenPropertyStore failed: hr = 0x%08x\n", hr);
            return hr;
        }

        // get the long name property
        PROPVARIANT pv; PropVariantInit(&pv);
        hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &pv);
        pPropertyStore->Release();
        if (FAILED(hr)) {
            pMMDevice->Release();
            pMMDeviceCollection->Release();
            printf("IPropertyStore::GetValue failed: hr = 0x%08x\n", hr);
            return hr;
        }

        if (VT_LPWSTR != pv.vt) {
            printf("PKEY_Device_FriendlyName variant type is %u - expected VT_LPWSTR", pv.vt);

            PropVariantClear(&pv);
            pMMDevice->Release();
            pMMDeviceCollection->Release();
            return E_UNEXPECTED;
        }

        // is it a match?
        if (0 == _wcsicmp(pv.pwszVal, szLongName)) {
            // did we already find it?
            if (NULL == *ppMMDevice) {
                *ppMMDevice = pMMDevice;
                pMMDevice->AddRef();
            }
            else {
                printf("Found (at least) two devices named %ls\n", szLongName);
                PropVariantClear(&pv);
                pMMDevice->Release();
                pMMDeviceCollection->Release();
                return E_UNEXPECTED;
            }
        }

        pMMDevice->Release();
        PropVariantClear(&pv);
    }
    pMMDeviceCollection->Release();

    if (NULL == *ppMMDevice) {
        printf("Could not find a device named %ls\n", szLongName);
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    return S_OK;
}

HRESULT open_file(LPCWSTR szFileName, HMMIO *phFile) {
    MMIOINFO mi = { 0 };

    *phFile = mmioOpen(
        // some flags cause mmioOpen write to this buffer
        // but not any that we're using
        const_cast<LPWSTR>(szFileName),
        &mi,
        MMIO_WRITE | MMIO_CREATE
        );

    if (NULL == *phFile) {
        printf("mmioOpen(\"%ls\", ...) failed. wErrorRet == %u\n", szFileName, mi.wErrorRet);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT LoopbackCapture(IMMDevice *pMMDevice,HMMIO hFile, PUINT32 pnFrames)
{
    time_t start = time(NULL);
    HRESULT hr; 

    // activate an IAudioClient
    IAudioClient* audioClient;
    hr = pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient);
    if (FAILED(hr)) {
        printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        return hr;
    }

    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = audioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr)) {
        printf("IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
        audioClient->Release();
        return hr;
    }

    // get the default device format
    WAVEFORMATEX *waveform;
    hr = audioClient->GetMixFormat(&waveform);
    if (FAILED(hr)) {
        printf("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
        CoTaskMemFree(waveform);
        audioClient->Release();
        return hr;
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
            if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
                pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                pEx->Samples.wValidBitsPerSample = BITS_PER_SAMPLE;
                waveform->wBitsPerSample = BITS_PER_SAMPLE;
                waveform->nBlockAlign = waveform->nChannels * BYTE_PER_SAMPLE;
                waveform->nAvgBytesPerSec = waveform->nBlockAlign * waveform->nSamplesPerSec;
                printf("Number of channels: %u\n", waveform->nChannels);
                printf("SamplesSec: %u\n", waveform->nSamplesPerSec);
                printf("waveform->nBlockAlign: %u\n", waveform->nBlockAlign);
            }
            break;
        }
    }

    MMCKINFO ckRIFF = { 0 };
    MMCKINFO ckData = { 0 };
    hr = WriteWaveHeader(hFile, waveform, &ckRIFF, &ckData);

    // create a periodic waitable timer
    HANDLE hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
    UINT32 nBlockAlign = waveform->nBlockAlign;

    *pnFrames = 0;

    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, waveform, 0);
    if (FAILED(hr)) {
        printf("IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
        CloseHandle(hWakeUp);
        audioClient->Release();
        return hr;
    }

    // free up waveform
    CoTaskMemFree(waveform);

    // activate an IAudioCaptureClient
    IAudioCaptureClient* audioCaptureClient;
    hr = audioClient->GetService(__uuidof(IAudioCaptureClient),(void**)&audioCaptureClient);

    // register with MMCSS
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
    if (NULL == hTask) {
        DWORD dwErr = GetLastError();
        printf("AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
        audioCaptureClient->Release();
        CloseHandle(hWakeUp);
        audioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }

    // set the waitable timer
    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    BOOL bOK = SetWaitableTimer(hWakeUp, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE );
    if (!bOK) {
        DWORD dwErr = GetLastError();
        printf("SetWaitableTimer failed: last error = %u\n", dwErr);
        AvRevertMmThreadCharacteristics(hTask);
        audioCaptureClient->Release();
        CloseHandle(hWakeUp);
        audioClient->Release();
        return HRESULT_FROM_WIN32(dwErr);
    }

    // call IAudioClient::Start
    hr = audioClient->Start();

    // loopback capture loop
    DWORD dwWaitResult;

    for (UINT32 passes = 0; ; passes++) {
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

            hr = audioCaptureClient->GetBuffer(&data, &framesToRead, &dwFlags, NULL, NULL);
            if (FAILED(hr)) {
                printf("IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", passes, *pnFrames, hr);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return hr;
            }

            // this type of error seems to happen often, ignore it
            if (dwFlags == AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                ;
            else if (0 != dwFlags) {
                printf("IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, passes, *pnFrames);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return E_UNEXPECTED;
            }

            if (0 == framesToRead) {
                printf("IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", passes, *pnFrames);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return E_UNEXPECTED;
            }

            LONG lBytesToWrite = framesToRead * nBlockAlign;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
            LONG lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(data), lBytesToWrite);
            if (lBytesToWrite != lBytesWritten) {
                printf("mmioWrite wrote %u bytes on pass %u after %u frames: expected %u bytes\n", lBytesWritten, passes, *pnFrames, lBytesToWrite);
                audioClient->Stop();
                CancelWaitableTimer(hWakeUp);
                AvRevertMmThreadCharacteristics(hTask);
                audioCaptureClient->Release();
                CloseHandle(hWakeUp);
                audioClient->Release();
                return E_UNEXPECTED;
            }

            *pnFrames += framesToRead;

            hr = audioCaptureClient->ReleaseBuffer(framesToRead);
        }

        dwWaitResult = WaitForSingleObject(hWakeUp, INFINITE);

        if (time(NULL) - start > 5)
            break;
    }

    hr = FinishWaveFile(hFile, &ckData, &ckRIFF);

    audioClient->Stop();
    CancelWaitableTimer(hWakeUp);
    AvRevertMmThreadCharacteristics(hTask);
    audioCaptureClient->Release();
    CloseHandle(hWakeUp);
    audioClient->Release();
    return hr;
}

HRESULT WriteWaveHeader(HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO *pckRIFF, MMCKINFO *pckData) {
    MMRESULT result;

    // make a RIFF/WAVE chunk
    pckRIFF->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
    pckRIFF->fccType = MAKEFOURCC('W', 'A', 'V', 'E');

    result = mmioCreateChunk(hFile, pckRIFF, MMIO_CREATERIFF);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"RIFF/WAVE\") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'fmt ' chunk (within the RIFF/WAVE chunk)
    MMCKINFO chunk;
    chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
    result = mmioCreateChunk(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // write the WAVEFORMATEX data to it
    LONG lBytesInWfx = sizeof(WAVEFORMATEX)+pwfx->cbSize;
    LONG lBytesWritten =
        mmioWrite(
        hFile,
        reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)),
        lBytesInWfx
        );
    if (lBytesWritten != lBytesInWfx) {
        printf("mmioWrite(fmt data) wrote %u bytes; expected %u bytes\n", lBytesWritten, lBytesInWfx);
        return E_FAIL;
    }

    // ascend from the 'fmt ' chunk
    result = mmioAscend(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"fmt \" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'fact' chunk whose data is (DWORD)0
    chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
    result = mmioCreateChunk(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // write (DWORD)0 to it
    // this is cleaned up later
    DWORD frames = 0;
    lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
    if (lBytesWritten != sizeof(frames)) {
        printf("mmioWrite(fact data) wrote %u bytes; expected %u bytes\n", lBytesWritten, (UINT32)sizeof(frames));
        return E_FAIL;
    }

    // ascend from the 'fact' chunk
    result = mmioAscend(hFile, &chunk, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"fact\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    // make a 'data' chunk and leave the data pointer there
    pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
    result = mmioCreateChunk(hFile, pckData, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT FinishWaveFile(HMMIO hFile, MMCKINFO *pckRIFF, MMCKINFO *pckData) {
    MMRESULT result;

    result = mmioAscend(hFile, pckData, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"data\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    result = mmioAscend(hFile, pckRIFF, 0);
    if (MMSYSERR_NOERROR != result) {
        printf("mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x\n", result);
        return E_FAIL;
    }

    return S_OK;
}
