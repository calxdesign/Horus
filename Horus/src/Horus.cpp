#include <stdio.h>
#include <windows.h>

typedef signed char           s8;
typedef char                  u8;
typedef short                 s16;
typedef unsigned short        u16;
typedef int                   s32;
typedef unsigned int          u32;
typedef long long             s64;
typedef unsigned long long    u64;

const static u32 output_width	= 8;
const static u32 output_height	= 8;
const static u32 output_size	= output_width * output_height;

#pragma pack(1)

struct BitmapFileHeader
{
	u16 type;
	u32 size;
	u16 res_1;
	u16 res_2;
	u32 offset;
};

struct BitmapInfoHeader
{
	u32 size;
	u32 width;
	u32 height;
	u16 planes;
	u16 bits;
	u32 compression;
	u32 image_size;
	u32 x_resolution;
	u32 y_resolution;
	u32 colours;
	u32 colours_important;
};

int main()
{
	u32 remainder = output_width % 4;
	u32 row_size = (remainder == 0) ? (output_width * 3) : ((output_width + 4 - remainder) * 3);

	BitmapFileHeader file_header;

	file_header.type = 0x4d42;
	file_header.size = (row_size * output_height) + sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
	file_header.res_1 = 0;
	file_header.res_2 = 0;
	file_header.offset = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

	BitmapInfoHeader info_header;

	info_header.size = sizeof(BitmapInfoHeader);
	info_header.width = output_width;
	info_header.height = output_height;
	info_header.planes = 1;
	info_header.bits = 24;
	info_header.compression = 0;
	info_header.image_size = row_size * output_height;
	info_header.x_resolution = output_width;
	info_header.y_resolution = output_height;
	info_header.colours = 0;
	info_header.colours_important = 0;

	u8* bitmap_image_data = (u8*) malloc(info_header.image_size);

	u8 components[] = { 0x00, 0x99, 0xff, 0x00};

	for (u16 i = 0; i < info_header.image_size; ++i)
	{
		bitmap_image_data[i] = components[i % 3];
	}

	FILE* file = fopen("C:\\Users\\dave.wilson\\Desktop\\output.bmp", "wb");

	if (!file)
	{
		printf("Unable to open file!");
		return 1;
	}

	fwrite(&file_header,		sizeof(BitmapFileHeader),	1, file);
	fwrite(&info_header,		sizeof(BitmapInfoHeader),	1, file);
	fwrite(bitmap_image_data,	info_header.image_size,		1, file);

	fclose(file);

	printf("");
}