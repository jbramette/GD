#ifndef GIFDEC_GIFDEC_H
#define GIFDEC_GIFDEC_H


#include <stdint.h>
#include <stddef.h>
#include <stdio.h>


#define GCT_MAX_SIZE 256

#define EXT_LABEL_PLAINTEXT   0x01
#define EXT_LABEL_GRAPHICS    0xF9
#define EXT_LABEL_COMMENT     0xFE
#define EXT_LABEL_APPLICATION 0xFF

#define BLOCK_INTRODUCER_EXT 0x21
#define BLOCK_INTRODUCER_IMG 0x2C
#define TRAILER              0x3B



/////////////////////////////////////////////////////////////////
///               GD LIBRARY TYPE DEFINITIONS                  //
/////////////////////////////////////////////////////////////////

typedef uint8_t  BYTE;
typedef uint16_t WORD;


typedef enum GD_BOOL
{
	GD_FALSE,
	GD_TRUE
} GD_BOOL;


typedef enum GD_ERR
{
	GD_OK,
	GD_NOMEM,
	GD_IOFAIL,
	GD_NOTFOUND,
	GD_NOT_ENOUGH_DATA,
	GD_UNEXPECTED_DATA,
	GD_INVALID_SIGNATURE,
	GD_INVALID_IMG_INDEX,
	GD_MAX_REGISTERED_ROUTINE
} GD_ERR;




/////////////////////////////////////////////////////////////////
///                   EXTENSION SUPPORT                        //
/////////////////////////////////////////////////////////////////

#define SUB_BLOCK_MAX_SIZE 255

typedef struct GD_DataSubBlock
{
	struct GD_DataSubBlock* BLink;
	struct GD_DataSubBlock* FLink;

	const BYTE Data[SUB_BLOCK_MAX_SIZE];
	BYTE EffectiveSize;

} GD_DataSubBlock;


typedef struct GD_SubBlockList
{
	GD_DataSubBlock* Head;
	GD_DataSubBlock* Tail;

	size_t BlockCount;

} GD_SubBlockList;


typedef struct GD_EXT_GRAPHICS
{
	BYTE PackedFields;
	WORD DelayTime;
	BYTE TransparentColorIndex;
} GD_EXT_GRAPHICS;


typedef struct GD_EXT_COMMENT
{
	GD_DataSubBlock Data;
} GD_EXT_COMMENT;


typedef struct GD_EXT_PLAINTEXT
{
	WORD GridPositionLeft;
	WORD GridPositionTop;
	WORD GridWidth;
	WORD GridHeight;
	BYTE CharCellWidth;
	BYTE CharCellHeight;
	BYTE FgColorIndex;
	BYTE BgColorIndex;

	GD_DataSubBlock Data;

} GD_EXT_PLAINTEXT;


typedef struct GD_EXT_APPLICATION
{
	BYTE AppId[8];
	BYTE AppAuth[3];

	GD_DataSubBlock Data;

} GD_EXT_APPLICATION;


typedef enum GD_EXTENSION_TYPE
{
	GD_APPLICATION = 0,
	GD_PLAINTEXT   = 1,
	GD_COMMENT     = 2

} GD_EXTENSION_TYPE;


typedef void(*GD_EXT_ROUTINE_APPLICATION)(GD_EXT_APPLICATION* Extension);
typedef void(*GD_EXT_ROUTINE_PLAINTEXT)(GD_EXT_PLAINTEXT* Extension);
typedef void(*GD_EXT_ROUTINE_COMMENT)(GD_EXT_COMMENT* Extension);

#define MAX_REGISTERED_ROUTINES 4

GD_EXT_ROUTINE_APPLICATION AppExtRoutines[MAX_REGISTERED_ROUTINES];
GD_EXT_ROUTINE_APPLICATION CommentExtRoutines[MAX_REGISTERED_ROUTINES];
GD_EXT_ROUTINE_APPLICATION PlaintextExtRoutines[MAX_REGISTERED_ROUTINES];



/////////////////////////////////////////////////////////////////
///                    GIF DEFINITIONS                         //
/////////////////////////////////////////////////////////////////
typedef enum GD_GIF_VERSION
{
	GD_GIF87A,
	GD_GIF89A
} GD_GIF_VERSION;


