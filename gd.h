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

typedef uint8_t  GD_BYTE;
typedef uint16_t GD_WORD;
typedef uint32_t GD_DWORD;


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
	GD_NO_COLOR_TABLE,
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

typedef struct GD_DataBlock
{
	struct GD_DataBlock* BLink;
	struct GD_DataBlock* FLink;

	GD_BYTE Data[SUB_BLOCK_MAX_SIZE];
	GD_BYTE EffectiveSize;

} GD_DataBlock;


typedef struct GD_DataBlockList
{
	GD_DataBlock* Head;
	GD_DataBlock* Tail;

	size_t BlockCount;

} GD_DataBlockList;


typedef struct GD_EXT_GRAPHICS
{
	GD_BYTE PackedFields;
	GD_WORD DelayTime;
	GD_BYTE TransparentColorIndex;
} GD_EXT_GRAPHICS;


typedef struct GD_EXT_COMMENT
{
	GD_DataBlockList Blocks;
} GD_EXT_COMMENT;


typedef struct GD_EXT_PLAINTEXT
{
	GD_WORD GridPositionLeft;
	GD_WORD GridPositionTop;
	GD_WORD GridWidth;
	GD_WORD GridHeight;
	GD_BYTE CharCellWidth;
	GD_BYTE CharCellHeight;
	GD_BYTE FgColorIndex;
	GD_BYTE BgColorIndex;

	GD_DataBlockList Blocks;

} GD_EXT_PLAINTEXT;


typedef struct GD_EXT_APPLICATION
{
	GD_BYTE AppId[8];
	GD_BYTE AppAuth[3];

	GD_DataBlockList Blocks;

} GD_EXT_APPLICATION;


typedef enum GD_EXTENSION_TYPE
{
	GD_APPLICATION = 0,
	GD_PLAINTEXT   = 1,
	GD_GRAPHICS    = 2,
	GD_COMMENT     = 3
} GD_EXTENSION_TYPE;


typedef void(*GD_EXT_ROUTINE_APPLICATION)(const GD_EXT_APPLICATION* Extension);
typedef void(*GD_EXT_ROUTINE_PLAINTEXT)(const GD_EXT_PLAINTEXT* Extension);
typedef void(*GD_EXT_ROUTINE_GRAPHICS)(const GD_EXT_GRAPHICS* Extension);
typedef void(*GD_EXT_ROUTINE_COMMENT)(const GD_EXT_COMMENT* Extension);

#define MAX_REGISTERED_ROUTINES 4

typedef struct GD_EXT_ROUTINES {
	void* Routines[MAX_REGISTERED_ROUTINES];
	size_t RegisteredCount;
} GD_EXT_ROUTINES;

static GD_EXT_ROUTINES ApplicationExtRoutines;
static GD_EXT_ROUTINES PlaintextExtRoutines;
static GD_EXT_ROUTINES GraphicsExtRoutines;
static GD_EXT_ROUTINES CommentExtRoutines;



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
	GD_WORD LogicalWidth;
	GD_WORD LogicalHeight;
	GD_BYTE PackedFields;
	GD_BYTE BgColorIndex;
	GD_BYTE PixelAspectRatio;
} GD_LOGICAL_SCREEN_DESCRIPTOR;


typedef struct GD_IMAGE_DESCRIPTOR
{
	GD_WORD PositionLeft;
	GD_WORD PositionTop;
	GD_WORD Width;
	GD_WORD Height;
	GD_BYTE PackedFields;
} GD_IMAGE_DESCRIPTOR;


typedef struct GD_GIF_COLOR
{
	GD_BYTE r;
	GD_BYTE g;
	GD_BYTE b;
} GD_GIF_COLOR;


typedef struct GD_GLOBAL_COLOR_TABLE
{
	GD_GIF_COLOR Internal[GCT_MAX_SIZE];
	size_t Count;
} GD_COLOR_TABLE;




/////////////////////////////////////////////////////////////////
///                      GD PUBLIC API                         //
/////////////////////////////////////////////////////////////////

typedef struct GD_GIF
{
	GD_GIF_VERSION Version;
	GD_LOGICAL_SCREEN_DESCRIPTOR ScreenDesc;
	GD_COLOR_TABLE GlobalColorTable;

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
GD_ImageBuffer(GD_GIF_HANDLE Gif, size_t ImageIndex, GD_BYTE** Buffer, size_t* BufferSize);

/// \brief Register a routine to be called when the decoder encounters an application, comment or plain text extension
/// \param RoutineType
/// \param UserRoutine
/// \return
GD_ERR
GD_RegisterExRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine);

/// \brief Clear all routines for a certain extension type
/// \param RoutineType
void
GD_ClearExRoutines(GD_EXTENSION_TYPE RoutineType);

/// \brief Clear all routines RegisteredCount for every extension type
void
GGD_ClearAllExRoutines();

/// \brief Unregister a single routine by address
/// \param RoutineType
/// \param UserRoutine
void
GD_UnregisterExRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine);

/// \brief Convert a GD_ERR value to its string representation
/// \param Error
/// \return
const char*
GD_ErrorAsString(GD_ERR Error);



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
	GD_BYTE   StreamChunk[CHUNK_SIZE];

	//
	// Pointer and size of the buffer used to decode from memory
	//
	GD_BYTE*  MemoryBuffer;
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
	GD_BYTE* SourceBeg;
	GD_BYTE* SourceEnd;

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
/// \param Decoder
static void
GD_DecoderLoadChunk(GD_DECODE_CONTEXT* Decoder);

