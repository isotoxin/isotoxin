#include "stdafx.h"
#include "decoder.h"

/*
#pragma comment(lib, "libFLAC_static")
#pragma comment(lib, "libogg_static")
#pragma comment(lib, "libvorbis_static")
#pragma comment(lib, "libvorbisfile_static")
*/

namespace s3
{
Decoder::Decoder() : flac(NULL), oggVorbis(NULL)
{
	type = TYPE_UNKNOWN;
}

Decoder::~Decoder()
{
	if (flac) FLAC__stream_decoder_delete((FLAC__StreamDecoder*)flac);//FLAC__stream_decoder_finish вызывать не обязательно, т.к. он все равно вызывается в FLAC__stream_decoder_delete

	if (oggVorbis && oggVorbis->datasource) ov_clear(oggVorbis);
	delete oggVorbis;
}

int Decoder::readCompressed(char *buffer, int size)
{
	if (signatureRead)//еще не прочитана сигнатура
	{
		if (size > (int)signatureRead) size = signatureRead;
		memcpy(buffer, (char*)&signature + 4 - signatureRead, size);
		signatureRead -= size;
		return size;
// 		int n = std::min(size, (int)signatureRead);
// 		memcpy(buffer, (char*)&signature + 4 - signatureRead, n);
// 		signatureRead -= n;
// 		size -= n;
// 		if (size == 0) return n;
//		return (*readFunc)(buffer + n, size, userPtr) + n;
	}

	return (*readFunc)(buffer, size, userPtr);
}

class Decoder::DecIl//Decoder Internal
{
public:
	static size_t OggReadCallback(void *ptr, size_t size, size_t nmemb, void *datasource)
	{
		int r = ((Decoder*)datasource)->readCompressed((char*)ptr, size*nmemb);
		return r < 0 ? _set_errno(/*EIO*/5), 0 : r;
	}

