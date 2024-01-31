#include "gifdec.h"
#include <stdlib.h>


#define SIGNATURE_SIZE 3
#define VERSION_SIZE   3
#define HEADER_SIZE (SIGNATURE_SIZE + VERSION_SIZE)


#define LSD_HAS_GCT(LsdFields)  ((LsdFields) & 0x80)
#define LSD_GCT_SIZE(LsdFields) (2 << (((LsdFields) & 7)))


static GD_ERR
GD_InitDecodeContextStream(GD_DECODE_CONTEXT* DecodeCtx, const char* Path)
{
	FILE* fd = fopen(Path, "rb");

	if (!fd)
		return GD_NOTFOUND;

	DecodeCtx->StreamFd = fd;
	DecodeCtx->SourceMode = GD_FROM_STREAM;
	DecodeCtx->SourceEOF = GD_FALSE;
	DecodeCtx->DataStreamOffset = 0;

	//
	// Unused members
	//
	DecodeCtx->MemoryBuffer = NULL;
	DecodeCtx->MemoryBufferSize = 0;

	//
	// Load a first chunk
	//
	GD_DecoderLoadChunk(DecodeCtx);

	return GD_OK;
}

static GD_ERR
GD_InitDecodeContextMemory(GD_DECODE_CONTEXT* DecodeCtx, const void* Buffer, size_t BufferSize)
{
	DecodeCtx->StreamFd = NULL;

	DecodeCtx->SourceMode = GD_FROM_MEMORY;
	DecodeCtx->MemoryBuffer = (BYTE*)Buffer;
	DecodeCtx->MemoryBufferSize = BufferSize;
	DecodeCtx->SourceBeg = DecodeCtx->MemoryBuffer;
	DecodeCtx->SourceEnd = DecodeCtx->MemoryBuffer + BufferSize;
	DecodeCtx->SourceEOF = GD_FALSE;
	DecodeCtx->DataStreamOffset = 0;

	return GD_OK;
}

static void
GD_DecoderLoadChunk(GD_DECODE_CONTEXT* DecodeCtx)
{
	const size_t BytesRead = fread(DecodeCtx->StreamChunk, 1, CHUNK_SIZE, DecodeCtx->StreamFd);

	DecodeCtx->SourceBeg = DecodeCtx->StreamChunk;
	DecodeCtx->SourceEnd = DecodeCtx->SourceBeg + BytesRead;
}

static GD_BOOL
GD_DecoderCanRead(GD_DECODE_CONTEXT* DecodeCtx)
{
	if (DecodeCtx->SourceBeg < DecodeCtx->SourceEnd)
		return GD_TRUE;

	if (DecodeCtx->SourceMode == GD_FROM_STREAM)
	{
		//
		// Try loading a new chunk from stream
		//
		GD_DecoderLoadChunk(DecodeCtx);

		DecodeCtx->SourceEOF = DecodeCtx->SourceBeg >= DecodeCtx->SourceEnd;
		return !DecodeCtx->SourceEOF;
	}

	DecodeCtx->SourceEOF = GD_TRUE;

	return GD_FALSE;
}

static BYTE
GD_ReadByte(GD_DECODE_CONTEXT* DecodeCtx)
{
	if (GD_DecoderCanRead(DecodeCtx))
	{
		++DecodeCtx->DataStreamOffset;
		return *DecodeCtx->SourceBeg++;
	}

	return 0;
}

static size_t
GD_ReadBytes(GD_DECODE_CONTEXT* DecodeCtx, BYTE* Buffer, size_t Count)
{
	size_t i;

	for (i = 0; i < Count && !DecodeCtx->SourceEOF; ++i)
		Buffer[i] = GD_ReadByte(DecodeCtx);

	return i;
}

static WORD
GD_ReadWord(GD_DECODE_CONTEXT* DecodeCtx)
{
	BYTE HiByte = GD_ReadByte(DecodeCtx);
	BYTE LoByte = GD_ReadByte(DecodeCtx);

	//
	// Convert to little-endian (high byte becomes low bytes, and vice-versa)
	//
	return HiByte | (LoByte << 8);
}

static GD_ERR
GD_DecoderAdvance(GD_DECODE_CONTEXT* DecodeCtx, size_t BytesCount)
{
	if (DecodeCtx->SourceMode == GD_FROM_MEMORY)
	{
		DecodeCtx->SourceBeg += BytesCount;
		DecodeCtx->DataStreamOffset += BytesCount;

		return GD_OK;
	}

	//
	// Advance from file
	//
	const long SeekTo = (long)(DecodeCtx->DataStreamOffset + BytesCount);
	const int ret = fseek(DecodeCtx->StreamFd, SeekTo, SEEK_SET);

	if (ret != 0)
		return GD_IOFAIL;

	DecodeCtx->DataStreamOffset += BytesCount;
	GD_DecoderLoadChunk(DecodeCtx);

	return GD_OK;
}

