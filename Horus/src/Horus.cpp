#include <stdio.h>
#include <windows.h>
#include "vec3.h"

typedef signed char           s8;
typedef char                  u8;
typedef short                 s16;
typedef unsigned short        u16;
typedef int                   s32;
typedef unsigned int          u32;
typedef long long             s64;
typedef unsigned long long    u64;
typedef float				  f32;
typedef double				  f64;

const static u32 output_width	= 640;
const static u32 output_height	= 320;
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

struct Ray
{
	vec3 origin;
	vec3 direction;
};

bool sphere(vec3& centre, float radius, Ray r)
{
	vec3	oc = r.origin - centre;
	f32		a = dot(r.direction, r.direction);
	f32		b = 2.0f * dot(oc, r.direction);
	f32		c = dot(oc, oc) - radius * radius;
	f32		d = b*b - 4*a*c;

	return (d > 0);
}

vec3 colour(Ray& r)
{
	
	vec3	sphere_colour = vec3(1.0f, 0.0f, 0.0f);
	vec3	sphere_position = vec3(0.0f, 0.0f, -1.0f);
	f32		sphere_radius = 0.5f;

	vec3	dir = normalize(r.direction);
	f32		t = 0.5f * (dir.y() + 1.0f);
	vec3	sky_colour = (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);

	vec3	final = sphere(sphere_position, sphere_radius, r) ? sphere_colour : sky_colour;

	return	final;
}

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
	info_header.x_resolution = 0;
	info_header.y_resolution = 0;
	info_header.colours = 0;
	info_header.colours_important = 0;

	u8* bitmap_image_data = (u8*) malloc(info_header.image_size);
	u32 bitmap_index = 0;

	vec3 bottom_left(-2.0f, -1.0f, -1.0f);
	vec3 horizontal	( 4.0f,  0.0f,  0.0f);
	vec3 vertical	( 0.0f,  2.0f,  0.0f);
	vec3 origin		( 0.0f,  0.0f,  0.0f);

	for (s16 y = 0; y < output_height; y++)
	{
		for (s16 x = 0; x < output_width; x++)
		{
			f32 u = (f32) x / (f32) output_width;
			f32 v = (f32) y / (f32) output_height;

			Ray r;
			r.origin = origin;
			r.direction = bottom_left + u * horizontal + v * vertical;

			vec3 c = colour(r);

			u8 red =  (int) 255.99 * c.r();
			u8 grn =  (int) 255.99 * c.g();
			u8 blu =  (int) 255.99 * c.b();

			bitmap_image_data[bitmap_index] = blu;
			bitmap_index++;

			bitmap_image_data[bitmap_index] = grn;
			bitmap_index++;

			bitmap_image_data[bitmap_index] = red;
			bitmap_index++;
		}
	}

	FILE* file = fopen("C:\\Users\\dave.wilson\\Documents\\Dave\\Horus\\Renders\\output.bmp", "wb");

	if (!file)
	{
		printf("Unable to open file!");
		return 1;
	}

	fwrite(&file_header,		sizeof(BitmapFileHeader),	1, file);
	fwrite(&info_header,		sizeof(BitmapInfoHeader),	1, file);
	fwrite(bitmap_image_data,	info_header.image_size,		1, file);

	fclose(file);
}