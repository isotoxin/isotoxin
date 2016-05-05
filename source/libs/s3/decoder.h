#pragma once


struct OggVorbis_File;

#include "s3.h"

namespace s3
{

class Decoder
{
	void *flac;//использовать FLAC__StreamDecoder* здесь нельзя, т.к. из-за идиотского определения этой структуры через typedef в файле stream_decoder.h, её предекларация невозможна
	OggVorbis_File *oggVorbis;

	typedef s3int ReadFunc(char *dest, s3int size, void *userPtr);
	ReadFunc *readFunc;
	void *userPtr;
	unsigned signature, signatureRead;
	enum {TYPE_UNKNOWN, TYPE_FLAC, TYPE_OGGVORBIS, TYPE_WAV, TYPE_RAW} type;

	class DecIl;
	const int *flacbuffer[2]; // max 2 channels (stereo)
	int fbSize, fbRead;
    s3int readCompressed(char *buffer, s3int size);

public:
	Decoder();
	~Decoder();

	Format format;
	int samples, sampleSize;

	bool init(ReadFunc *readFunc, void *userPtr);//initialization and fill of format structure
    s3int read(void *buffer, s3int size);//reads [size] bytes of unpacked data into [buffer]
};
}