typedef struct GD_LOGICAL_SCREEN_DESCRIPTOR
{
	WORD LogicalWidth;
	WORD LogicalHeight;
	BYTE PackedFields;
	BYTE BgColorIndex;
	BYTE PixelAspectRatio;
} GD_LOGICAL_SCREEN_DESCRIPTOR;


typedef struct GD_GIF_COLOR
{
	BYTE r;
	BYTE g;
	BYTE b;
} GD_GIF_COLOR;


typedef struct GD_GLOBAL_COLOR_TABLE
{
	GD_GIF_COLOR Internal[GCT_MAX_SIZE];
	size_t Count;
} GD_GLOBAL_COLOR_TABLE;




/////////////////////////////////////////////////////////////////
///                      GD PUBLIC API                         //
/////////////////////////////////////////////////////////////////

typedef struct GD_GIF
{
	GD_GIF_VERSION Version;
	GD_LOGICAL_SCREEN_DESCRIPTOR ScreenDesc;
	GD_GLOBAL_COLOR_TABLE GlobalColorTable;

} GD_GIF, *GD_GIF_HANDLE;


/// \brief Obtain a GIF handle from a file
/// \param Path GIF file path
/// \param ErrorCode
/// \param ErrorBytePos
/// \return
GD_GIF_HANDLE
GD_OpenGif(const char* Path, GD_ERR* ErrorCode, size_t* ErrorBytePos);

/// \brief Obtain a GIF handle from an already existing buffer
/// \param Buffer
/// \param BufferSize
/// \param ErrorCode
/// \param ErrorBytePos
/// \return
GD_GIF_HANDLE
GD_FromMemory(const void* Buffer, size_t BufferSize, GD_ERR* ErrorCode, size_t* ErrorBytePos);


/// \brief Close a GIF handle obtained by \ref GD_OpenGif or \ref GD_FromMemory
/// \param Gif
void
GD_CloseGif(GD_GIF_HANDLE Gif);


/// \brief Retrieve GIF version
/// \param Gif
/// \return
GD_GIF_VERSION
GD_GifVersion(GD_GIF_HANDLE Gif);


/// \brief Verify whether the GIF has a Global Color Table or not
/// \param Gif
/// \return
GD_BOOL
GD_HasGlobalColorTable(GD_GIF_HANDLE Gif);


/// \brief Get the background color used by the GIF
/// \param Gif
/// \return
GD_GIF_COLOR
GD_BackgroundColor(GD_GIF_HANDLE Gif);


/// \brief Retrieve the amount of images contained by the GIF
/// \param Gif
/// \return
size_t
GD_ImageCount(GD_GIF_HANDLE Gif);


/// \brief Get the decoded bytes of an image buffer
/// \param Gif
/// \param ImageIndex
/// \param Buffer
/// \param BufferSize
/// \return
GD_ERR
GD_ImageBuffer(GD_GIF_HANDLE Gif, size_t ImageIndex, BYTE** Buffer, size_t* BufferSize);

/// \brief Register a routine to be called when the decoder encounters an application, comment or plain text extension
/// \param RoutineType
/// \param UserRoutine
/// \return
GD_ERR
GD_RegisterRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine);

/// \brief Clear all routines for a certain extension type
/// \param RoutineType
/// \return
GD_ERR
GD_ClearRoutines(GD_EXTENSION_TYPE RoutineType);

/// \brief Clear all routines registered for every extension type
/// \return
GD_ERR
GD_ClearAllRoutines();

/// \brief Unregister a single routine by address
/// \param RoutineType
/// \param UserRoutine
/// \return
GD_ERR
GD_UnregisterRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine);




/////////////////////////////////////////////////////////////////
///                      GD PRIVATE API                        //
/////////////////////////////////////////////////////////////////

typedef enum GD_SOURCE_MODE
{
	GD_FROM_STREAM,
	GD_FROM_MEMORY
} GD_SOURCE_MODE;


#define CHUNK_SIZE 1024


typedef struct GD_DECODE_CONTEXT
{
	//
	// File descriptor and buffer used as a chunk when reading from a stream (ie: a file)
	//
	FILE*  StreamFd;
	BYTE   StreamChunk[CHUNK_SIZE];

	//
	// Pointer and size of the buffer used to decode from memory
	//
	BYTE*  MemoryBuffer;
	size_t MemoryBufferSize;

	//
	// Indicates whether the decoder use a stream or a buffer as a source input
	//
	GD_SOURCE_MODE SourceMode;

	//
	// Iterators on the source input, depending on the mode they can point to:
	//		- StreamChunk  for SourceMode == GD_FROM_STREAM
	//		- MemoryBuffer for SourceMode == GD_FROM_MEMORY
	//
	BYTE* SourceBeg;
	BYTE* SourceEnd;

	//
	// Indicates whether the source has reached EOF (buffer entirely read for memory mode)
	//
	GD_BOOL SourceEOF;

	//
	// Current position of the decoder in the data stream, useful for errors
	//
	size_t DataStreamOffset;

} GD_DECODE_CONTEXT;


