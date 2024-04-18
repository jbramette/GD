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



///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
////                                                            ///
////                      GD PUBLIC API                         ///
////                                                            ///
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////
///                      LIBRARY TYPES                           //
///////////////////////////////////////////////////////////////////
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

#define GD_SUCCESS(ErrCode) (ErrCode == GD_OK)


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


typedef struct GD_FRAME
{
	GD_IMAGE_DESCRIPTOR Descriptor;
	GD_GIF_COLOR* Buffer;
} GD_FRAME;



/// Used as an opaque pointer
/// The user should not need/have access to the internal structure
struct GD_GIF;
typedef struct GD_GIF* GD_GIF_HANDLE;


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


const char*
GD_ErrorAsString(GD_ERR Error);

GD_DWORD GD_FrameCount(GD_GIF_HANDLE Gif);

GD_FRAME* GD_GetFrame(GD_GIF_HANDLE Gif, GD_DWORD FrameIndex);


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


#define MAX_REGISTERED_ROUTINES 4


typedef void(*GD_EXT_ROUTINE_APPLICATION)(const GD_EXT_APPLICATION* Extension);
typedef void(*GD_EXT_ROUTINE_PLAINTEXT)(const GD_EXT_PLAINTEXT* Extension);
typedef void(*GD_EXT_ROUTINE_GRAPHICS)(const GD_EXT_GRAPHICS* Extension);
typedef void(*GD_EXT_ROUTINE_COMMENT)(const GD_EXT_COMMENT* Extension);


GD_ERR
GD_RegisterExRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine);

void
GD_ClearExRoutines(GD_EXTENSION_TYPE RoutineType);

void
GGD_ClearAllExRoutines();

void
GD_UnregisterExRoutine(GD_EXTENSION_TYPE RoutineType, void* UserRoutine);



#endif //GIFDEC_GIFDEC_H
