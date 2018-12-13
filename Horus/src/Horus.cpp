#include <stdio.h>
#include <windows.h>
#include "vec3.h"

#pragma pack(1)

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

const static u32 output_width = 1001;
const static u32 output_height = 500;
const static u32 output_size = output_width * output_height;

static u8* bitmap_image_data;
static HBITMAP bitmap_handle;
static HDC device_context;
static BITMAPINFO bitmapinfo;
static BitmapFileHeader file_header;
static BitmapInfoHeader info_header;
static bool output = false;

const char class_name[] = "Simple Rheytracer";

vec3 point_at_parameter(Ray r, float t)
{
	return r.origin + t * r.direction;
}

float sphere(vec3& centre, float radius, Ray r)
{
	vec3	oc = r.origin - centre;
	f32		a = dot(r.direction, r.direction);
	f32		b = 2.0f * dot(oc, r.direction);
	f32		c = dot(oc, oc) - radius * radius;
	f32		d = b * b - 4 * a*c;

	if (d < 0)
	{
		return -1.0f;
	}
	else
	{
		return (-b-sqrt(d)) / (2.0f*a);
	}
}

vec3 raytrace(Ray& ray)
{
	vec3 sphere_position = vec3(0.0f, 0.0f, -1.0f);
	f32	 sphere_radius = 0.5f;

	float t = sphere(sphere_position, sphere_radius, ray);

	if (t > 0.0f)
	{
		vec3 rp = point_at_parameter(ray, t);
		vec3 normal = normalize(rp - sphere_position);

		return 0.5f * vec3(normal.x() + 1.0f, normal.y() + 1.0f, normal.z() + 1.0f);
	}

	vec3 dir = normalize(ray.direction);

	t = 0.5*(dir.y() + 1.0f);

	return (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		s32 x = ps.rcPaint.left;
		s32 y = ps.rcPaint.top;
		s32 w = ps.rcPaint.right - ps.rcPaint.left;
		s32 h = ps.rcPaint.bottom - ps.rcPaint.top;

		StretchDIBits(dc, x, y, w, h, x, y, w, h, (void*)bitmap_image_data, (BITMAPINFO*)&info_header, DIB_RGB_COLORS, SRCCOPY);

		EndPaint(hwnd, &ps);
	}
	break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, w_param, l_param);
		break;
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
	u32 remainder = output_width % 5;
	u32 row_size = (remainder == 0) ? (output_width * 4) : ((output_width + 4 - remainder) * 4);

	file_header.type = 0x4d42;
	file_header.size = (row_size * output_height) + sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
	file_header.res_1 = 0;
	file_header.res_2 = 0;
	file_header.offset = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

	device_context = CreateCompatibleDC(0);
	bitmap_handle = CreateDIBSection(device_context, (BITMAPINFO*)&file_header, DIB_RGB_COLORS, (void**)&bitmap_image_data, 0, 0);

	info_header.size = sizeof(BitmapInfoHeader);
	info_header.width = output_width;
	info_header.height = output_height;
	info_header.planes = 1;
	info_header.bits = 32;
	info_header.compression = 0;
	info_header.image_size = row_size * output_height;
	info_header.x_resolution = 0;
	info_header.y_resolution = 0;
	info_header.colours = 0;
	info_header.colours_important = 0;

	bitmap_image_data = (u8*)malloc(info_header.image_size);
	u32 bitmap_index = 0;

	vec3 bottom_left(-2.0f, -1.0f, -1.0f);
	vec3 horizontal(4.0f, 0.0f, 0.0f);
	vec3 vertical(0.0f, 2.0f, 0.0f);
	vec3 origin(0.0f, 0.0f, 0.0f);

	for (s16 y = 0; y < output_height; y++)
	{
		for (s16 x = 0; x < output_width; x++)
		{
			f32 u = (f32)x / (f32)output_width;
			f32 v = (f32)y / (f32)output_height;

			Ray r;
			r.origin = origin;
			r.direction = bottom_left + u * horizontal + v * vertical;

			vec3 c = raytrace(r);

			u8 red = (int) 255.99 * c.r();
			u8 grn = (int) 255.99 * c.g();
			u8 blu = (int) 255.99 * c.b();
			u8 res = (int)0;

			bitmap_image_data[bitmap_index] = blu;
			bitmap_index++;

			bitmap_image_data[bitmap_index] = grn;
			bitmap_index++;

			bitmap_image_data[bitmap_index] = red;
			bitmap_index++;

			bitmap_image_data[bitmap_index] = res;
			bitmap_index++;
		}
	}

	if (output)
	{
		FILE* file = fopen("D:\\Root\\Horus\\Renders\\output.bmp", "wb");

		if (!file)
		{
			printf("Unable to open file!");
			return false;
		}

		fwrite(&file_header, sizeof(BitmapFileHeader), 1, file);
		fwrite(&info_header, sizeof(BitmapInfoHeader), 1, file);
		fwrite(bitmap_image_data, info_header.image_size, 1, file);

		fclose(file);
	}

	WNDCLASSEX wc;
	HWND hwnd;
	MSG msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = h_instance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = class_name;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc)) return 0;

	DWORD dwStyle = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, class_name, class_name, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, output_width, output_height + 43, NULL, NULL, h_instance, NULL);

	if (!hwnd) return 0;

	ShowWindow(hwnd, cmd_show);
	UpdateWindow(hwnd);

	while (1)
	{
		if (!GetMessage(&msg, NULL, 0, 0)) return 0;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}