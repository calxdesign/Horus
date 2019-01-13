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
	
//#define FINISHED_MESSAGE		
#define MULTITHREADED		
#define OUTPUT					
#define SEED_OVERRIDE			46556
#define NUM_COLOURS				5
#define	NUM_SPHERES 			256	
#define NUM_AA_SAMPLES 			1	
#define OUTPUT_WIDTH			512
#define OUTPUT_HEIGHT			256
#define	OUTPUTSIZE				OUTPUT_WIDTH * OUTPUT_HEIGHT
#define ASPECT					OUTPUT_WIDTH / OUTPUT_HEIGHT
#define V_FOV					50
#define	RENDER					0
#define	IDLE					1
#define MAX_BOUNCES				50
#define CAM_POS_X				0.00f
#define CAM_POS_Y				0.70f
#define CAM_POS_Z			   -1.450f
#define CAM_TARGET_X			0.00f
#define CAM_TARGET_Y			0.47f
#define CAM_TARGET_Z			0.00f
#define CAM_APERTURE			0.05f

typedef enum MaterialType
{
	METAL, LAMBERT, CHECKER, LIGHT

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
	v3	origin;
	v3	direction;
	u32 bounces;
	u32 id;

} Ray;

typedef struct Material
{
	MaterialType type;
	v3			 albedo;
	f32			 intensity;
	f32			 fuzz;

} Material;

typedef struct Sphere
{
	v3			position;
	f32			radius;
	Material	material;

} Sphere;

typedef struct Camera
{
	v3  bottom_left;
	v3  position;
	v3  horizontal;
	v3  vertical;
	f32 lens_radius;
	v3 u;
	v3 v;
	v3 w;

} Camera;

typedef struct Hit
{
	f32			t;
	v3			point;
	v3			normal;
	Material	material;

} Hit;

static HWND						hwnd;
static Sphere*					spheres;
static v3*						palette;
static u8*						bitmap_image_data;
static HBITMAP					bitmap_handle;
static HDC						device_context;
static BITMAPINFO				bitmapinfo;
static BitmapFileHeader			file_header;
static BitmapInfoHeader			info_header;
static Camera					camera;
static u8						STATE = RENDER;
static s32						SEED;
static char						path[128];
static f64						render_time;

const char class_name[] = "Horus Path-Tracer";

f32 fract(f32 x)
{
	return x - (s64)x;
}

f32 hash(f32 x)
{
	return abs(fract(sin(x) * 43758.5453));
}

f32 ffmin(f32 a, f32 b)
{
	return a < b ? a : b;
}

f32 ffmax(f32 a, f32 b)
{
	return a > b ? a : b;
}

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

