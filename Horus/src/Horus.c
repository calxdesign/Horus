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

typedef enum MaterialType
{
	METAL, LAMBERT

} MaterialType;

typedef struct v3
{
	f32 x;
	f32 y;
	f32 z;

} v3;

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
	v3 origin;
	v3 direction;

} Ray;

typedef struct Material
{
	MaterialType type;
	v3			 albedo;

} Material;

typedef struct Sphere
{
	v3			position;
	f32			radius;
	Material	material;

} Sphere;

typedef struct Camera
{
	v3 bottom_left;
	v3 eye;
	v3 horizontal;
	v3 vertical;

} Camera;

typedef struct Hit
{
	f32			t;
	v3			point;
	v3			normal;
	Material	material;

} Hit;

static u32						output_width = 800;
static u32						output_height = 400;
static u32						num_spheres = 2;
static u32						ssx_samples = 4;
static u32						ssy_samples = 4;
static u32						rnd_samples = 127;

#define	AA_SAMPLESTYLE_GRID		0
#define	AA_SAMPLESTYLE_RANDOM	1
#define	OUTPUTSIZE				output_width * output_height
#define	AA_SAMPLESTYLE			AA_SAMPLESTYLE_RANDOM
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

const char class_name[] = "BerkTracer";

f32 v3_dot(v3 a, v3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

v3 v3_div(v3 a, f32 n)
{
	v3 v;
	v.x = a.x / n;
	v.y = a.y / n;
	v.z = a.z / n;

	return v;
}

f32 v3_mag(v3 a)
{
	return (f32)sqrt(a.x*a.x + a.y * a.y + a.z * a.z);
}

v3 vec3(f32 x, f32 y, f32 z)
{
	v3 v;
	v.x = x;
	v.y = y;
	v.z = z;

	return v;
}

v3 v3_normalized(v3 a)
{
	f32  m = v3_mag(a);
	v3 v;
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = 0.0f;

	if (m != 0.0f && m != 1.0f) v = v3_div(a, m);

	return v;
}

v3 v3_mulf(v3 a, f32 n)
{
	v3 v;
	v.x = a.x * n;
	v.y = a.y * n;
	v.z = a.z * n;

	return v;
}

v3 v3_mulv(v3 a, v3 b)
{
	v3 v;
	v.x = a.x * b.x;
	v.y = a.y * b.y;
	v.z = a.z * b.z;

	return v;
}

v3 v3_sub(v3 a, v3 b)
{
	v3 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	v.z = a.z - b.z;

	return v;
}

v3 v3_add(v3 a, v3 b)
{
	v3 v;

	v.x = a.x + b.x;
	v.y = a.y + b.y;
	v.z = a.z + b.z;

	return v;
}

v3 v3_reflect(v3 a, v3 normal)
{
	f32 dp = v3_dot(a, normal);
	v3	v = v3_mulf(normal, 2.0f*dp);
	v3  r = v3_sub(a, v);

	return r;
}

v3 point_at_parameter(Ray r, float t)
{
	v3 t_dir = v3_mulf(r.direction, t);
	v3 point = v3_add(t_dir, r.origin);

	return point;
}

f32 v3_squared_length(v3 a)
{
	return (a.x*a.x + a.y * a.y + a.z * a.z);
}

Ray get_ray(Camera cam, f32 u, f32 v)
{
	Ray ray;
	ray.origin = cam.eye;

	v3 u_norm = v3_mulf(cam.horizontal, u);
	v3 v_norm = v3_mulf(cam.vertical, v);
	v3 uv_norm = v3_add(u_norm, v_norm);
	v3 uv_cam = v3_add(uv_norm, cam.bottom_left);

	ray.direction = v3_sub(uv_cam, cam.eye);

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

f32 sphere(v3 centre, float radius, Ray r)
{
	v3	oc = v3_sub(r.origin, centre);
	f32		a = v3_dot(r.direction, r.direction);
	f32		b = 2.0f * v3_dot(oc, r.direction);
	f32		c = v3_dot(oc, oc) - radius * radius;
	f32		d = b * b - 4 * a*c;

	if (d < 0) return -1.0f;
	else return (-b - sqrt(d)) / (2.0f * a);
}

v3 random_point_within_magnitude(f32 mag)
{
	while (1)
	{
		v3 pos;
		pos.x = ((f32)rand() / (RAND_MAX));
		pos.y = ((f32)rand() / (RAND_MAX));
		pos.z = ((f32)rand() / (RAND_MAX));

		v3 point = v3_mulf(pos, 2.0f);

		v3 unit = vec3(1.0f, 1.0f, 1.0f);

		v3 final = v3_sub(point, unit);

		if (v3_squared_length(final) < mag)
		{
			return final;
		}
	}
}

u32 intersection(Ray* r, Sphere s, Hit* h, float t_min, float t_max)
{
	v3	oc = v3_sub(r->origin, s.position);
	f32	a  = v3_dot(r->direction, r->direction);
	f32	b  = v3_dot(oc, r->direction);
	f32	c  = v3_dot(oc, oc) - s.radius * s.radius;

	f32	d  = b * b*a*c;

	if (d > 0)
	{
		float temp = 0.0f;
		temp = (-b - sqrt(b*b - a * c)) / a;

		if (temp < t_max && temp > t_min)
		{
			h->t = temp;
			h->point = point_at_parameter(*r, temp);

			v3 dir = v3_sub(h->point, s.position);
			h->normal = v3_div(dir, s.radius);

			return 1;
		}

		temp = (-b + sqrt(b*b - a * c)) / a;

		if (temp < t_max && temp > t_min)
		{
			h->t = temp;
			h->point = point_at_parameter(*r, temp);

			v3 dir = v3_sub(h->point, s.position);
			h->normal = v3_div(dir, s.radius);

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
			h->material = spheres[i].material;
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

	Material object_sphere_material;
	object_sphere_material.type = LAMBERT;
	object_sphere_material.albedo = vec3(0.549f, 0.658f, -0.218f);

	f32 object_sphere_radius = 0.5f;
	v3 object_sphere_position = vec3(0.0f, 0.0f, -1.0f);

	Material ground_sphere_material;
	ground_sphere_material.type = LAMBERT;
	ground_sphere_material.albedo = vec3(1.0f, 1.0f, 1.0f);

	f32 ground_sphere_radius = 100.0f;
	v3 ground_sphere_position = vec3(0.0f, -100.5f, -1.0f);

	spheres[0].position = object_sphere_position;
	spheres[0].radius = object_sphere_radius;
	spheres[0].material = object_sphere_material;

	spheres[1].position = ground_sphere_position;
	spheres[1].radius = ground_sphere_radius;
	spheres[1].material = ground_sphere_material;
}

void setup_camera(void)
{
	camera.eye = vec3(0.0f, 0.0f, 0.0f);
	camera.bottom_left = vec3(-2.0f, -1.0f, -1.0f);
	camera.horizontal = vec3(4.0f, 0.0f, 0.0f);
	camera.vertical = vec3(0.0f, 2.0f, 0.0f);
}

v3 colour(Ray r)
{
	Hit h;

	if (intersects_all(r, &h, 0.001f, FLT_MAX))
	{
		switch (h.material.type)
		{
			case LAMBERT:
			{
				v3 random_unit_vector = random_point_within_magnitude(1.0f);
				v3 point_pos = v3_add(h.point, h.normal);
				v3 target = v3_add(point_pos, random_unit_vector);

				Ray ray;
				ray.origin = h.point;
				ray.direction = v3_sub(target, h.point);

				v3 c = colour(ray);
				v3 lambert = v3_mulv(c, h.material.albedo);

				return lambert;
			}

			case METAL:
			{
				v3 ray_dir_n = v3_normalized(r.direction);
				v3 reflected = v3_reflect(ray_dir_n, h.normal);
			
				Ray scattered; 
				scattered.origin = h.point;
				scattered.direction = reflected;

				f32 v = v3_dot(scattered.direction, h.normal);
				v3	c = colour(scattered);
				v3  calb = v3_mulv(c, h.material.albedo);

				v3 metal = (v > 0.0f) ? calb : vec3(0.0f, 0.0f, 0.0f);

				return metal;
			}
		}
	}
	else
	{
		v3 dir_n = v3_normalized(r.direction);

		f32 t = 0.5f * (dir_n.y + 1.0f);

		v3 white = vec3(1.0f, 1.0f, 1.0f);
		v3 blue = vec3(0.5f, 0.7f, 1.0f);

		v3 white_component = v3_mulf(white, 1.0f - t);
		v3 blue_component = v3_mulf(blue, t);
		v3 gradient = v3_add(white_component, blue_component);

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
	v3	col;
	col.x = 0.0f;
	col.y = 0.0f;
	col.z = 0.0f;

	u32	num_aa_samples = 8;

	switch (AA_SAMPLESTYLE)
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

				v3 c = colour(r);
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
			f32 u = (x + ((f32)rand() / (RAND_MAX))) / (f32)output_width;
			f32 v = (y + ((f32)rand() / (RAND_MAX))) / (f32)output_height;

			Ray r = get_ray(camera, u, v);

			v3 c = colour(r);
			col.x += c.x;
			col.y += c.y;
			col.z += c.z;
		}
	}
	}

	v3 final = v3_div(col, (f32)num_aa_samples);

	u8 red = (int)(255.99 * sqrt(final.x));
	u8 grn = (int)(255.99 * sqrt(final.y));
	u8 blu = (int)(255.99 * sqrt(final.z));
	u8 res = (int)0;

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
	u32* d = (u32*)data;

	u16 offset = (*d >> 16);
	u16 increment = *d;

	render(offset, increment);

	return 0;
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
	WNDCLASSEX wc;
	MSG msg;

	v3 first;
	first.x = 1.0f;
	first.y = 1.0f;
	first.z = 1.0f;

	v3 second;
	second.x = 2.0f;
	second.y = 2.0f;
	second.z = 2.0f;

	v3 a = v3_add(first, second);
	v3 b = v3_div(first, 2.0f);
	v3 c = v3_mulf(first, 0.5f);
	v3 d = v3_sub(first, second);

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

				if (FINISHED_MESSAGE) MessageBox(NULL, s, "Renderer", MB_ICONEXCLAMATION | MB_OK);

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