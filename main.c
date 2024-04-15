#include "gd.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


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
StreamRawBytes(const char* Path, GD_BYTE** Buffer, size_t* BufferSize)
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

	GD_BYTE* StreamBuffer = malloc(FileSize);

	if (!StreamBuffer)
	{
		fclose(fd);
		return GD_NOMEM;
	}

	if (fread(StreamBuffer, sizeof(GD_BYTE), FileSize, fd) != FileSize)
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
	GD_BYTE Buffer[16] = {
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




void
DumpDataBlocks(GD_DataBlockList* Blocks)
{
	size_t BlockCount = Blocks->BlockCount;

	printf("Data Blocks (%zu):\n", BlockCount);

	GD_DataBlock* Current = Blocks->Head;

	for (size_t i = 0; i < BlockCount; ++i)
	{
		printf("---- Block #%zu\n", i);

		for (size_t j = 0; j < Current->EffectiveSize; ++j)
		{
			printf("%02X ", Current->Data[j]);

			if ((j + 1) % 16 == 0)
				printf("\n");
		}

		Current = Current->FLink;
	}

	printf("\n");
}

void
OnGraphicsExt(GD_EXT_GRAPHICS* ExData)
{
	puts("Graphics extension routine called");

	if (!ExData)
		return;

	printf("--------------------------\n");
	printf("| PackedFields: %d\n", ExData->PackedFields);
	printf("| Delay Time:   %d\n", ExData->DelayTime);
	printf("| Transparent:  %d\n", ExData->TransparentColorIndex);
	printf("--------------------------\n");
}

void
OnCommentExt(GD_EXT_COMMENT* ExData)
{
	puts("Comment extension routine called");

	if (ExData)
		DumpDataBlocks(&ExData->Blocks);
}

void
OnPlaintextExt(GD_EXT_PLAINTEXT* ExData)
{
	puts("Plaintext extension routine called");

	if (!ExData)
		return;

	printf("---------------------------------------\n");
	printf("| Text Grid Position Left:     %u\n", ExData->GridPositionLeft);
	printf("| Text Grid Position Top:      %u\n", ExData->GridPositionTop);
	printf("| Text Grid Width:             %u\n", ExData->GridWidth);
	printf("| Text Grid Height:            %u\n", ExData->GridHeight);
	printf("| Character Cell Width:        %u\n", ExData->CharCellWidth);
	printf("| Character Cell Height:       %u\n", ExData->CharCellHeight);
	printf("| Text Foreground Color Index: %u\n", ExData->FgColorIndex);
	printf("| Text Background Color Index: %u\n", ExData->BgColorIndex);
	printf("---------------------------------------\n");

	DumpDataBlocks(&ExData->Blocks);
}

void
OnAppExt(GD_EXT_APPLICATION* ExData)
{
	puts("Application extension routine called");

	if (!ExData)
		return;

	printf("--------------------------\n");
	printf("| App Id:   %.8s\n", ExData->AppId);
	printf("| App Auth: %.3s\n", ExData->AppAuth);
	printf("--------------------------\n");

	DumpDataBlocks(&ExData->Blocks);
}


int main(int argc, const char** argv)
{
	GD_ERR ErrCode;
	size_t ErrorBytePos;

	assert(GD_OK == GD_RegisterExRoutine(GD_APPLICATION, OnAppExt));
	assert(GD_OK == GD_RegisterExRoutine(GD_GRAPHICS, OnGraphicsExt));
	assert(GD_OK == GD_RegisterExRoutine(GD_COMMENT, OnCommentExt));
	assert(GD_OK == GD_RegisterExRoutine(GD_PLAINTEXT, OnPlaintextExt));

	GD_GIF_HANDLE Gif = GD_OpenGif("testgif89a.gif", &ErrCode, &ErrorBytePos);

	if (ErrCode != GD_OK)
	{
		fprintf(stderr, "[GD]: Decoding error code=%d (%s), pos=%zu\n", ErrCode, GD_ErrorAsString(ErrCode), ErrorBytePos);
		return EXIT_FAILURE;
	}

	GD_CloseGif(Gif);

	return EXIT_SUCCESS;
}
