#include "gd.h"
#include <stdlib.h>


#define SIGNATURE_SIZE 3
#define VERSION_SIZE   3
#define HEADER_SIZE (SIGNATURE_SIZE + VERSION_SIZE)


#define DESCRIPTOR_TABLE_PRESENT(DescriptorFields) ((DescriptorFields) & 0x80)
#define DESCRIPTOR_TABLE_SIZE(DescriptorFields)    (2 << ((DescriptorFields) & 7))


#define LZW_MAX_CODEWIDTH 12
#define LZW_INVALID_CODE 0xFFFF


// TODO: 1. Add support for custom extensions
//       2. Fix integer overflows and possible OOB access
//       3. Fuzzin' time !



static GD_ERR
GD_InitDecodeContextStream(GD_DECODE_CONTEXT* Decoder, const char* Path)
{
	FILE* fd = fopen(Path, "rb");

	if (!fd)
		return GD_NOTFOUND;

	Decoder->StreamFd = fd;
	Decoder->SourceMode = GD_FROM_STREAM;
	Decoder->SourceEOF = GD_FALSE;
	Decoder->DataStreamOffset = 0;

	//
	// Unused members
	//
	Decoder->MemoryBuffer = NULL;
	Decoder->MemoryBufferSize = 0;

	//
	// Load a first chunk
	//
	GD_DecoderLoadChunk(Decoder);

	return GD_OK;
}

static GD_ERR
GD_InitDecodeContextMemory(GD_DECODE_CONTEXT* Decoder, const void* Buffer, size_t BufferSize)
{
	Decoder->StreamFd = NULL;

	Decoder->SourceMode = GD_FROM_MEMORY;
	Decoder->MemoryBuffer = (GD_BYTE*)Buffer;
	Decoder->MemoryBufferSize = BufferSize;
	Decoder->SourceBeg = Decoder->MemoryBuffer;
	Decoder->SourceEnd = Decoder->MemoryBuffer + BufferSize;
	Decoder->SourceEOF = GD_FALSE;
	Decoder->DataStreamOffset = 0;

	return GD_OK;
}

static void
GD_DecoderLoadChunk(GD_DECODE_CONTEXT* Decoder)
{
	const size_t BytesRead = fread(Decoder->StreamChunk, 1, CHUNK_SIZE, Decoder->StreamFd);

	Decoder->SourceBeg = Decoder->StreamChunk;
	Decoder->SourceEnd = Decoder->SourceBeg + BytesRead;
}

static GD_BOOL
GD_DecoderCanRead(GD_DECODE_CONTEXT* Decoder)
{
	if (Decoder->SourceBeg < Decoder->SourceEnd)
		return GD_TRUE;

	if (Decoder->SourceMode == GD_FROM_STREAM)
	{
		//
		// Try loading a new chunk from stream
		//
		GD_DecoderLoadChunk(Decoder);

		Decoder->SourceEOF = Decoder->SourceBeg >= Decoder->SourceEnd;
		return !Decoder->SourceEOF;
	}

	Decoder->SourceEOF = GD_TRUE;

	return GD_FALSE;
}

static GD_BYTE
GD_ReadByte(GD_DECODE_CONTEXT* Decoder)
{
	if (GD_DecoderCanRead(Decoder))
	{
		++Decoder->DataStreamOffset;
		return *Decoder->SourceBeg++;
	}

	return 0;
}

static size_t
GD_ReadBytes(GD_DECODE_CONTEXT* Decoder, GD_BYTE* Buffer, size_t Count)
{
	size_t i;

	for (i = 0; i < Count && !Decoder->SourceEOF; ++i)
		Buffer[i] = GD_ReadByte(Decoder);

	return i;
}

static GD_WORD
GD_ReadWord(GD_DECODE_CONTEXT* Decoder)
{
	GD_BYTE HiByte = GD_ReadByte(Decoder);
	GD_BYTE LoByte = GD_ReadByte(Decoder);

	//
	// Convert to little-endian (high byte becomes low bytes, and vice-versa)
	//
	return HiByte | (LoByte << 8);
}

static GD_ERR
GD_DecoderAdvance(GD_DECODE_CONTEXT* Decoder, size_t BytesCount)
{
	if (Decoder->SourceMode == GD_FROM_MEMORY)
	{
		Decoder->SourceBeg += BytesCount;
		Decoder->DataStreamOffset += BytesCount;

		return GD_OK;
	}

	//
	// Advance from file
	//
	const long SeekTo = (long)(Decoder->DataStreamOffset + BytesCount);
	const int ret = fseek(Decoder->StreamFd, SeekTo, SEEK_SET);

	if (ret != 0)
		return GD_IOFAIL;

	Decoder->DataStreamOffset += BytesCount;
	GD_DecoderLoadChunk(Decoder);

	return GD_OK;
}

