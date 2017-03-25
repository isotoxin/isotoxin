#pragma once

namespace fmt
{
    typedef unsigned short      WORD;
    typedef unsigned long       DWORD;

    typedef struct waveformat_tag {
        WORD    wFormatTag;        /* format type */
        WORD    nChannels;         /* number of channels (i.e. mono, stereo, etc.) */
        DWORD   nSamplesPerSec;    /* sample rate */
        DWORD   nAvgBytesPerSec;   /* for buffer estimation */
        WORD    nBlockAlign;       /* block size of data */
    } WAVEFORMAT, *PWAVEFORMAT, *NPWAVEFORMAT, *LPWAVEFORMAT;

    typedef struct tWAVEFORMATEX
    {
        WORD        wFormatTag;         /* format type */
        WORD        nChannels;          /* number of channels (i.e. mono, stereo...) */
        DWORD       nSamplesPerSec;     /* sample rate */
        DWORD       nAvgBytesPerSec;    /* for buffer estimation */
        WORD        nBlockAlign;        /* block size of data */
        WORD        wBitsPerSample;     /* number of bits per sample of mono data */
        WORD        cbSize;             /* the count in bytes of the size of */
                                        /* extra information (after cbSize) */
    } WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;

    typedef struct pcmwaveformat_tag {
        WAVEFORMAT  wf;
        WORD        wBitsPerSample;
    } PCMWAVEFORMAT, *PPCMWAVEFORMAT, *NPPCMWAVEFORMAT, *LPPCMWAVEFORMAT;

#define WAVE_FORMAT_PCM     1

}



