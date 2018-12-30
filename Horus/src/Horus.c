#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

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

typedef struct vec3
{
	f32 x;
	f32 y;
	f32 z;

} vec3;

typedef struct BitmapFileHeader
{
	u16 type;
	u32 size;
	u16 res_1;
	u16 res_2;
	u32 offset;

} BitmapFileHeader;

typedef struct BitmapInfoHeader
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

} BitmapInfoHeader;

typedef struct Ray
{
	vec3 origin;
	vec3 direction;

} Ray;

typedef struct Sphere
{
	vec3 position;
	f32  radius;

} Sphere;

typedef struct Camera
{
	vec3 bottom_left;
	vec3 eye;
	vec3 horizontal;
	vec3 vertical;

} Camera;

typedef struct Material
{
	f32 reflection;

} Material;

typedef struct Hit
{
	f32			t;
	vec3		point;
	vec3		normal;
	Material*	material;
} Hit;

static u32						output_width = 800;
static u32						output_height = 400;
static u32						num_spheres = 2;
static u32						ssx_samples = 4;
static u32						ssy_samples = 4;
static u32						rnd_samples = 127;

#define	AA_SAMPLESTYLE_GRID		0
#define	AA_SAMPLESTYLE_RANDOM	1
#define	output_size				output_width * output_height
#define	aa_samplestyle			AA_SAMPLESTYLE_RANDOM
#define	RENDER					0
#define	IDLE					1
#define MULTITHREADING			1
#define FINISHED_MESSAGE		1
#define OUTPUT					0

static HWND						hwnd;
static Sphere*					spheres;
static u8*						bitmap_image_data;
static HBITMAP					bitmap_handle;
static HDC						device_context;
static BITMAPINFO				bitmapinfo;
static BitmapFileHeader			file_header;
static BitmapInfoHeader			info_header;
static Camera					camera;
static u8						STATE = RENDER;

const char class_name[] = "Simple Rheytracer";

f32 vec3_dot(vec3 a, vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_div(vec3 a, f32 n)
{
	vec3 v;
	v.x = a.x / n;
	v.y = a.y / n;
	v.z = a.z / n;

	return v;
}

f32 vec3_mag(vec3 a)
{
	return (f32)sqrt(a.x*a.x + a.y * a.y + a.z * a.z);
}

vec3 vec3_normalized(vec3 a)
{
	f32  m = vec3_mag(a);
	vec3 v;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = 0.0f;

	if (m != 0.0f && m != 1.0f) 
	{
		v = vec3_div(a, m);
	}

	return v;
}

vec3 vec3_mul(vec3 a, f32 n)
{
	vec3 v;
	v.x = a.x * n;
	v.y = a.y * n;
	v.z = a.z * n;

	return v;
}

vec3 vec3_sub(vec3 a, vec3 b)
{
	vec3 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	v.z = a.z - b.z;

	return v;
}

vec3 vec3_add(vec3 a, vec3 b)
{
	vec3 v;

	v.x = a.x + b.x;
	v.y = a.y + b.y;
	v.z = a.z + b.z;

	return v;
}

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
	vec3 t_dir = vec3_mul(r.direction, t);
	vec3 point = vec3_add(t_dir, r.origin);

	return point;
}

f32 vec3_squared_length(vec3 a)
{
	return (a.x*a.x + a.y * a.y + a.z * a.z);
}

Ray get_ray(Camera cam, f32 u, f32 v)
{
	Ray ray;
	ray.origin = cam.eye;

	vec3 u_norm = vec3_mul(cam.horizontal, u);
	vec3 v_norm = vec3_mul(cam.vertical, v);
	vec3 uv_norm = vec3_add(u_norm, v_norm);
	vec3 uv_cam = vec3_add(uv_norm, cam.bottom_left);

	ray.direction = vec3_sub(uv_cam, cam.eye);

	return ray;
}

void save_file(void)
{
	if (!OUTPUT) return;

	FILE* file = fopen("D:\\Root\\Horus\\Renders\\output.bmp", "wb");

	if (!file) return;

	fwrite(&file_header, sizeof(BitmapFileHeader), 1, file);
	fwrite(&info_header, sizeof(BitmapInfoHeader), 1, file);
	fwrite(bitmap_image_data, info_header.image_size, 1, file);

	fclose(file);
}

f32 sphere(vec3 centre, float radius, Ray r)
{
	vec3	oc = vec3_sub(r.origin, centre);
	f32		a = vec3_dot(r.direction, r.direction);
	f32		b = 2.0f * vec3_dot(oc, r.direction);
	f32		c = vec3_dot(oc, oc) - radius * radius;
	f32		d = b * b - 4 * a*c;

	if (d < 0) return -1.0f;
	else return (-b - sqrt(d)) / (2.0f * a);
}