static GD_ERR
GD_ValidateHeader(GD_DECODE_CONTEXT* DecodeCtx, GD_GIF_VERSION* Version)
{
	BYTE Header[HEADER_SIZE];

	//
	// Read first 6 bytes of the data stream
	//
	size_t BytesRead = GD_ReadBytes(DecodeCtx, Header, HEADER_SIZE);

	if (BytesRead != HEADER_SIZE)
		return GD_NOT_ENOUGH_DATA;

	//
	// Must match with one of the two versions
	//
	const char* Version1Header = "GIF89a";
	const char* Version2Header = "GIF87a";

	for (BYTE i = 0; i < HEADER_SIZE; ++i)
	{
		if (Header[i] != (BYTE)Version1Header[i] &&
		    Header[i] != (BYTE)Version2Header[i])
		{
			return GD_UNEXPECTED_DATA;
		}
	}

	*Version = (Header[4] == '9') ? GD_GIF89A : GD_GIF87A;

	return GD_OK;
}


static GD_ERR
GD_ReadScreenDescriptor(GD_DECODE_CONTEXT* DecodeCtx, GD_LOGICAL_SCREEN_DESCRIPTOR* ScrDescriptor)
{
	ScrDescriptor->LogicalWidth     = GD_ReadWord(DecodeCtx);
	ScrDescriptor->LogicalHeight    = GD_ReadWord(DecodeCtx);
	ScrDescriptor->PackedFields     = GD_ReadByte(DecodeCtx);
	ScrDescriptor->BgColorIndex     = GD_ReadByte(DecodeCtx);
	ScrDescriptor->PixelAspectRatio = GD_ReadByte(DecodeCtx);

	return GD_OK;
}

static GD_ERR
GD_ReadGlobalColorTable(GD_DECODE_CONTEXT* DecodeCtx, GD_GLOBAL_COLOR_TABLE* Table, BYTE ScrDescriptorFields)
{
	Table->Count = LSD_GCT_SIZE(ScrDescriptorFields);

	for (size_t i = 0; i < Table->Count; ++i)
	{
		Table->Internal[i].r = GD_ReadByte(DecodeCtx);
		Table->Internal[i].g = GD_ReadByte(DecodeCtx);
		Table->Internal[i].b = GD_ReadByte(DecodeCtx);
	}

	return GD_OK;
}

static void
GD_IgnoreSubDataBlocks(GD_DECODE_CONTEXT* DecodeCtx)
{
	for (BYTE BSize = GD_ReadByte(DecodeCtx);
	          BSize != 0;
			  BSize = GD_ReadByte(DecodeCtx))
	{
		if (GD_DecoderAdvance(DecodeCtx, BSize) != GD_OK)
			return;
	}
}

static GD_ERR
GD_CreateBlock(GD_DECODE_CONTEXT* DecodeCtx, BYTE BSize, GD_DataBlock** OutputBlock)
{
	GD_DataBlock* Block = malloc(sizeof(GD_DataBlock));

	if (!Block)
		return GD_NOMEM;

	if (GD_ReadBytes(DecodeCtx, Block->Data, BSize) != BSize)
	{
		free(Block);
		return GD_NOT_ENOUGH_DATA;
	}

	Block->EffectiveSize = BSize;
	*OutputBlock = Block;

	return GD_OK;
}

static GD_ERR
GD_BuildBlockList(GD_DECODE_CONTEXT* DecodeCtx, GD_DataBlockList* List)
{
	List->Head = NULL;
	List->Tail = NULL;
	List->BlockCount = 0;

	for (BYTE BSize = GD_ReadByte(DecodeCtx);
	          BSize != 0;
			  BSize = GD_ReadByte(DecodeCtx))
	{
		GD_DataBlock* Block;

		const GD_ERR ErrCode = GD_CreateBlock(DecodeCtx, BSize, &Block);

		if (ErrCode != GD_OK)
			return ErrCode;

		GD_BlockListAppend(List, Block);
	}

	return GD_OK;
}

static void
GD_BlockListAppend(GD_DataBlockList* List, GD_DataBlock* Block)
{
	if (!List->BlockCount)
	{
		Block->BLink = NULL;
		Block->FLink = NULL;

		List->Head = Block;
		List->Tail = Block;
	}
	else
	{
		Block->FLink = NULL;
		Block->BLink = List->Tail;
		List->Tail->FLink = Block;
		List->Tail = Block;
	}

	++List->BlockCount;
}