static GD_ERR
GD_ValidateHeader(GD_DECODE_CONTEXT* Decoder, GD_GIF_VERSION* Version)
{
	GD_BYTE Header[HEADER_SIZE];

	//
	// Read first 6 bytes of the data stream
	//
	size_t BytesRead = GD_ReadBytes(Decoder, Header, HEADER_SIZE);

	if (BytesRead != HEADER_SIZE)
		return GD_NOT_ENOUGH_DATA;

	//
	// Must match with one of the two versions
	//
	const char* Version1Header = "GIF89a";
	const char* Version2Header = "GIF87a";

	for (GD_BYTE i = 0; i < HEADER_SIZE; ++i)
	{
		if (Header[i] != (GD_BYTE)Version1Header[i] &&
		    Header[i] != (GD_BYTE)Version2Header[i])
		{
			return GD_INVALID_SIGNATURE;
		}
	}

	*Version = (Header[4] == '9') ? GD_GIF89A : GD_GIF87A;

	return GD_OK;
}


static GD_ERR
GD_ReadScreenDescriptor(GD_DECODE_CONTEXT* Decoder, GD_LOGICAL_SCREEN_DESCRIPTOR* ScrDescriptor)
{
	ScrDescriptor->LogicalWidth     = GD_ReadWord(Decoder);
	ScrDescriptor->LogicalHeight    = GD_ReadWord(Decoder);
	ScrDescriptor->PackedFields     = GD_ReadByte(Decoder);
	ScrDescriptor->BgColorIndex     = GD_ReadByte(Decoder);
	ScrDescriptor->PixelAspectRatio = GD_ReadByte(Decoder);

	return GD_OK;
}

static GD_ERR
GD_ReadColorTable(GD_DECODE_CONTEXT* Decoder, GD_COLOR_TABLE* Table, GD_BYTE ScrDescriptorFields)
{
	Table->Count = DESCRIPTOR_TABLE_SIZE(ScrDescriptorFields);

	for (size_t i = 0; i < Table->Count; ++i)
	{
		Table->Internal[i].r = GD_ReadByte(Decoder);
		Table->Internal[i].g = GD_ReadByte(Decoder);
		Table->Internal[i].b = GD_ReadByte(Decoder);
	}

	return GD_OK;
}

static void
GD_IgnoreSubDataBlocks(GD_DECODE_CONTEXT* Decoder)
{
	for (GD_BYTE BSize = GD_ReadByte(Decoder);
	          BSize != 0;
			  BSize = GD_ReadByte(Decoder))
	{
		if (GD_DecoderAdvance(Decoder, BSize) != GD_OK)
			return;
	}
}

static GD_ERR
GD_BlocksToLinearBuffer(GD_DECODE_CONTEXT* Decoder, GD_BYTE** Buffer, GD_DWORD* BufferSize)
{
	*Buffer = NULL;
	*BufferSize = 0;

	for (GD_BYTE BSize = GD_ReadByte(Decoder);
		 BSize != 0;
		 BSize = GD_ReadByte(Decoder))
	{
		//
		// Resize buffer
		//
		*BufferSize += BSize;
		void* tmp = realloc(*Buffer, *BufferSize);

		if (!tmp)
		{
			free(*Buffer);
			return GD_NOMEM;
		}
		else
		{
			*Buffer = (GD_BYTE*)tmp;
		}

		//
		// Append new block
		//
		if (GD_ReadBytes(Decoder, *Buffer + (*BufferSize - BSize), BSize) != BSize)
		{

			free(*Buffer);
			return GD_IOFAIL;
		}
	}

	return GD_OK;
}

static GD_ERR
GD_CreateBlock(GD_DECODE_CONTEXT* Decoder, GD_BYTE BSize, GD_DataBlock** OutputBlock)
{
	GD_DataBlock* Block = malloc(sizeof(GD_DataBlock));

	if (!Block)
		return GD_NOMEM;

	if (GD_ReadBytes(Decoder, Block->Data, BSize) != BSize)
	{
		free(Block);
		return GD_NOT_ENOUGH_DATA;
	}

	Block->EffectiveSize = BSize;
	*OutputBlock = Block;

	return GD_OK;
}