vec3 random_point_within_magnitude(f32 mag)
{
	while (1)
	{
		vec3 pos;
		pos.x = ((f32)rand() / (RAND_MAX));
		pos.y = ((f32)rand() / (RAND_MAX));
		pos.z = ((f32)rand() / (RAND_MAX));

		vec3 point = vec3_mul(pos, 2.0f);

		vec3 unit;
		unit.x = 1.0f;
		unit.y = 1.0f;
		unit.z = 1.0f;

		vec3 final = vec3_sub(point, unit);

		if (vec3_squared_length(final) < mag)
		{
			return final;
		}
	}	
}

u32 intersection(Ray* r, Sphere s, Hit* h, float t_min, float t_max)
{
	vec3	oc = vec3_sub(r->origin, s.position);
	f32		a = vec3_dot(r->direction, r->direction);
	f32		b = vec3_dot(oc, r->direction);
	f32		c = vec3_dot(oc, oc) - s.radius * s.radius;
	f32		d = b*b*a*c;

	if (d > 0)
	{
		float temp = 0.0f;
		temp = (-b - sqrt(b*b - a*c)) / a;

		if (temp < t_max && temp > t_min)
		{
			h->t = temp;
			h->point = point_at_parameter(*r, temp);

			vec3 dir = vec3_sub(h->point, s.position);
			h->normal = vec3_div(dir, s.radius);

			return 1;
		}

		temp = (-b + sqrt(b*b - a * c)) / a;

		if (temp < t_max && temp > t_min)
		{
			h->t = temp;
			h->point = point_at_parameter(*r, temp);

			vec3 dir = vec3_sub(h->point, s.position);
			h->normal = vec3_div(dir, s.radius);

			return 1;
		}
	}

	return 0;
}

u32 intersects_all(Ray r, Hit* h, float t_min, float t_max)
{
	Hit	 temp;
	u32  hit_something = 0;
	u64  closest = t_max;

	for (u32 i = 0; i < num_spheres; i++)
	{
		if (intersection(&r, spheres[i], &temp, t_min, closest))
		{
			hit_something = 1;
			closest = temp.t;

			h->t = temp.t;
			h->point = temp.point;
			h->normal = temp.normal;
		}
	}

	return hit_something;
}

void paint(void)
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
		save_file();
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

void setup_scene(void)
{
	spheres = (Sphere*)malloc(num_spheres * sizeof(Sphere));

	f32 object_sphere_radius = 0.5f;
	vec3 object_sphere_position;
	object_sphere_position.x = 000.0f;
	object_sphere_position.y = 000.0f;
	object_sphere_position.z = -001.0f;

	f32 ground_sphere_radius = 100.0f;
	vec3 ground_sphere_position;
	ground_sphere_position.x = 000.0f;
	ground_sphere_position.y = -100.5f;
	ground_sphere_position.z = -001.0f;

	spheres[0].position = object_sphere_position;
	spheres[0].radius = object_sphere_radius;

	spheres[1].position = ground_sphere_position;
	spheres[1].radius = ground_sphere_radius;
}

void setup_camera(void)
{
	camera.eye.x = 0.0f;
	camera.eye.y = 0.0f;
	camera.eye.z = 0.0f;

	camera.bottom_left.x = -2.0f;
	camera.bottom_left.y = -1.0f;
	camera.bottom_left.z = -1.0f;

	camera.horizontal.x = 4.0f;
	camera.horizontal.y = 0.0f;
	camera.horizontal.z = 0.0f;

	camera.vertical.x = 0.0f;
	camera.vertical.y = 2.0f;
	camera.vertical.z = 0.0f;
}

vec3 colour(Ray r)
{
	Hit h;

	if (intersects_all(r, &h, 0.001f, FLT_MAX))
	{
		vec3 random_unit_vector = random_point_within_magnitude(1.0f);
		vec3 point_pos = vec3_add(h.point, h.normal);
		vec3 target = vec3_add(point_pos, random_unit_vector);
		
		Ray ray;
		ray.origin = h.point;
		ray.direction = vec3_sub(target, h.point);

		vec3 col = colour(ray);
		vec3 final = vec3_mul(col, 0.5f);

		return final;
	}
	else
	{
		vec3 dir_n = vec3_normalized(r.direction);

		f32 t = 0.5f * (dir_n.y + 1.0f);

		vec3 white;
		white.x = 1.0f;
		white.y = 1.0f;
		white.z = 1.0f;

		vec3 blue;
		blue.x = 0.5f;
		blue.y = 0.7f;
		blue.z = 1.0f;

		vec3 white_component = vec3_mul(white, 1.0f - t);
		vec3 blue_component = vec3_mul(blue, t);
		vec3 gradient = vec3_add(white_component, blue_component);

		return gradient;
	}
}

void setup_bitmap(void)
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

	bitmap_image_data = (u8*)_aligned_malloc(info_header.image_size, 64);
}




