#ifndef GIFDEC_GIFDEC_H
#define GIFDEC_GIFDEC_H


#include <stdint.h>
#include <stddef.h>


#define SIGNATURE_SIZE 3
#define VERSION_SIZE   3
#define HEADER_SIZE (SIGNATURE_SIZE + VERSION_SIZE)
#define GCT_MAX_SIZE 256



#define EXT_LABEL_PLAINTEXT   0x01
#define EXT_LABEL_GRAPHICS    0xF9
#define EXT_LABEL_COMMENT     0xFE
#define EXT_LABEL_APPLICATION 0xFF

#define BLOCK_INTRODUCER_EXT 0x21
#define BLOCK_INTRODUCER_IMG 0x2C
#define TRAILER              0x3B


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
} GD_ERR;


typedef struct GD_EXT_GRAPHICS
{
	BYTE PackedFields;
	WORD DelayTime;
	BYTE TransparentColorIndex;
} GD_EXT_GRAPHICS;


typedef struct GD_EXT_COMMENT
{
	char c;
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
} GD_EXT_PLAINTEXT;


typedef struct GD_EXT_APPLICATION
{
	BYTE AppId[8];
	BYTE AppAuth[3];
} GD_EXT_APPLICATION;


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

typedef struct GD_GRAPHICS_CONTROL_EXTENSION
{
	char c;
} GD_GRAPHICS_CONTROL_EXTENSION;

typedef struct GD_GIF
{
	GD_GIF_VERSION Version;
	GD_LOGICAL_SCREEN_DESCRIPTOR ScreenDesc;
	GD_GLOBAL_COLOR_TABLE GlobalColorTable;

} GD_GIF, *GD_GIF_HANDLE;


typedef void(*AppExRoutine)(GD_EXT_APPLICATION* Data);

GD_ERR
GD_RegisterAppExtRoutine(AppExRoutine UserRoutine);

GD_GIF_HANDLE
GD_OpenGif(const char* Path, GD_ERR* ErrorCode, size_t* ErrorBytePos);

GD_GIF_HANDLE
GD_FromMemory(const void* Buffer, size_t BufferSize, GD_ERR* ErrorCode, size_t* ErrorBytePos);

void
GD_CloseGif(GD_GIF_HANDLE Gif);

GD_GIF_VERSION
GD_GifVersion(GD_GIF_HANDLE Gif);

GD_BOOL
GD_HasGlobalColorTable(GD_GIF_HANDLE Gif);

GD_GIF_COLOR
GD_BackgroundColor(GD_GIF_HANDLE Gif);

size_t
GD_ImageCount(GD_GIF_HANDLE Gif);

GD_ERR
GD_ImageBuffer(GD_GIF_HANDLE Gif, size_t ImageIndex, BYTE** Buffer, size_t* BufferSize);



#endif //GIFDEC_GIFDEC_H