/// \brief Load a new chunk from the input file's stream
/// \param DecodeCtx
static void
GD_DecoderLoadChunk(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief Initialize the decoding context when reading from a file
/// \param DecodeCtx
/// \param Path
/// \return
static GD_ERR
GD_InitDecodeContextStream(GD_DECODE_CONTEXT* DecodeCtx, const char* Path);

/// \brief Initialize the decoding context when reading from a buffer
/// \param DecodeCtx
/// \param Buffer
/// \param BufferSize
/// \return
static GD_ERR
GD_InitDecodeContextMemory(GD_DECODE_CONTEXT* DecodeCtx, const void* Buffer, size_t BufferSize);

/// \brief Check whether the decoder can read a byte or has reached end of data-stream
/// \param DecodeCtx
/// \return
static GD_BOOL
GD_DecoderCanRead(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief Read a byte from the data-stream, and advance the reading cursor
/// \param DecodeCtx
/// \return
static BYTE
GD_ReadByte(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief Read an amount of bytes into a buffer from the data-stream, advance the reading cursor
/// \param DecodeCtx
/// \param Buffer
/// \param Count
/// \return
static size_t
GD_ReadBytes(GD_DECODE_CONTEXT* DecodeCtx, BYTE* Buffer, size_t Count);

/// \brief Read a two bytes in a little-endian format, advance the reading cursor
/// \param DecodeCtx
/// \return
static WORD
GD_ReadWord(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief Advance the reading cursor into the data-stream from a number of bytes
/// \param DecodeCtx
/// \param BytesCount
/// \return
static GD_ERR
GD_DecoderAdvance(GD_DECODE_CONTEXT* DecodeCtx, size_t BytesCount);

/// \brief Make sure the GIF header is correct, that means a valid signature and version
/// \param DecodeCtx
/// \param Version
/// \return
static GD_ERR
GD_ValidateHeader(GD_DECODE_CONTEXT* DecodeCtx, GD_GIF_VERSION* Version);

/// \brief Decode a GIF screen descriptor from the data-stream
/// \param DecodeCtx
/// \param ScrDescriptor
/// \return
static GD_ERR
GD_ReadScreenDescriptor(GD_DECODE_CONTEXT* DecodeCtx, GD_LOGICAL_SCREEN_DESCRIPTOR* ScrDescriptor);

/// \brief Decode a gif global color table from the data-stream
/// \param DecodeCtx
/// \param Table
/// \param ScrDescriptorFields
/// \return
static GD_ERR
GD_ReadGlobalColorTable(GD_DECODE_CONTEXT* DecodeCtx, GD_GLOBAL_COLOR_TABLE* Table, BYTE ScrDescriptorFields);

/// \brief Ignore gif data sub-blocks
/// \param DecodeCtx
static void
GD_IgnoreBlocks(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief
/// \param DecodeCtx
/// \return
static GD_ERR
GD_ReadExtPlainText(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief
/// \param DecodeCtx
/// \return
static GD_ERR
GD_ReadExtGraphics(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief
/// \param DecodeCtx
/// \return
static GD_ERR
GD_ReadExtComment(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief
/// \param DecodeCtx
/// \return
static GD_ERR
GD_ReadExtApplication(GD_DECODE_CONTEXT* DecodeCtx);

/// \brief
/// \param DecodeCtx
/// \param Gif
/// \return
static GD_ERR
GD_ReadExtension(GD_DECODE_CONTEXT* DecodeCtx, GD_GIF_HANDLE Gif);

/// \brief The main GIF decoding routine
/// \param DecodeCtx
/// \param ErrorCode
/// \return
static GD_GIF_HANDLE
GD_DecodeInternal(GD_DECODE_CONTEXT* DecodeCtx, GD_ERR* ErrorCode);


#endif //GIFDEC_GIFDEC_H