/// \brief Initialize the decoding context when reading from a file
/// \param Decoder
/// \param Path
/// \return
static GD_ERR
GD_InitDecodeContextStream(GD_DECODE_CONTEXT* Decoder, const char* Path);

/// \brief Initialize the decoding context when reading from a buffer
/// \param Decoder
/// \param Buffer
/// \param BufferSize
/// \return
static GD_ERR
GD_InitDecodeContextMemory(GD_DECODE_CONTEXT* Decoder, const void* Buffer, size_t BufferSize);

/// \brief Check whether the decoder can read a byte or has reached end of data-stream
/// \param Decoder
/// \return
static GD_BOOL
GD_DecoderCanRead(GD_DECODE_CONTEXT* Decoder);

/// \brief Read a byte from the data-stream, and advance the reading cursor
/// \param Decoder
/// \return
static GD_BYTE
GD_ReadByte(GD_DECODE_CONTEXT* Decoder);

/// \brief Read an amount of bytes into a buffer from the data-stream, advance the reading cursor
/// \param Decoder
/// \param Buffer
/// \param Count
/// \return
static size_t
GD_ReadBytes(GD_DECODE_CONTEXT* Decoder, GD_BYTE* Buffer, size_t Count);

/// \brief Read a two bytes in a little-endian format, advance the reading cursor
/// \param Decoder
/// \return
static GD_WORD
GD_ReadWord(GD_DECODE_CONTEXT* Decoder);

/// \brief Advance the reading cursor into the data-stream from a number of bytes
/// \param Decoder
/// \param BytesCount
/// \return
static GD_ERR
GD_DecoderAdvance(GD_DECODE_CONTEXT* Decoder, size_t BytesCount);

/// \brief Make sure the GIF header is correct, that means a valid signature and version
/// \param Decoder
/// \param Version
/// \return
static GD_ERR
GD_ValidateHeader(GD_DECODE_CONTEXT* Decoder, GD_GIF_VERSION* Version);

/// \brief Decode a GIF screen descriptor from the data-stream
/// \param Decoder
/// \param ScrDescriptor
/// \return
static GD_ERR
GD_ReadScreenDescriptor(GD_DECODE_CONTEXT* Decoder, GD_LOGICAL_SCREEN_DESCRIPTOR* ScrDescriptor);

/// \brief Decode a gif global color table from the data-stream
/// \param Decoder
/// \param Table
/// \param ScrDescriptorFields
/// \return
static GD_ERR
GD_ReadColorTable(GD_DECODE_CONTEXT* Decoder, GD_COLOR_TABLE* Table, GD_BYTE ScrDescriptorFields);

/// \brief Ignore gif data sub-blocks
/// \param Decoder
static void
GD_IgnoreSubDataBlocks(GD_DECODE_CONTEXT* Decoder);

/// \brief Same as BlockListBuild but creates a linear buffer instead of a linked list
/// \param Decoder
/// \param Buffer
/// \param BufferSize
/// \return
static GD_ERR
GD_BlocksToLinearBuffer(GD_DECODE_CONTEXT* Decoder, GD_BYTE** Buffer, GD_DWORD* BufferSize);

static GD_ERR
GD_CreateBlock(GD_DECODE_CONTEXT* Decoder, GD_BYTE BSize, GD_DataBlock** OutputBlock);

/// \brief Create a doubly linked list of sub data blocks
/// \param Decoder
/// \param List
/// \return
static GD_ERR
GD_BlockListBuild(GD_DECODE_CONTEXT* Decoder, GD_DataBlockList* List);

/// \brief De-allocates a block list allocated by \ref GD_BlockListBuild
/// \param List
static void
GD_BlockListDestroy(GD_DataBlockList* List);

/// \brief Add a new block at the end of the list
/// \param List
/// \param Block
static void
GD_BlockListAppend(GD_DataBlockList* List, GD_DataBlock* Block);


/// \brief
/// \param Decoder
/// \return
static GD_ERR
GD_ReadExtPlainText(GD_DECODE_CONTEXT* Decoder);

/// \brief
/// \param Decoder
/// \return
static GD_ERR
GD_ReadExtGraphics(GD_DECODE_CONTEXT* Decoder);

/// \brief
/// \param Decoder
/// \return
static GD_ERR
GD_ReadExtComment(GD_DECODE_CONTEXT* Decoder);

/// \brief
/// \param Decoder
/// \return
static GD_ERR
GD_ReadExtApplication(GD_DECODE_CONTEXT* Decoder);

/// \brief
/// \param Decoder
/// \param Gif
/// \return
static GD_ERR
GD_ReadExtension(GD_DECODE_CONTEXT* Decoder);

/// \brief
/// \param Decoder
/// \return
static GD_ERR
GD_ReadImage(GD_DECODE_CONTEXT* Decoder, GD_GIF_HANDLE Gif);

/// \brief
/// \param Decoder
/// \param Gif
/// \param ImageDescriptor
/// \return
static GD_ERR
GD_ProcessImageRaster(GD_DECODE_CONTEXT* Decoder, GD_GIF_HANDLE Gif, GD_IMAGE_DESCRIPTOR* ImageDescriptor, GD_COLOR_TABLE* ActiveTable);

/// \brief The main GIF decoding routine
/// \param Decoder
/// \param ErrorCode
/// \return
static GD_GIF_HANDLE
GD_DecodeInternal(GD_DECODE_CONTEXT* Decoder, GD_ERR* ErrorCode);



#endif //GIFDEC_GIFDEC_H
