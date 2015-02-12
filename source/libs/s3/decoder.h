#pragma once


struct OggVorbis_File;

#include "s3.h"

namespace s3
{

class Decoder
{
	void *flac;//использовать FLAC__StreamDecoder* здесь нельзя, т.к. из-за идиотского определения этой структуры через typedef в файле stream_decoder.h, её предекларация невозможна
	OggVorbis_File *oggVorbis;

	typedef int ReadFunc(char *dest, int size, void *userPtr);
	ReadFunc *readFunc;
	void *userPtr;
	unsigned signature, signatureRead;
	enum {TYPE_UNKNOWN, TYPE_FLAC, TYPE_OGGVORBIS, TYPE_WAV, TYPE_RAW} type;

	class DecIl;
	const int *flacbuffer[2];//максимум 2 канала (стерео)
	int fbSize, fbRead;
	int readCompressed(char *buffer, int size);

public:
	Decoder();
	~Decoder();

	Format format;
	int samples, sampleSize;

	bool init(ReadFunc *readFunc, void *userPtr);//инициализация и заполнение структуры format
	int read(void *buffer, int size);//читает в буфер size байт распакованных данных
};
}
