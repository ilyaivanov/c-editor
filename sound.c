#include <windows.h>
#include <dsound.h>

#include "util/win32.c"

#pragma pack(push, 1)
typedef struct WAVFile
{
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    int wav_size;        // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"

    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    int fmt_chunk_size; // Should be 16 for PCM
    short audio_format; // Should be 1 for PCM. 3 for IEEE Float
    short num_channels;
    int sample_rate;
    int byte_rate;          // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    short sample_alignment; // num_channels * Bytes Per Sample
    short bit_depth;        // Number of bits per sample

    // Data
    char data_header[4]; // Contains "data"
    int data_bytes;      // Number of bytes in data. Number of samples * num_channels * sample byte size
    u8 bytes[];          // Remainder of wave file is bytes
} WAVFile;

#pragma pack(pop)

typedef HRESULT WINAPI MyDirectSoundCreate(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

typedef struct CursorInfo
{
    u32 play;
    u32 write;
    u32 runningSampleIndex;
} CursorInfo;

u32 cursorInfoPosition = 0;
CursorInfo cursors[800];

int status = 0;

IDirectSoundBuffer *mainBuffer;

typedef struct win32_sound_output
{
    int SamplesPerSecond;
    i32 RunningSampleIndex;
    int BytesPerSample;
    int SecondaryBufferSize;
    f32 tSine;
    int LatencySampleCount;
} win32_sound_output;

// WAVFile *fileToPlay;
win32_sound_output SoundOutput = {0};

typedef struct
{
    u32 isPlaying;
    u32 currentSampleIndex;
    WAVFile *file;
} FilePlaying;

FilePlaying files[255];

FileContent oneFile;
FileContent twoFile;
FileContent goFile;
FileContent fireFile;
FileContent burstFile;
FileContent musicFile;

typedef enum
{
    One,
    Two,
    Go,
    Fire,
    Burst,
    Music,
    SoundCount
} Sound;

void PlayFile(Sound sound)
{
    i32 foundIndex = -1;
    for (i32 fi = 0; fi < ArrayLength(files); fi++)
    {
        if (!files[fi].isPlaying)
        {
            foundIndex = fi;
            break;
        }
    }

    if (foundIndex >= 0)
    {
        FilePlaying *file = &files[foundIndex];

        file->isPlaying = 1;
        file->currentSampleIndex = 0;
        if (sound == One)
            file->file = (WAVFile *)oneFile.content;
        if (sound == Music)
            file->file = (WAVFile *)musicFile.content;
        else if (sound == Two)
            file->file = (WAVFile *)twoFile.content;
        else if (sound == Go)
            file->file = (WAVFile *)goFile.content;
        else if (sound == Fire)
            file->file = (WAVFile *)fireFile.content;
        else if (sound == Burst)
            file->file = (WAVFile *)burstFile.content;
    }
}

void InitSoundOutput()
{
    // FileContent scifi = ReadMyFileImp("C:\\Users\\ila_i\\Downloads\\Shapeforms Audio Free Sound Effects\\Sci Fi Weapons Cyberpunk Arsenal Preview\\AUDIO\\EXPLDsgn_Implode_15.wav");
    musicFile = ReadMyFileImp("..\\sounds\\music_test.wav");

    oneFile = ReadMyFileImp("..\\sounds\\1.wav");
    twoFile = ReadMyFileImp("..\\sounds\\2.wav");
    goFile = ReadMyFileImp("..\\sounds\\go.wav");
    fireFile = ReadMyFileImp("..\\sounds\\file.wav");
    burstFile = ReadMyFileImp("..\\sounds\\burst.wav");

    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = 3 * 2;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 12;

    PlayFile(Music);
}

i32 mybuffer[48000];

// PerByte
void WriteAudioFile(VOID *region, DWORD size)
{
    DWORD samples = size / SoundOutput.BytesPerSample;
    byte *SampleOut = (byte *)region;

    memset(&mybuffer[0], 0, 48000 * sizeof(i32));

    for (i32 fi = 0; fi < ArrayLength(files); fi++)
    {
        FilePlaying *fileInfo = &files[fi];

        if (fileInfo->isPlaying)
        {
            byte *fileContent = (byte *)fileInfo->file->bytes;
            u32 bytesPerChannelSample = SoundOutput.BytesPerSample / fileInfo->file->num_channels;
            u32 samplesInFile = fileInfo->file->data_bytes / (fileInfo->file->bit_depth / 8) / fileInfo->file->num_channels;

            for (DWORD i = 0; i < samples; i++)
            {
                const int channelsCount = 2;
                const int j = fileInfo->currentSampleIndex * channelsCount * bytesPerChannelSample;

                i32 leftSample = (fileContent[j]) | (fileContent[j + 1] << 8) | (fileContent[j + 2] << 16);
                if (leftSample & 0x800000)
                    leftSample |= 0xFF000000;

                mybuffer[i * 2] += leftSample;

                i32 rightSample = (fileContent[j + 3]) | (fileContent[j + 3 + 1] << 8) | (fileContent[j + 3 + 2] << 16);
                if (rightSample & 0x800000)
                    rightSample |= 0xFF000000;

                mybuffer[i * 2 + 1] += rightSample;

                ++fileInfo->currentSampleIndex;

                if (fileInfo->currentSampleIndex >= samplesInFile)
                {
                    fileInfo->isPlaying = 0;
                    break;
                }
            }
        }
    }

    for (i32 i = 0; i < samples * 2; i++)
    {
        i32 clipped = mybuffer[i];

        const i32 max = 8388607;
        const i32 min = -8388608;

        if (clipped > max)
            clipped = (max - (clipped - max) / 2);
        if (clipped < min)
            clipped = (min - (clipped - min) / 2);

        *SampleOut++ = clipped & 0xff;
        *SampleOut++ = (clipped >> 8) & 0xff;
        *SampleOut++ = (clipped >> 16) & 0xff;
        // SampleOut[i] = mybuffer[i];
    }

    SoundOutput.RunningSampleIndex += samples;
}

void WriteSineWave(DWORD ByteToLock, DWORD BytesToWrite)
{
    // TODO(casey): More strenuous test!
    // TODO(casey): Switch to a sine wave
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(mainBuffer->lpVtbl->Lock(mainBuffer, ByteToLock, BytesToWrite,
                                           &Region1, &Region1Size,
                                           &Region2, &Region2Size,
                                           0)))
    {
        WriteAudioFile(Region1, Region1Size);
        WriteAudioFile(Region2, Region2Size);
        mainBuffer->lpVtbl->Unlock(mainBuffer, Region1, Region1Size, Region2, Region2Size);
    }
}