static GD_ERR
GD_ReadExtApplication(GD_DECODE_CONTEXT* DecodeCtx)
{
	//
	// Just ignore the extension if there is no callback routine
	//
	if (!ApplicationExtRoutines.RegisteredCount)
	{
		GD_IgnoreSubDataBlocks(DecodeCtx);
		return GD_OK;
	}

	GD_EXT_APPLICATION ExData;

	// Consume useless size byte
	GD_ReadByte(DecodeCtx);

	GD_ReadBytes(DecodeCtx, ExData.AppId, sizeof(ExData.AppId));
	GD_ReadBytes(DecodeCtx, ExData.AppAuth, sizeof(ExData.AppAuth));

	const GD_ERR ErrCode = GD_BuildBlockList(DecodeCtx, &ExData.Blocks);

	if (ErrCode != GD_OK)
		return ErrCode;

	//
	// Call registered callback routines
	//
	for (size_t i = 0; i < ApplicationExtRoutines.RegisteredCount; ++i)
	{
		if (ApplicationExtRoutines.Routines[i] != NULL)
			((GD_EXT_ROUTINE_APPLICATION)ApplicationExtRoutines.Routines[i])(&ExData);
	}

	// TODO: Free list

	return GD_OK;
}

static GD_ERR
GD_ReadExtPlainText(GD_DECODE_CONTEXT* DecodeCtx)
{
	GD_IgnoreSubDataBlocks(DecodeCtx);

	return GD_OK;
}

static GD_ERR
GD_ReadExtGraphics(GD_DECODE_CONTEXT* DecodeCtx)
{
	if (!GraphicsExtRoutines.RegisteredCount)
	{
		GD_IgnoreSubDataBlocks(DecodeCtx);
		return GD_OK;
	}

	for (size_t i = 0; i < GraphicsExtRoutines.RegisteredCount; ++i)
	{
		if (GraphicsExtRoutines.Routines[i] != NULL)
			((GD_EXT_ROUTINE_GRAPHICS)GraphicsExtRoutines.Routines[i])(NULL);
	}

	return GD_OK;
}

static GD_ERR
GD_ReadExtComment(GD_DECODE_CONTEXT* DecodeCtx)
{
	GD_IgnoreSubDataBlocks(DecodeCtx);

	return GD_OK;
}

static GD_ERR
GD_ReadExtension(GD_DECODE_CONTEXT* DecodeCtx)
{
	switch (GD_ReadByte(DecodeCtx))
	{
		case EXT_LABEL_APPLICATION: return GD_ReadExtApplication(DecodeCtx);
		case EXT_LABEL_PLAINTEXT: return GD_ReadExtPlainText(DecodeCtx);
		case EXT_LABEL_GRAPHICS: return GD_ReadExtGraphics(DecodeCtx);
		case EXT_LABEL_COMMENT: return GD_ReadExtComment(DecodeCtx);

		default: return GD_UNEXPECTED_DATA;
	}
}

GD_ERR
GD_RegisterExRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine)
{
	GD_EXT_ROUTINES* Routines = NULL;

	switch (RoutineType)
	{
		case GD_APPLICATION: Routines = &ApplicationExtRoutines; break;
		case GD_GRAPHICS:    Routines = &GraphicsExtRoutines;    break;
		case GD_PLAINTEXT:   Routines = &PlaintextExtRoutines;   break;
		case GD_COMMENT:     Routines = &CommentExtRoutines;     break;

		default:
			return GD_UNEXPECTED_DATA;
	}

	if (Routines->RegisteredCount >= MAX_REGISTERED_ROUTINES)
		return GD_MAX_REGISTERED_ROUTINE;

	Routines->Routines[Routines->RegisteredCount++] = UserRoutine;

	return GD_OK;
}

void
GD_ClearExRoutines(GD_EXTENSION_TYPE RoutineType)
{
	GD_EXT_ROUTINES* Routines = NULL;

	switch (RoutineType)
	{
		case GD_APPLICATION: Routines = &ApplicationExtRoutines; break;
		case GD_GRAPHICS:    Routines = &GraphicsExtRoutines;    break;
		case GD_PLAINTEXT:   Routines = &PlaintextExtRoutines;   break;
		case GD_COMMENT:     Routines = &CommentExtRoutines;     break;

		default: return;
	}

	for (size_t i = 0; i < Routines->RegisteredCount; ++i)
		Routines->Routines[i] = NULL;

	Routines->RegisteredCount = 0;
}