void render_pixel(u32 x, u32 y)
{
	vec3	col;
	col.x = 0.0f;
	col.y = 0.0f;
	col.z = 0.0f;

	u32	num_aa_samples = 8;

	switch (aa_samplestyle)
	{
		case AA_SAMPLESTYLE_GRID:
		{
			f32 x_inc = 1.0f / ssx_samples;
			f32 y_inc = 1.0f / ssy_samples;

			num_aa_samples = ssx_samples * ssy_samples;

			for (s16 ssy = 0; ssy < ssy_samples; ssy++)
			{
				for (s16 ssx = 0; ssx < ssx_samples; ssx++)
				{
					f32 u = (f32)(x + (x_inc * ssx)) / (f32)output_width;
					f32 v = (f32)(y + (y_inc * ssy)) / (f32)output_height;

					Ray r = get_ray(camera, u, v);

					vec3 c = colour(r);
					col.x += c.x;
					col.y += c.y;
					col.z += c.z;
				}
			}
		}

		case AA_SAMPLESTYLE_RANDOM:
		{
			f32 x_inc = 1.0f / ssx_samples;
			f32 y_inc = 1.0f / ssy_samples;

			col.x = 0.0f;
			col.y = 0.0f;
			col.z = 0.0f;

			num_aa_samples = rnd_samples;

			for (s16 sample = 0; sample < num_aa_samples; sample++)
			{
				f32 u = (x + ((f32) rand() / (RAND_MAX))) / (f32) output_width;
				f32 v = (y + ((f32) rand() / (RAND_MAX))) / (f32) output_height;

				Ray r = get_ray(camera, u, v);

				vec3 c = colour(r);
				col.x += c.x;
				col.y += c.y;
				col.z += c.z;
			}
		}
	}

	vec3 final = vec3_div(col, (f32)num_aa_samples);

	u8 red = (int) (255.99 * sqrt(final.x));
	u8 grn = (int) (255.99 * sqrt(final.y));
	u8 blu = (int) (255.99 * sqrt(final.z));
	u8 res = (int) 0;

	int bitmap_index = ((y * output_width) + x) * 4;

	bitmap_image_data[bitmap_index + 0] = blu;
	bitmap_image_data[bitmap_index + 1] = grn;
	bitmap_image_data[bitmap_index + 2] = red;
	bitmap_image_data[bitmap_index + 3] = res;
}

void render(u16 offset, u16 inc)
{
	for (s16 y = offset; y < output_height; y += inc)
	{
		for (s16 x = 0; x < output_width; x++)
		{
			render_pixel(x, y);
		}
	}
}

DWORD WINAPI PaintThread(void* data)
{
	while (1)
	{
		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
		Sleep(30);
	}

	return 0;
}

DWORD WINAPI RenderThread(void* data)
{
	u32* d = (u32*) data;

	u16 offset = (*d >> 16);
	u16 increment = *d;

	render(offset, increment);

	return 0;
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
	WNDCLASSEX wc;
	MSG msg;

	vec3 first;
	first.x = 1.0f;
	first.y = 1.0f;
	first.z = 1.0f;

	vec3 second;
	second.x = 2.0f;
	second.y = 2.0f;
	second.z = 2.0f;

	vec3 a = vec3_add(first, second);
	vec3 b = vec3_div(first, 2.0f);
	vec3 c = vec3_mul(first, 0.5f);
	vec3 d = vec3_sub(first, second);

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

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	u16 cpu_count = MULTITHREADING ? system_info.dwNumberOfProcessors : 1;
	u32* render_thread_data = (u32*)malloc(cpu_count * sizeof(u32));
	HANDLE* render_threads = (HANDLE*)malloc(cpu_count * sizeof(HANDLE));
	clock_t start, end;
	f64 cpu_time_used;

	start = clock();

	for (u16 i = 0; i < cpu_count; i++)
	{
		render_thread_data[i] = i << 16 | cpu_count;
		render_threads[i] = CreateThread(NULL, 0, RenderThread, (void*)&render_thread_data[i], 0, NULL);
		SetThreadAffinityMask(render_threads[i], 1 << i);
	}

	HANDLE window_thread = CreateThread(NULL, 0, PaintThread, NULL, 0, NULL);

	while (1)
	{
		switch (STATE)
		{
		case RENDER:
		{
			u32 active_threads = 0;

			for (u32 i = 0; i < cpu_count; i++)
			{
				if (WaitForSingleObject(render_threads[i], 0) != WAIT_OBJECT_0) active_threads++;
			}

			if (active_threads == 0)
			{
				end = clock();

				TerminateThread(window_thread, 0);

				cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

				char s[32];

				sprintf(s, "Finished in %f", cpu_time_used);

				if(FINISHED_MESSAGE) MessageBox(NULL, s, "Renderer", MB_ICONEXCLAMATION | MB_OK);

				STATE = IDLE;
			}
		}
		break;
		case IDLE:
			break;
		default:
			break;
		}

		if (!GetMessage(&msg, NULL, 0, 0)) return 0;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}