static GD_ERR
GD_BlockListBuild(GD_DECODE_CONTEXT* Decoder, GD_DataBlockList* List)
{
	if (!Decoder || !List)
		return GD_UNEXPECTED_DATA;

	List->Head = NULL;
	List->Tail = NULL;
	List->BlockCount = 0;

	for (GD_BYTE BSize = GD_ReadByte(Decoder);
	          BSize != 0;
			  BSize = GD_ReadByte(Decoder))
	{
		GD_DataBlock* Block;

		const GD_ERR ErrCode = GD_CreateBlock(Decoder, BSize, &Block);

		if (ErrCode != GD_OK)
			return ErrCode;

		GD_BlockListAppend(List, Block);
	}

	return GD_OK;
}

static void
GD_BlockListDestroy(GD_DataBlockList* List)
{
	if (!List)
		return;

	GD_DataBlock* Next = NULL;
	GD_DataBlock* Curr = List->Head;

	while (List->BlockCount--)
	{
		Next = Curr->FLink;
		free(Curr);
		Curr = Next;
	}
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
GD_ReadExtApplication(GD_DECODE_CONTEXT* Decoder)
{
	if (!ApplicationExtRoutines.RegisteredCount)
	{
		//
		// Just ignore the extension if there is no callback routine
		//
		GD_IgnoreSubDataBlocks(Decoder);
		return GD_OK;
	}

	GD_EXT_APPLICATION ExData;

	// Consume useless size byte
	GD_ReadByte(Decoder);

	GD_ReadBytes(Decoder, ExData.AppId, sizeof(ExData.AppId));
	GD_ReadBytes(Decoder, ExData.AppAuth, sizeof(ExData.AppAuth));

	const GD_ERR ErrCode = GD_BlockListBuild(Decoder, &ExData.Blocks);

	if (ErrCode != GD_OK)
		return ErrCode;

	for (size_t i = 0; i < ApplicationExtRoutines.RegisteredCount; ++i)
	{
		//
		// Call registered callback routines
		//
		if (ApplicationExtRoutines.Routines[i] != NULL)
			((GD_EXT_ROUTINE_APPLICATION)ApplicationExtRoutines.Routines[i])(&ExData);
	}

	GD_BlockListDestroy(&ExData.Blocks);

	return GD_OK;
}

static GD_ERR
GD_ReadExtPlainText(GD_DECODE_CONTEXT* Decoder)
{
	if (!PlaintextExtRoutines.RegisteredCount)
	{
		GD_IgnoreSubDataBlocks(Decoder);
		return GD_OK;
	}

	GD_EXT_PLAINTEXT ExData;

	// Consume useless size byte
	GD_ReadByte(Decoder);

	ExData.GridPositionLeft = GD_ReadWord(Decoder);
	ExData.GridPositionTop  = GD_ReadWord(Decoder);
	ExData.GridWidth = GD_ReadWord(Decoder);
	ExData.GridHeight = GD_ReadWord(Decoder);
	ExData.CharCellWidth = GD_ReadByte(Decoder);
	ExData.CharCellHeight = GD_ReadByte(Decoder);
	ExData.FgColorIndex = GD_ReadByte(Decoder);
	ExData.BgColorIndex = GD_ReadByte(Decoder);

	const GD_ERR ErrCode = GD_BlockListBuild(Decoder, &ExData.Blocks);

	if (ErrCode != GD_OK)
		return ErrCode;

	for (size_t i = 0; i < PlaintextExtRoutines.RegisteredCount; ++i)
	{
		if (PlaintextExtRoutines.Routines[i] != NULL)
			((GD_EXT_ROUTINE_PLAINTEXT)PlaintextExtRoutines.Routines[i])(&ExData);
	}

	GD_BlockListDestroy(&ExData.Blocks);

	return GD_OK;
}

static GD_ERR
GD_ReadExtGraphics(GD_DECODE_CONTEXT* Decoder)
{
	if (!GraphicsExtRoutines.RegisteredCount)
	{
		GD_IgnoreSubDataBlocks(Decoder);
		return GD_OK;
	}

	GD_EXT_GRAPHICS ExData;

	// Consume useless size byte
	GD_ReadByte(Decoder);

	ExData.PackedFields = GD_ReadByte(Decoder);
	ExData.DelayTime = GD_ReadWord(Decoder);
	ExData.TransparentColorIndex = GD_ReadByte(Decoder);

	for (size_t i = 0; i < GraphicsExtRoutines.RegisteredCount; ++i)
	{
		if (GraphicsExtRoutines.Routines[i] != NULL)
			((GD_EXT_ROUTINE_GRAPHICS)GraphicsExtRoutines.Routines[i])(&ExData);
	}

	// Consume block terminator
	GD_ReadByte(Decoder);

	return GD_OK;
}

static GD_ERR
GD_ReadExtComment(GD_DECODE_CONTEXT* Decoder)
{
	if (!CommentExtRoutines.RegisteredCount)
	{
		GD_IgnoreSubDataBlocks(Decoder);
		return GD_OK;
	}

	GD_EXT_COMMENT ExData;

	const GD_ERR ErrCode = GD_BlockListBuild(Decoder, &ExData.Blocks);

	if (ErrCode != GD_OK)
		return ErrCode;

	for (size_t i = 0; i < CommentExtRoutines.RegisteredCount; ++i)
	{
		if (CommentExtRoutines.Routines[i] != NULL)
			((GD_EXT_ROUTINE_COMMENT)CommentExtRoutines.Routines[i])(&ExData);
	}

	GD_BlockListDestroy(&ExData.Blocks);

	return GD_OK;
}

static GD_ERR
GD_ReadExtension(GD_DECODE_CONTEXT* Decoder)
{
	switch (GD_ReadByte(Decoder))
	{
		case EXT_LABEL_APPLICATION: return GD_ReadExtApplication(Decoder);
		case EXT_LABEL_PLAINTEXT: return GD_ReadExtPlainText(Decoder);
		case EXT_LABEL_GRAPHICS: return GD_ReadExtGraphics(Decoder);
		case EXT_LABEL_COMMENT: return GD_ReadExtComment(Decoder);

		default:
			return GD_UNEXPECTED_DATA;
	}
}

typedef struct LZW_TABLE_ENTRY
{
	GD_WORD Length;
	GD_WORD Prefix;
	GD_BYTE Suffix;
} LZW_TABLE_ENTRY;

typedef struct LZW_CONTEXT
{
	LZW_TABLE_ENTRY Dictionary[1 << (LZW_MAX_CODEWIDTH + 1)];
	GD_WORD DictIndex;
	GD_WORD DictCount;
	GD_BYTE CodeWidth;
	GD_WORD CodeClear;
	GD_WORD CodeBreak;
} LZW_CONTEXT;


static GD_WORD
GD_LzwUnpackCode(GD_BYTE LzwCodeWidth, GD_BYTE** CompressedData, GD_DWORD* CompressedDataLength, GD_WORD* Mask)
{
	///
	/// For example, 10-bits code 11'1001'0100 was packed into:
	///      8-bits: 1001'0100
	///      8-bits: xxxx'xx11
	///
	/// This unpacks the 16-bits word: 1001'0100'xxxx'xx11
	/// Into the original value 11'1001'0100
	///

	// TODO: this can very clearly be optimized:
	//       Code = *CompressedData
	//       if (LzwCodeWidth <= 8) return Code
	//       ++CompressedData, --CompressedDataLength
	//       Code |= ((*CompressedData & (remaining byte mask)) << 8) ?
	//

	GD_WORD Code = 0;

	for (GD_BYTE i = 0; i <= LzwCodeWidth; ++i)
	{
		GD_BOOL bit = (**CompressedData & *Mask) ? GD_TRUE : GD_FALSE;

		*Mask <<= 1;

		if (*Mask == 0x100)
		{
			// We consumed every bit of the first byte, move to next one
			*Mask = 1;
			++*CompressedData;
			--CompressedDataLength;
		}

		Code |= (bit << i);
	}

	return Code;
}

static void
GD_LzwInitContext(LZW_CONTEXT* Lzw, GD_BYTE CodeWidth)
{
	Lzw->CodeWidth = CodeWidth;
	Lzw->DictCount = (1 << CodeWidth);
	Lzw->CodeClear = (1 << CodeWidth);
	Lzw->CodeBreak = (1 << CodeWidth) + 1;

	for (Lzw->DictIndex = 0; Lzw->DictIndex < Lzw->DictCount; ++Lzw->DictIndex)
	{
		Lzw->Dictionary[Lzw->DictIndex].Length = 1;
		Lzw->Dictionary[Lzw->DictIndex].Prefix = LZW_INVALID_CODE;
		Lzw->Dictionary[Lzw->DictIndex].Suffix = Lzw->DictIndex;
	}

	// Skip clear and end codes
	Lzw->DictIndex += 2;
}

static GD_ERR
GD_LzwDecompressRaster(GD_BYTE InitialCodeWidth,
					   GD_BYTE* CompressedData,
					   GD_DWORD CompressedDataLength,
					   GD_BYTE* DecompressedData)
{
	if (InitialCodeWidth > LZW_MAX_CODEWIDTH)
		return GD_UNEXPECTED_DATA;

	LZW_CONTEXT Lzw;

	// Normally GIF should have a clear code at the start of the raster but let's make sure anyway
	GD_LzwInitContext(&Lzw, InitialCodeWidth);

	GD_WORD PrevCode = LZW_INVALID_CODE;

	GD_WORD Mask = 1;

	while (CompressedDataLength)
	{
		GD_WORD Code = GD_LzwUnpackCode(Lzw.CodeWidth, &CompressedData, &CompressedDataLength, &Mask);

		if (Code == Lzw.CodeClear)
		{
			GD_LzwInitContext(&Lzw, InitialCodeWidth);
			PrevCode = 0xFFFF;
			continue;
		}
		else if (Code == Lzw.CodeBreak)
			break;

		if (PrevCode != LZW_INVALID_CODE && Lzw.CodeWidth < LZW_MAX_CODEWIDTH)
		{
			int ptr = (Code == Lzw.DictIndex) ? PrevCode : Code;

			while (Lzw.Dictionary[ptr].Prefix != 0xFFFF)
				ptr = Lzw.Dictionary[ptr].Prefix;

			Lzw.Dictionary[Lzw.DictIndex].Suffix = Lzw.Dictionary[ptr].Suffix;
			Lzw.Dictionary[Lzw.DictIndex].Prefix = PrevCode;
			Lzw.Dictionary[Lzw.DictIndex].Length = Lzw.Dictionary[PrevCode].Length + 1;
			++Lzw.DictIndex;

			if (Lzw.DictIndex == (1 << (Lzw.CodeWidth + 1)) && Lzw.CodeWidth < 11)
			{
				++Lzw.CodeWidth;
				Lzw.DictCount = 1 << Lzw.CodeWidth;
			}
		}

		PrevCode = Code;

		GD_WORD Copied = Lzw.Dictionary[Code].Length;

		while (Code != LZW_INVALID_CODE)
		{
			const LZW_TABLE_ENTRY* Entry = &Lzw.Dictionary[Code];

			DecompressedData[Entry->Length - 1] = Entry->Suffix;

			// Prevent infinite loop
			if (Entry->Prefix == Code)
				return GD_UNEXPECTED_DATA;

			Code = Entry->Prefix;
		}

		DecompressedData += Copied;
	}

	return GD_OK;
}

static GD_ERR
GD_ProcessImageRaster(GD_DECODE_CONTEXT* Decoder, GD_GIF_HANDLE Gif, GD_IMAGE_DESCRIPTOR* ImageDescriptor, GD_COLOR_TABLE* ActiveTable)
{
	const GD_BYTE LzwCodeWidth = GD_ReadByte(Decoder);

	GD_BYTE* CompressedData = NULL;
	GD_DWORD CompressedDataLength = 0;
	GD_ERR ErrorCode = GD_BlocksToLinearBuffer(Decoder, &CompressedData, &CompressedDataLength);

	if (ErrorCode != GD_OK)
		return ErrorCode;

	const GD_DWORD DecompressedDataLength = ImageDescriptor->Height * ImageDescriptor->Width;
	GD_BYTE* DecompressedData = malloc(sizeof(GD_BYTE) * DecompressedDataLength);

	ErrorCode = GD_LzwDecompressRaster(LzwCodeWidth, CompressedData, CompressedDataLength, DecompressedData);

	free(CompressedData);
	free(DecompressedData);

	return ErrorCode;
}

static GD_ERR
GD_ReadImage(GD_DECODE_CONTEXT* Decoder, GD_GIF_HANDLE Gif)
{
	GD_IMAGE_DESCRIPTOR ImageDescriptor;

	ImageDescriptor.PositionLeft = GD_ReadWord(Decoder);
	ImageDescriptor.PositionTop  = GD_ReadWord(Decoder);
	ImageDescriptor.Width        = GD_ReadWord(Decoder);
	ImageDescriptor.Height       = GD_ReadWord(Decoder);
	ImageDescriptor.PackedFields = GD_ReadByte(Decoder);

	if (DESCRIPTOR_TABLE_PRESENT(ImageDescriptor.PackedFields))
	{
		GD_COLOR_TABLE LocalColorTable;
		GD_ReadColorTable(Decoder, &LocalColorTable, ImageDescriptor.PackedFields);
		GD_ProcessImageRaster(Decoder, Gif, &ImageDescriptor, &LocalColorTable);
	}
	else if (DESCRIPTOR_TABLE_PRESENT(Gif->ScreenDesc.PackedFields))
	{
		GD_ProcessImageRaster(Decoder, Gif, &ImageDescriptor, &Gif->GlobalColorTable);
	}
	else
	{
		return GD_NO_COLOR_TABLE;
	}

	return GD_OK;
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
GD_DecodeInternal(GD_DECODE_CONTEXT* Decoder, GD_ERR* ErrorCode)
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
	*ErrorCode = GD_ValidateHeader(Decoder, &Gif->Version);

	if (*ErrorCode != GD_OK)
	{
		free(Gif);
		return NULL;
	}

	//
	// Read Logical Screen Descriptor
	//
	*ErrorCode = GD_ReadScreenDescriptor(Decoder, &Gif->ScreenDesc);

	if (*ErrorCode != GD_OK)
	{
		free(Gif);
		return NULL;
	}

	//
	// Read the GCT immediately after if bit is set in LOGICAL_SCREEN_DESCRIPTOR.PackedFields
	//
	if (DESCRIPTOR_TABLE_PRESENT(Gif->ScreenDesc.PackedFields))
		GD_ReadColorTable(Decoder, &Gif->GlobalColorTable, Gif->ScreenDesc.PackedFields);

	//
	// Process blocks
	//
	GD_BYTE b;
	while ((b = GD_ReadByte(Decoder)) != TRAILER)
	{
		switch (b)
		{
			case BLOCK_INTRODUCER_EXT:
				*ErrorCode = GD_ReadExtension(Decoder);
				break;

			case BLOCK_INTRODUCER_IMG:
				*ErrorCode = GD_ReadImage(Decoder, Gif);
				break;

			default:
				*ErrorCode = GD_UNEXPECTED_DATA;
				break;
		}

		if (*ErrorCode != GD_OK)
		{
			free(Gif);
			return NULL;
		}
	}

	return Gif;
}

GD_GIF_HANDLE
GD_OpenGif(const char* Path, GD_ERR* ErrorCode, size_t* ErrorBytePos)
{
	GD_DECODE_CONTEXT Decoder;

	// TODO: Close file handle
	*ErrorCode = GD_InitDecodeContextStream(&Decoder, Path);

	if (*ErrorCode != GD_OK)
		return NULL;

	GD_GIF_HANDLE Gif = GD_DecodeInternal(&Decoder, ErrorCode);

	if (!Gif && ErrorBytePos)
		*ErrorBytePos = Decoder.DataStreamOffset;

	free(Decoder.StreamFd);

	return Gif;
}

GD_GIF_HANDLE
GD_FromMemory(const void* Buffer, size_t BufferSize, GD_ERR* ErrorCode, size_t* ErrorBytePos)
{
	GD_DECODE_CONTEXT Decoder;

	*ErrorCode = GD_InitDecodeContextMemory(&Decoder, Buffer, BufferSize);

	if (*ErrorCode != GD_OK)
		return NULL;

	GD_GIF_HANDLE Gif = GD_DecodeInternal(&Decoder, ErrorCode);

	if (!Gif && ErrorBytePos)
		*ErrorBytePos = Decoder.DataStreamOffset;

	return Gif;
}

void GD_CloseGif(GD_GIF_HANDLE Gif)
{
	free(Gif);
}

const char*
GD_ErrorAsString(GD_ERR Error)
{
	switch (Error) {
		case GD_OK: return "GD_OK";
		case GD_NOMEM: return "GD_NOMEM";
		case GD_IOFAIL: return "GD_IOFAIL";
		case GD_NOTFOUND: return "GD_NOTFOUND";
		case GD_NO_COLOR_TABLE: return "GD_NO_COLOR_TABLE";
		case GD_NOT_ENOUGH_DATA: return "GD_NOT_ENOUGH_DATA";
		case GD_UNEXPECTED_DATA: return "GD_UNEXPECTED_DATA";
		case GD_INVALID_SIGNATURE: return "GD_INVALID_SIGNATURE";
		case GD_INVALID_IMG_INDEX: return "GD_INVALID_IMG_INDEX";
		case GD_MAX_REGISTERED_ROUTINE: return "GD_MAX_REGISTERED_ROUTINE";

		default:
			return "<unknown error code>";
	}
}