void
GGD_ClearAllExRoutines()
{
	GD_ClearExRoutines(GD_APPLICATION);
	GD_ClearExRoutines(GD_GRAPHICS);
	GD_ClearExRoutines(GD_PLAINTEXT);
	GD_ClearExRoutines(GD_COMMENT);
}

void
GD_UnregisterExRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine)
{
	GD_EXT_ROUTINES* Routines = NULL;

	switch (RoutineType)
	{
		case GD_APPLICATION: Routines = &ApplicationExtRoutines; break;
		case GD_GRAPHICS:    Routines = &GraphicsExtRoutines;    break;
		case GD_PLAINTEXT:   Routines = &PlaintextExtRoutines;   break;
		case GD_COMMENT:     Routines = &CommentExtRoutines;     break;

		default:
			return;
	}

	//
	// Find and shift
	//
	for (size_t i = 0; i < Routines->RegisteredCount; ++i)
	{
		if (Routines->Routines[i] == UserRoutine)
		{
			for (size_t j = i; j < Routines->RegisteredCount - 1; ++j)
				Routines->Routines[j] = Routines->Routines[j + 1];

			--Routines->RegisteredCount;
			break;
		}
	}
}

static GD_GIF_HANDLE
GD_DecodeInternal(GD_DECODE_CONTEXT* DecodeCtx, GD_ERR* ErrorCode)
{
	//
	// Allocate the GIF structure
	//
	GD_GIF_HANDLE Gif = malloc(sizeof(GD_GIF));

	if (!Gif)
	{
		*ErrorCode = GD_NOMEM;
		return NULL;
	}

	//
	// Verify header's signature and version
	//
	*ErrorCode = GD_ValidateHeader(DecodeCtx, &Gif->Version);

	if (*ErrorCode != GD_OK)
	{
		free(Gif);
		return NULL;
	}

	//
	// Read Logical Screen Descriptor
	//
	*ErrorCode = GD_ReadScreenDescriptor(DecodeCtx, &Gif->ScreenDesc);

	if (*ErrorCode != GD_OK)
	{
		free(Gif);
		return NULL;
	}

	//
	// Read the GCT immediately after if bit is set in LOGICAL_SCREEN_DESCRIPTOR.PackedFields
	//
	if (LSD_HAS_GCT(Gif->ScreenDesc.PackedFields))
	{
		*ErrorCode = GD_ReadGlobalColorTable(DecodeCtx, &Gif->GlobalColorTable, Gif->ScreenDesc.PackedFields);

		if (*ErrorCode != GD_OK)
		{
			free(Gif);
			return NULL;
		}
	}

	while (GD_TRUE)
	{
		const BYTE b = GD_ReadByte(DecodeCtx);

		switch (b)
		{
			case BLOCK_INTRODUCER_EXT:
				GD_ReadExtension(DecodeCtx);
				break;

			case BLOCK_INTRODUCER_IMG:
				break;

			case TRAILER:
				return Gif;

			default:
				free(Gif);
				*ErrorCode = GD_UNEXPECTED_DATA;
				return NULL;
		}
	}
}

GD_GIF_HANDLE
GD_OpenGif(const char* Path, GD_ERR* ErrorCode, size_t* ErrorBytePos)
{
	GD_DECODE_CONTEXT DecodeCtx;

	*ErrorCode = GD_InitDecodeContextStream(&DecodeCtx, Path);

	if (*ErrorCode != GD_OK)
		return NULL;

	GD_GIF_HANDLE Gif = GD_DecodeInternal(&DecodeCtx, ErrorCode);

	if (!Gif && ErrorBytePos)
		*ErrorBytePos = DecodeCtx.DataStreamOffset;

	free(DecodeCtx.StreamFd);

	return Gif;
}

GD_GIF_HANDLE
GD_FromMemory(const void* Buffer, size_t BufferSize, GD_ERR* ErrorCode, size_t* ErrorBytePos)
{
	GD_DECODE_CONTEXT DecodeCtx;

	*ErrorCode = GD_InitDecodeContextMemory(&DecodeCtx, Buffer, BufferSize);

	if (*ErrorCode != GD_OK)
		return NULL;

	GD_GIF_HANDLE Gif = GD_DecodeInternal(&DecodeCtx, ErrorCode);

	if (!Gif && ErrorBytePos)
		*ErrorBytePos = DecodeCtx.DataStreamOffset;

	return Gif;
}

void GD_CloseGif(GD_GIF_HANDLE Gif)
{
	free(Gif);
}
