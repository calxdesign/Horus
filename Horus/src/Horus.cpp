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

struct Sphere
{
	vec3 position;
	f32  radius;
};

struct Camera
{
	vec3 bottom_left;
	vec3 eye;
	vec3 horizontal;
	vec3 vertical;
};

const static u32		output_width = 1000;
const static u32		output_height = 500;
const static u32		output_aa_samples = 8;
const static u32		output_size = output_width * output_height;
const static u8			num_spheres = 10;

static HWND				hwnd;
static Sphere*			spheres;
static u8*				bitmap_image_data;
static HBITMAP			bitmap_handle;
static HDC				device_context;
static BITMAPINFO		bitmapinfo;
static BitmapFileHeader file_header;
static BitmapInfoHeader info_header;
static Camera			camera;
static bool				output = false;
static bool				rendering = false;

const char class_name[] = "Simple Rheytracer";

f32 fract(f32 x)
{
	return x - (s64)x;
}

f32 hash(f32 x)
{
	return abs(fract(sin(x) * 43758.5453));
}

vec3 point_at_parameter(Ray r, float t)
{
	return r.origin + t * r.direction;
}

Ray get_ray(Camera cam, f32 u, f32 v)
{
	Ray ray;
	ray.origin = cam.eye;
	ray.direction = cam.bottom_left + u * cam.horizontal + v * cam.vertical - cam.eye;

	return ray;
}

void save_file()
{
	if (!output) return;

	FILE* file = fopen("D:\\Root\\Horus\\Renders\\output.bmp", "wb");

	if (!file) return;

	fwrite(&file_header, sizeof(BitmapFileHeader), 1, file);
	fwrite(&info_header, sizeof(BitmapInfoHeader), 1, file);
	fwrite(bitmap_image_data, info_header.image_size, 1, file);

	fclose(file);
}

f32 sphere(vec3 centre, float radius, Ray r)
{
	vec3	oc = r.origin - centre;
	f32		a = dot(r.direction, r.direction);
	f32		b = 2.0f * dot(oc, r.direction);
	f32		c = dot(oc, oc) - radius * radius;
	f32		d = b * b - 4 * a*c;

	if (d < 0) return -1.0f;
	else return (-b - sqrt(d)) / (2.0f * a);
}

vec3 raytrace(Ray ray)
{
	u8	num_hit = 0;
	u8  hit_indices[num_spheres];
	f32 t_values[num_spheres];
	f32 t = -1.0f;

	for (s8 i = 0; i < num_spheres; i++)
	{
		t = sphere(spheres[i].position, spheres[i].radius, ray);

		if (t > 0.0f)
		{
			hit_indices[num_hit] = i;
			t_values[num_hit] = t;
			num_hit++;
		}
	}

	if (num_hit > 1) 
	{
		f32 closest_t = FLT_MAX;
		s8	closest_index = 0;

		for (s8 i = 0; i < num_hit; i++)
		{
			u8 hit_index = hit_indices[i];

			vec3 d = ray.origin - spheres[hit_index].position;

			if (t_values[i] < closest_t)
			{
				closest_t = t_values[i];
				closest_index = hit_index;
			}
		}

		vec3 rp = point_at_parameter(ray, closest_t);
		vec3 normal = normalize(rp - spheres[closest_index].position);

		return 0.5f * vec3(normal.x() + 1.0f, normal.y() + 1.0f, normal.z() + 1.0f);
	} 
	else if (num_hit == 1)
	{
		vec3 rp = point_at_parameter(ray, t_values[0]);
		vec3 normal = normalize(rp - spheres[hit_indices[0]].position);

		return 0.5f * vec3(normal.x() + 1.0f, normal.y() + 1.0f, normal.z() + 1.0f);
	}

	vec3 dir = normalize(ray.direction);

	t = 0.5*(dir.y() + 1.0f);

	return (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);
}

void paint()
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		paint();
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

void setup_scene()
{
	spheres = (Sphere*) malloc(num_spheres * sizeof(Sphere));

	for (s8 i = 0; i < num_spheres; i++)
	{
		spheres[i].position = vec3(-5.0f + (u32) i, 0.0f, -2.0f);
		spheres[i].radius = 0.5f;
	}
}

void setup_camera()
{
	camera.eye = vec3(0.0f, 0.0f, 0.0f);
	camera.bottom_left = vec3(-2.0f, -1.0f, -1.0f);
	camera.horizontal = vec3(4.0f, 0.0f, 0.0f);
	camera.vertical = vec3(0.0f, 2.0f, 0.0f);
}

void setup_bitmap()
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

	bitmap_image_data = (u8*) malloc(info_header.image_size);
}

void render()
{
	rendering = true;

	u32 bitmap_index = 0;

	for (s16 y = 0; y < output_height; y++)
	{
		for (s16 x = 0; x < output_width; x++)
		{
			vec3 col = vec3(0.0f, 0.0f, 0.0f);

			for (s16 s = 0; s < output_aa_samples; s++)
			{
				f32 u = (f32)(x + hash((f32)x*x-s)) / (f32) output_width;
				f32 v = (f32)(y + hash((f32)y*y+s)) / (f32) output_height;

				Ray r = get_ray(camera, u, v);

				col += raytrace(r);
			}

			col /= (f32) output_aa_samples;

			u8 red = (int) 255.99 * col.r();
			u8 grn = (int) 255.99 * col.g();
			u8 blu = (int) 255.99 * col.b();
			u8 res = (int) 0;

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

	rendering = false;
}

DWORD WINAPI PaintThread(void* data)
{
	while (rendering)
	{
		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
		Sleep(30);
	}

	return 0;
}

DWORD WINAPI RenderThread(void* data)
{
	HANDLE paint_thread = CreateThread(NULL, 0, PaintThread, NULL, 0, NULL);

	render();

	return 0;
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
	WNDCLASSEX wc;
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

	setup_camera();
	setup_scene();
	setup_bitmap();

	HANDLE render_thread = CreateThread(NULL, 0, RenderThread, NULL, 0, NULL);
	
	//save_file();

	while (1)
	{
		if (!GetMessage(&msg, NULL, 0, 0)) return 0;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}