#include "gifdec.h"
#include <stdlib.h>
#include <stdio.h>


static long
GetFileSize(FILE* fd)
{
	if (!fd)
		return -1;

	if (fseek(fd, 0, SEEK_END) != 0)
		return -1;

	const long ret = ftell(fd);
	rewind(fd);

	return ret;
}


static GD_ERR
StreamRawBytes(const char* Path, BYTE** Buffer, size_t* BufferSize)
{
	FILE* fd = fopen(Path, "rb");

	if (!fd)
		return GD_NOTFOUND;

	const long FileSize = GetFileSize(fd);

	if (FileSize == -1)
	{
		fclose(fd);
		return GD_IOFAIL;
	}

	BYTE* StreamBuffer = malloc(FileSize);

	if (!StreamBuffer)
	{
		fclose(fd);
		return GD_NOMEM;
	}

	if (fread(StreamBuffer, sizeof(BYTE), FileSize, fd) != FileSize)
	{
		fclose(fd);
		free(StreamBuffer);
		return GD_IOFAIL;
	}

	fclose(fd);

	*Buffer = StreamBuffer;
	*BufferSize = FileSize;

	return GD_OK;
}

void test_from_memory()
{
	BYTE Buffer[16] = {
			0x00, 0x11, 0x22, 0x33,
			0x44, 0x55, 0x66, 0x77,
			0x88, 0x99, 0xAA, 0xBB,
			0xCC, 0xDD, 0xEE, 0xFF
	};

	GD_ERR ErrCode;
	size_t ErrorBytePos;

	GD_GIF_HANDLE Gif = GD_FromMemory(Buffer, sizeof(Buffer), &ErrCode, &ErrorBytePos);

	if (ErrCode != GD_OK)
	{
		fprintf(stderr, "[GD]: Decoding error code=%d, pos=%zu\n", ErrCode, ErrorBytePos);
		return;
	}

	GD_CloseGif(Gif);
}


void test_from_file()
{
	GD_ERR ErrCode;
	size_t ErrorBytePos;

	GD_GIF_HANDLE Gif = GD_OpenGif("testgif89a.gif", &ErrCode, &ErrorBytePos);

	if (ErrCode != GD_OK)
	{
		fprintf(stderr, "[GD]: Decoding error code=%d, pos=%zu\n", ErrCode, ErrorBytePos);
		return;
	}

	GD_CloseGif(Gif);
}


int main(int argc, const char** argv)
{
	test_from_file();
	// test_from_memory();

	return EXIT_SUCCESS;
}