	static FLAC__StreamDecoderReadStatus FlacReadCallback(const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t *bytes, void *client_data)
	{
		int r = ((Decoder*)client_data)->readCompressed((char*)buffer, *bytes);
		if (r > 0) {*bytes = r; return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;}
		if (r == 0) {*bytes = 0; return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;}
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	static void FlacMetadataCallback(const FLAC__StreamDecoder* /*decoder*/, const FLAC__StreamMetadata *metadata, void *client_data)
	{
		if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
		{
			Decoder *d = (Decoder*)client_data;
			const FLAC__StreamMetadata_StreamInfo &si = metadata->data.stream_info;
			d->format.sampleRate	= si.sample_rate;
			d->format.channels		= (short)si.channels;
			d->format.bitsPerSample	= (short)si.bits_per_sample;
			d->samples				= ( int )si.total_samples;
		}
	}

	static FLAC__StreamDecoderWriteStatus FlacWriteCallback(const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
	{
		Decoder *d = (Decoder*)client_data;

		//Жесткая завязка на реализацию libflac (а именно, запоминание указателя на внутрений буфер) - конечно
		//нехорошо, но не хочется выделять дополнительную память под буфер только из-за того, что разработчики
		//libflac поленились сделать нормальный API с возможностью чтения заданного кол-ва байт (хотя бы как в
		//vorbisfile).
		//Сохраняется не указатель на массив буферов buffer, а указатели на сами буфера, т.к. массив буферов
		//может быть временным (на стеке) при использовании seeking-а, см. flac/src/libFLAC/stream_decoder.c: write_audio_frame_to_client_(...) {... const FLAC__int32 *newbuffer[FLAC__MAX_CHANNELS]; ...}
		for (int i=0; i<d->format.channels; i++) d->flacbuffer[i] = buffer[i];

		d->fbSize = frame->header.blocksize;
		d->fbRead = 0;

		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	static void FlacErrorCallback(const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus, void *) {}//без error callback-а flac не инициализируется :(
};

bool Decoder::init(ReadFunc *readFunc_, void *userPtr_)
{
	type = TYPE_UNKNOWN;
	readFunc = readFunc_;
	userPtr  = userPtr_;

	if ((*readFunc)((char*)&signature, 4, userPtr) != 4) return false;
	signatureRead = 4;
//	curSample = 0;

	if (signature == MAKEFOURCC('f','L','a','C'))
	{
		if (flac)
			FLAC__stream_decoder_reset((FLAC__StreamDecoder*)flac);//так гораздо быстрее, чем переинициализировать декодер каждый раз
		else
		{
			flac = FLAC__stream_decoder_new();
			if (FLAC__stream_decoder_init_stream((FLAC__StreamDecoder*)flac, DecIl::FlacReadCallback, NULL, NULL,
				NULL, NULL, DecIl::FlacWriteCallback, DecIl::FlacMetadataCallback, DecIl::FlacErrorCallback, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK) return false;
		}

		format.channels = 0;//на случай, если FlacMetadataCallback не будет вызван
		if (!FLAC__stream_decoder_process_until_end_of_metadata((FLAC__StreamDecoder*)flac) || format.channels <= 0 || format.channels > 2 || format.bitsPerSample != 16) return false;
		fbSize = fbRead = 0;

		type = TYPE_FLAC;
	}
	else if (signature == MAKEFOURCC('O','g','g','S'))
	{
		if (!oggVorbis) oggVorbis = new OggVorbis_File, oggVorbis->datasource = NULL;
		if (oggVorbis->datasource) ov_clear(oggVorbis), oggVorbis->datasource = NULL;

		ov_callbacks callbacks = {DecIl::OggReadCallback, NULL, NULL, NULL};
		if (ov_open_callbacks(this, oggVorbis, NULL, 0, callbacks)) return false;

		vorbis_info *info = ov_info(oggVorbis, -1);
		format.sampleRate	 = info->rate;
		format.channels		 = (short)info->channels;
		format.bitsPerSample = 16;
		samples = 0;//(int)ov_pcm_total(oggVorbis, -1);//эта функция работает только для seekable bitstream :(

		type = TYPE_OGGVORBIS;
	}
	else if (signature == MAKEFOURCC('R','I','F','F'))
	{
		struct
		{
			DWORD fsize, WAVESIG, fmt_SIG, fmtSize;
			PCMWAVEFORMAT fmt;
		} s;
		if ((*readFunc)((char*)&s, sizeof(s), userPtr) != sizeof(s)) return false;
		if (s.WAVESIG != MAKEFOURCC('W','A','V','E') || s.fmt_SIG != MAKEFOURCC('f','m','t',' ')) return false;
		if (s.fmtSize != sizeof(PCMWAVEFORMAT) || s.fmt.wf.wFormatTag != WAVE_FORMAT_PCM) return false;
		if (s.fmt.wBitsPerSample != 8 && s.fmt.wBitsPerSample != 16) return false;
		if (s.fmt.wf.nChannels != 1 && s.fmt.wf.nChannels != 2) return false;
		for (;;)
		{
			struct {DWORD id, size;} block;
			if ((*readFunc)((char*)&block, sizeof(block), userPtr) != sizeof(block)) return false;
			if (block.id == MAKEFOURCC('d','a','t','a'))
			{
				format.sampleRate	 = s.fmt.wf.nSamplesPerSec;
				format.channels		 = s.fmt.wf.nChannels;
				format.bitsPerSample = s.fmt.wBitsPerSample;
				samples = block.size / s.fmt.wf.nBlockAlign;
				fbSize = block.size;
				break;
			}
			for (char temp[1024]; block.size > 0;)//эмуляция seek :)
			{
				int sz = std::min((DWORD)sizeof(temp), block.size);
				if ((*readFunc)(temp, sz, userPtr) != sz) return false;
				block.size -= sz;
			}
		}

		type = TYPE_WAV;
	}
	else if (signature == MAKEFOURCC('R','A','W',' '))
	{
		if ((*readFunc)((char*)&format, sizeof(format), userPtr) != sizeof(format)) return false;
		if (format.channels == 0) return false;
		samples = 0;
		type = TYPE_RAW;
	}
	else return false;

	sampleSize = format.channels * format.bitsPerSample / 8;

	return true;
}

int Decoder::read(void *buffer, int size)
{
	if (size <= 0 || size % sampleSize != 0) return 0;

	switch (type)
	{
	case TYPE_FLAC:
		size /= sampleSize;
		for (short *dest = (short*)buffer;;)
		{
			//Сначала копируем остаток буфера (если есть)
			int n = std::min(size, fbSize), i = fbRead, nn = n + fbRead;

			if (format.channels == 1) for (; i<nn; i++) *dest++ = (short)flacbuffer[0][/*fbRead+*/i];//mono
			else for (; i<nn; i++) *dest++ = (short)flacbuffer[0][i], *dest++ = (short)flacbuffer[1][i];//stereo

			fbRead += n;
			fbSize -= n;
			size -= n;
			if (size == 0) return (char*)dest - (char*)buffer;

			_ASSERT(fbSize == 0);
			if (!FLAC__stream_decoder_process_single((FLAC__StreamDecoder*)flac) || fbSize == 0) {type = TYPE_UNKNOWN; return (char*)dest - (char*)buffer;}
		}
		break;

	case TYPE_OGGVORBIS:
		for (int totalRead=0; totalRead<size;)
		{
			int bytesRead = ov_read(oggVorbis, (char*)buffer + totalRead, size - totalRead, 0, 2, 1, NULL);
			if (bytesRead <= 0) return totalRead;
			totalRead += bytesRead;
		}
		return size;

	case TYPE_WAV:
		if (size > fbSize) size = fbSize;
		fbSize -= size;//в fbSize хранится кол-во оставшихся байт
	case TYPE_RAW:
		return (*readFunc)((char*)buffer, size, userPtr);

	case TYPE_UNKNOWN:
	default:
		return 0;
	}
}
}