void InitSound(HWND window)
{
    InitSoundOutput();

    status = 0;
    HMODULE soundLib = LoadLibraryA("dsound.dll");

    if (soundLib)
    {
        MyDirectSoundCreate *soundCreate = (MyDirectSoundCreate *)GetProcAddress(soundLib, "DirectSoundCreate");
        IDirectSound *directSound = 0;
        if (soundCreate && SUCCEEDED(soundCreate(0, &directSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {0};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SoundOutput.SamplesPerSecond;
            WaveFormat.wBitsPerSample = 24;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(directSound->lpVtbl->SetCooperativeLevel(directSound, window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {0};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(directSound->lpVtbl->CreateSoundBuffer(directSound, &BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->lpVtbl->SetFormat(PrimaryBuffer, &WaveFormat);
                    if (SUCCEEDED(Error))
                    {
                        // NOTE(casey): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        status = 1;
                    }
                }
                else
                {
                    status = 1;
                }
            }

            DSBUFFERDESC BufferDescription = {0};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = SoundOutput.SamplesPerSecond * 3 * 2;
            BufferDescription.lpwfxFormat = &WaveFormat;
            HRESULT Error = directSound->lpVtbl->CreateSoundBuffer(directSound, &BufferDescription, &mainBuffer, 0);
            if (SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }
        }
        else
        {
            status = 1;
        }
    }
    else
    {
        status = 1;
    }

    if (status != 0)
    {
        OutputDebugStringA("Failed to init sound platform\n");
    }
    else
    {
        OutputDebugStringA("Sound is good and starting\n");

        mainBuffer->lpVtbl->Play(mainBuffer, 0, 0, DSBPLAY_LOOPING);
        WriteSineWave(0, SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample);
    }
}

void UpdateSound()
{
    DWORD PlayCursor;
    DWORD WriteCursor;
    if (SUCCEEDED(mainBuffer->lpVtbl->GetCurrentPosition(mainBuffer, &PlayCursor, &WriteCursor)))
    {
        cursors[cursorInfoPosition].play = (u32)PlayCursor;
        cursors[cursorInfoPosition].write = (u32)WriteCursor;
        cursors[cursorInfoPosition].runningSampleIndex = SoundOutput.RunningSampleIndex;

        cursorInfoPosition++;

        if (cursorInfoPosition >= ArrayLength(cursors))
            cursorInfoPosition = 0;

        DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                            SoundOutput.SecondaryBufferSize);

        DWORD TargetCursor =
            ((PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
             SoundOutput.SecondaryBufferSize);

        DWORD BytesToWrite;

        // TODO(casey): Change this to using a lower latency offset from the playcursor
        // when we actually start having sound effects.
        if (ByteToLock > TargetCursor)
        {
            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
            BytesToWrite += TargetCursor;
        }
        else
        {
            BytesToWrite = TargetCursor - ByteToLock;
        }

        WriteSineWave(ByteToLock, BytesToWrite);
    }
}