v3 v3_cross(v3 a, v3 b)
{
	v3 v;

	v.x = a.y * b.z - b.y * a.z;
	v.y = a.z * b.x - b.z * a.x;
	v.z = a.x * b.y - b.x * a.y;

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

f32 nrand()
{
	return ((f32)rand() / ((f32) RAND_MAX));
}

u32 ray_index = 0;

Ray get_ray(Camera* cam, f32 s, f32 t)
{
	v3 lens_ray_offset = vec3(0.0f, 0.0f, 0.0f);

	while (1)
	{
		v3 pos = vec3(nrand(), nrand(), 0.0f);
		v3 offset = vec3(1.0f, 1.0f, 0.0f);
		v3 point = v3_sub(pos, offset);
		v3 lens_point = v3_mulf(point, 2.0f);

		if (v3_dot(lens_point, lens_point) < 1.0f)
		{
			lens_ray_offset = v3_mulf(lens_point, cam->lens_radius);
			break;
		}
	}

	v3 ray_origin_offset_u = v3_mulf(cam->u, lens_ray_offset.x);
	v3 ray_origin_offset_v = v3_mulf(cam->v, lens_ray_offset.y);
	v3 final_ray_origin_offset = v3_add(ray_origin_offset_u, ray_origin_offset_v);

	Ray ray;
	ray.origin = v3_add(cam->position, final_ray_origin_offset);

	v3 u_norm = v3_mulf(cam->horizontal, s);
	v3 v_norm = v3_mulf(cam->vertical, t);
	v3 uv_norm = v3_add(u_norm, v_norm);
	v3 uv_cam = v3_add(uv_norm, cam->bottom_left);

	ray.direction = v3_sub(uv_cam, ray.origin);
	ray.bounces = 0;

	ray.id = ray_index;

	ray_index++;

	return ray;
}

void save_file(void)
{
	char* system_path = "D:\\Root\\Horus_Renders\\";
	char* prefix = "render_";
	char* suffix = ".bmp";

	sprintf(path, "%s%i%s", system_path, SEED, "\\");

	_mkdir(path);

	char filepath_and_name[128];

	sprintf(filepath_and_name, "%s%s%s%i%s", path, "\\", prefix, SEED, suffix);

	FILE* file = fopen(filepath_and_name, "wb");

	if (!file) return;

	fwrite(&file_header, sizeof(BitmapFileHeader), 1, file);
	fwrite(&info_header, sizeof(BitmapInfoHeader), 1, file);
	fwrite(bitmap_image_data, info_header.image_size, 1, file);

	fclose(file);

	char log_filename_and_path[128];

	sprintf(log_filename_and_path, "%s%s%i%s", path, "data_", SEED, ".txt");

	FILE* log;

	log = fopen(log_filename_and_path, "w");

	fprintf(log, "SEED			%i\n\n", SEED);
	fprintf(log, "NUM_COLOURS		%i\n", NUM_COLOURS);
	fprintf(log, "NUM_SPHERES:		%i\n", NUM_SPHERES);
	fprintf(log, "NUM_AA_SAMPLES: 	%i\n", NUM_AA_SAMPLES);
	fprintf(log, "OUTPUT_WIDTH:		%i\n", OUTPUT_WIDTH);
	fprintf(log, "OUTPUT_HEIGHT:		%i\n", OUTPUT_HEIGHT);
	fprintf(log, "ASPECT:			%i\n", ASPECT);
	fprintf(log, "V_FOV:			%i\n", V_FOV);
	fprintf(log, "RENDER_TIME:		%f s\n", render_time);
	fprintf(log, "MAX_BOUNCES:		%i\n", MAX_BOUNCES);
	fprintf(log, "CAM_POS_X:		%f\n", CAM_POS_X);
	fprintf(log, "CAM_POS_Y:		%f\n", CAM_POS_Y);
	fprintf(log, "CAM_POS_Z:		%f\n", CAM_POS_Z);
	fprintf(log, "CAM_TARGET_X:		%f\n", CAM_TARGET_X);
	fprintf(log, "CAM_TARGET_Z:		%f\n", CAM_TARGET_Z);
	fprintf(log, "CAM_APERTURE:		%f\n", CAM_APERTURE);

	fclose(log);
}

v3 random_unit_sphere(void)
{
	while (1)
	{
		v3 pos = vec3(nrand(), nrand(), nrand());
		v3 point = v3_mulf(pos, 2.0f);
		v3 unit = vec3(1.0f, 1.0f, 1.0f);
		v3 final = v3_sub(point, unit);

		if (v3_squared_length(final) < 1.0f)
		{
			return final;
		}
	}
}

u32 intersection(Ray* r, Sphere s, Hit* h, float t_min, float t_max)
{
	v3	i  = v3_sub(r->origin, s.position);
	f32	a  = v3_dot(r->direction, r->direction);
	f32	b  = v3_dot(i, r->direction);
	f32	c  = v3_dot(i, i) - s.radius * s.radius;
	f32	d  = b*b*a*c;

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


u32 intersects_all(Ray r, Hit* h, float t_min, float t_max, Sphere* first, s32 count)
{
	Hit		temp;
	u32		hit_something = 0;
	f32		closest = t_max;

	Sphere* sphere_end = (first + (count-1));

	for (Sphere* sphere = first; sphere != sphere_end; sphere++)
	{
		if (intersection(&r, *sphere, &temp, t_min, closest))
		{
			hit_something = 1;
			closest = temp.t;

			h->t = temp.t;
			h->point = temp.point;
			h->normal = temp.normal;
			h->material = sphere->material;
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

v3 get_random_colour_in_hue(f32 x, f32 y, f32 z)
{
	v3 c = vec3(0.0f, 0.0f, 0.0f);
	c.x = x * (0.392);
	c.y = x * (0.627 + (y * 0.176));
	c.z = x * (0.392 + (z * 0.607));

	return c;
}

void setup_camera(Camera* camera, v3 pos, v3 target, v3 up, f32 vertical_fov, f32 aspect, f32 aperture, f32 focus_distance)
{
	camera->lens_radius = aperture / 2.0f;

	f32 theta		= vertical_fov * (3.14159265358979323846f / 180.0f);
	f32 half_height = tan(theta / 2.0f);
	f32 half_width	= aspect * half_height;

	camera->position = pos;

	v3	dir = v3_sub(pos, target);
	camera->w = v3_normalized(dir);

	v3	cp_dir = v3_cross(up, camera->w);
	camera->u  = v3_normalized(cp_dir);

	camera->v  = v3_cross(camera->w, camera->u);

	v3	hwu	  = v3_mulf(camera->u, half_width*focus_distance);
	v3	hhv	  = v3_mulf(camera->v, half_height*focus_distance);
	v3	ht    = v3_add(hwu, hhv);
	v3  fdw   = v3_mulf(camera->w, focus_distance);
	v3	total = v3_add(ht, fdw);

	camera->bottom_left	= v3_sub(pos, total);
	camera->horizontal	= v3_mulf(camera->u, 2.0f*focus_distance*half_width);
	camera->vertical	= v3_mulf(camera->v, 2.0f*focus_distance*half_height);
}

v3 colour(Ray r)
{
	Hit h;

	if (intersects_all(r, &h, 0.001f, FLT_MAX, spheres, NUM_SPHERES))
	{
		if (r.bounces < MAX_BOUNCES)
		{
			switch (h.material.type)
			{
				case LAMBERT:
				{
					v3 random_unit_vector = random_unit_sphere();
					v3 point_pos = v3_add(h.point, h.normal);
					v3 target = v3_add(point_pos, random_unit_vector);

					Ray ray;
					ray.origin = h.point;
					ray.direction = v3_sub(target, h.point);
					ray.bounces = r.bounces + 1;

					v3 c = colour(ray);
					v3 lambert = v3_mulv(c, h.material.albedo);

					return lambert;
				}

				case METAL:
				{
					v3 ray_dir_n = v3_normalized(r.direction);
					v3 reflected = v3_reflect(ray_dir_n, h.normal);
					v3 rnd_fuzz = v3_mulf(random_unit_sphere(), h.material.fuzz);
					v3 final_reflection_dir = v3_add(reflected, rnd_fuzz);

					Ray scattered;
					scattered.origin = h.point;
					scattered.direction = final_reflection_dir;
					scattered.bounces = r.bounces + 1;

					f32 v = v3_dot(scattered.direction, h.normal);
					v3	c = colour(scattered);
					v3  calb = v3_mulv(c, h.material.albedo);

					v3 metal = (v > 0.0f) ? calb : vec3(0.0f, 0.0f, 0.0f);

					return metal;
				}

				case CHECKER:
				{
					v3 random_unit_vector = random_unit_sphere();
					v3 point_pos = v3_add(h.point, h.normal);
					v3 target = v3_add(point_pos, random_unit_vector);

					Ray ray;
					ray.origin = h.point;
					ray.direction = v3_sub(target, h.point);
					ray.bounces = r.bounces + 1;

					v3 c = colour(ray);

					f32 sines = (sin(10.0f*h.point.x) * sin(10.0f*h.point.y) * sin(10.0f*h.point.z) + 1.0f);
					u8  trunc = sines;
					v3  check_1		= vec3(0.1f, 0.1f, 0.1f);
					v3  check_2		= vec3(0.9f, 0.9, 0.9f);
					v3  check_col	= trunc ? check_1 : check_2;
					v3	check = v3_mulv(c, check_col);

					return check;
				}

				case LIGHT:
				{
					return  h.material.albedo;
				}
			}
		}
		else
		{
			return vec3(0.0f, 0.0f, 0.0f);
		}
	}
	else
	{
		v3 dir_n = v3_normalized(r.direction);

		f32 t = 0.5f * (dir_n.y + 1.0f);

		//v3 lower = vec3(1.0f, 1.0f, 1.0f);
		//v3 upper = vec3(0.5f, 0.7f, 1.0f);

		v3 lower = vec3(0.470f, 0.211f, 0.184f);
		v3 upper = vec3(0.02f, 0.02f, 0.02f);

		v3 white_component = v3_mulf(lower, 1.0f-t);
		v3 blue_component = v3_mulf(upper, t);
		v3 gradient = v3_add(white_component, blue_component);

		return gradient;
	}
}

void setup_pallete(void)
{
	palette = malloc(NUM_COLOURS * sizeof(v3));

	*(palette + 0) = vec3(255.0f / 255.0f, 107.0f / 255.0f, 107.0f / 255.0f);
	*(palette + 1) = vec3(85.0f  / 255.0f, 98.0f  / 255.0f, 112.0f / 255.0f);
	*(palette + 2) = vec3(78.0f  / 255.0f, 205.0f / 255.0f, 198.0f / 255.0f);
	*(palette + 3) = vec3(198.0f / 255.0f, 77.0f  / 255.0f, 88.0f  / 255.0f);
	*(palette + 4) = vec3(199.0f / 255.0f, 244.0f / 255.0f, 100.0f / 255.0f);

	return;
}

v3 get_random_colour_in_pallete(void)
{
	u32 index = (u32) (nrand() * NUM_COLOURS);

	return palette[index];
}

void setup_scene(void)
{
	spheres = malloc(NUM_SPHERES * sizeof(Sphere));

	spheres->position = vec3(0.0f, -100000.0f, -0.0f);
	spheres->radius = 100000.0f;
	spheres->material.type = LAMBERT;
	spheres->material.albedo = vec3(0.5f, 0.5f, 0.5f);

	u32 sphere_count = 1;

	while (1)
	{
		if (sphere_count == NUM_SPHERES) break;

		(spheres + sphere_count)->radius = (nrand() * 0.25f) + 0.05f;
		(spheres + sphere_count)->position.x = (nrand() * 5.0f) - 2.5f;
		(spheres + sphere_count)->position.y = (spheres + sphere_count)->radius;
		(spheres + sphere_count)->position.z = (nrand() * 2.5f);
		(spheres + sphere_count)->material.type = (nrand() > 0.75f) ? ((nrand() > 0.65f) ? LIGHT : METAL) : LAMBERT;
		(spheres + sphere_count)->material.intensity = nrand();

		f32 brightness = 1.0f;

		(spheres + sphere_count)->material.albedo = (spheres + sphere_count)->material.type == METAL ? vec3(brightness, brightness, brightness) : get_random_colour_in_pallete();
		(spheres + sphere_count)->material.fuzz = nrand() * 0.25;

		int dismiss = 0;

		f32 inner = 0.00f;
		f32 outer = 4.05f;

		for (int i = 0; i < sphere_count; ++i)
		{
			v3  dir = v3_sub((spheres + sphere_count)->position, (spheres + i)->position);
			f32 dis = v3_mag(dir);
			v3  dir_to_sphere = v3_sub((spheres + sphere_count)->position, camera.position);
			f32 distance_to_cam = v3_mag(dir_to_sphere);

			if (dis < ((spheres + sphere_count)->radius + (spheres + i)->radius) || distance_to_cam < inner || distance_to_cam > outer)
			{
				dismiss = 1;
				break;
			}
		}

		if (dismiss == 0) sphere_count++;
	}
}

s32 intersects(f32 rect_x, f32 rect_y, f32 rect_width, f32 rect_height, f32 circle_x, f32 circle_y, f32 circle_radius)
{
	f32 delta_x = circle_x - max(rect_x, min(circle_x, rect_x + rect_width));
	f32 delta_y = circle_y - max(rect_y, min(circle_y, rect_y + rect_height));

	return ((delta_x * delta_x + delta_y * delta_y) < (circle_radius * circle_radius)) ? 1 : 0;
}

void setup_bitmap(void)
{
	u32 remainder = OUTPUT_WIDTH % 5;
	u32 row_size = (remainder == 0) ? (OUTPUT_WIDTH * 4) : ((OUTPUT_WIDTH + 4 - remainder) * 4);

	file_header.type = 0x4d42;
	file_header.size = (row_size * OUTPUT_HEIGHT) + sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
	file_header.res_1 = 0;
	file_header.res_2 = 0;
	file_header.offset = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

	device_context = CreateCompatibleDC(0);
	bitmap_handle = CreateDIBSection(device_context, (BITMAPINFO*)&file_header, DIB_RGB_COLORS, (void**)&bitmap_image_data, 0, 0);

	info_header.size = sizeof(BitmapInfoHeader);
	info_header.width = OUTPUT_WIDTH;
	info_header.height = OUTPUT_HEIGHT;
	info_header.planes = 1;
	info_header.bits = 32;
	info_header.compression = 0;
	info_header.image_size = row_size * OUTPUT_HEIGHT;
	info_header.x_resolution = 0;
	info_header.y_resolution = 0;
	info_header.colours = 0;
	info_header.colours_important = 0;

	bitmap_image_data = _aligned_malloc(info_header.image_size, 64);
}

void render_pixel(u32 x, u32 y)
{
	v3	col = vec3(0.0f, 0.0f, 0.0f);

	for (s16 sample = 0; sample < NUM_AA_SAMPLES; sample++)
	{
		f32 u = (x + nrand()) / (f32)OUTPUT_WIDTH;
		f32 v = (y + nrand()) / (f32)OUTPUT_HEIGHT;

		Ray r = get_ray(&camera, u, v);

		v3 c = colour(r);
		col.x += c.x;
		col.y += c.y;
		col.z += c.z;
	}
	
	v3 final = v3_div(col, (f32) NUM_AA_SAMPLES);

	u8 red = (int)(255.99 * sqrt(final.x));
	u8 grn = (int)(255.99 * sqrt(final.y));
	u8 blu = (int)(255.99 * sqrt(final.z));
	u8 res = (int)0;

	int bitmap_index = ((y * OUTPUT_WIDTH) + x) * 4;

	bitmap_image_data[bitmap_index + 0] = blu;
	bitmap_image_data[bitmap_index + 1] = grn;
	bitmap_image_data[bitmap_index + 2] = red;
	bitmap_image_data[bitmap_index + 3] = res;
}

void render(u16 offset, u16 inc)
{
	for (s16 x = 0; x < OUTPUT_WIDTH; x++)
	{
		for (s16 y = offset; y < OUTPUT_HEIGHT; y+= inc)
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
		Sleep(60);
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

	#ifndef SEED_OVERRIDE
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	SEED = tm.tm_hour * tm.tm_min * tm.tm_sec;
	#else
	SEED = SEED_OVERRIDE;
	#endif // !SEED_OVERRRIDE

	srand(SEED);

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

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, class_name, class_name, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, OUTPUT_WIDTH, OUTPUT_HEIGHT + 43, NULL, NULL, h_instance, NULL);

	if (!hwnd) return 0;

	ShowWindow(hwnd, cmd_show);
	UpdateWindow(hwnd);

	v3 cam_position = vec3(CAM_POS_X, CAM_POS_Y, CAM_POS_Z);
	v3 cam_target = vec3(CAM_TARGET_X, CAM_TARGET_Y, CAM_TARGET_Z);
	v3 world_up = vec3(0.0f, 1.0f, 0.0f);
	v3 cam_direction = v3_sub(cam_position, cam_target);
	f32 cam_focal_dist = v3_mag(cam_direction);

	setup_camera(&camera, cam_position, cam_target, world_up, V_FOV, ASPECT, CAM_APERTURE, cam_focal_dist);
	setup_pallete();
	setup_scene();
	setup_bitmap();
	
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	u16 cpu_count = 1; 

	#ifdef MULTITHREADED
	cpu_count = system_info.dwNumberOfProcessors;
	#endif // MULTITHREADED

	u32* render_thread_data = malloc(cpu_count * sizeof(u32));
	HANDLE* render_threads = malloc(cpu_count * sizeof(HANDLE));
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

				cpu_time_used = ((f64) (end - start)) / CLOCKS_PER_SEC;
				render_time = cpu_time_used;
				u8 s[32];

				#ifdef OUTPUT
				save_file();
				#endif // OUTPUT

				#ifdef FINISHED_MESSAGE
					sprintf(s, "Finished in %f", cpu_time_used);
					MessageBox(NULL, s, "Renderer", MB_ICONEXCLAMATION | MB_OK);
				#endif // FINISHED_MESSAGE

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