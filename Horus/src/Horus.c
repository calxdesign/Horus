#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#pragma pack(1)

typedef signed char			s8;
typedef char				u8;
typedef short				s16;
typedef unsigned short			u16;
typedef int				s32;
typedef unsigned int		u32;
typedef long long			s64;
typedef unsigned long long		u64;
typedef float				f32;
typedef double				f64;

#define FINISHED_MESSAGE		
#define NIGHT
#define MULTITHREADED		
#define OUTPUT					
#define SEED_OVERRIDE			46557
#define NUM_COLOURS				5
#define	NUM_SPHERES 			128	
#define NUM_AA_SAMPLES 			128	
#define OUTPUT_WIDTH			1024
#define OUTPUT_HEIGHT			512
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

v3 viridis(f32 f);

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
	return ((f32)rand() / ((f32)RAND_MAX));
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
	v3	i = v3_sub(r->origin, s.position);
	f32	a = v3_dot(r->direction, r->direction);
	f32	b = v3_dot(i, r->direction);
	f32	c = v3_dot(i, i) - s.radius * s.radius;
	f32	d = b * b*a*c;

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

	Sphere* sphere_end = (first + (count - 1));

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

void setup_camera(Camera* camera, v3 pos, v3 target, v3 up, f32 vertical_fov, f32 aspect, f32 aperture, f32 focus_distance)
{
	camera->lens_radius = aperture / 2.0f;

	f32 theta = vertical_fov * (3.14159265358979323846f / 180.0f);
	f32 half_height = tan(theta / 2.0f);
	f32 half_width = aspect * half_height;

	camera->position = pos;

	v3	dir = v3_sub(pos, target);
	camera->w = v3_normalized(dir);

	v3	cp_dir = v3_cross(up, camera->w);
	camera->u = v3_normalized(cp_dir);

	camera->v = v3_cross(camera->w, camera->u);

	v3	hwu = v3_mulf(camera->u, half_width*focus_distance);
	v3	hhv = v3_mulf(camera->v, half_height*focus_distance);
	v3	ht = v3_add(hwu, hhv);
	v3  fdw = v3_mulf(camera->w, focus_distance);
	v3	total = v3_add(ht, fdw);

	camera->bottom_left = v3_sub(pos, total);
	camera->horizontal = v3_mulf(camera->u, 2.0f*focus_distance*half_width);
	camera->vertical = v3_mulf(camera->v, 2.0f*focus_distance*half_height);
}

v3 colour(Ray r)
{
	Hit h;

	if (intersects_all(r, &h, 0.000000001f, FLT_MAX, spheres, NUM_SPHERES))
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
				v3  check_1 = vec3(0.1f, 0.1f, 0.1f);
				v3  check_2 = vec3(0.9f, 0.9, 0.9f);
				v3  check_col = trunc ? check_1 : check_2;
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

		v3 lower = vec3(1.0f, 1.0f, 1.0f);
		v3 upper = vec3(0.5f, 0.7f, 1.0f);

#ifdef NIGHT

		lower = vec3(0.270f, 0.211f, 0.184f);
		upper = vec3(0.020f, 0.020f, 0.020f);

#endif // NIGHT

		v3 white_component = v3_mulf(lower, 1.0f - t);
		v3 blue_component = v3_mulf(upper, t);
		v3 gradient = v3_add(white_component, blue_component);

		return gradient;
	}
}

void setup_pallete(void)
{
	palette = malloc(NUM_COLOURS * sizeof(v3));

	*(palette + 0) = vec3(255.0f / 255.0f, 107.0f / 255.0f, 107.0f / 255.0f);
	*(palette + 1) = vec3(85.0f / 255.0f, 98.0f / 255.0f, 112.0f / 255.0f);
	*(palette + 2) = vec3(78.0f / 255.0f, 205.0f / 255.0f, 198.0f / 255.0f);
	*(palette + 3) = vec3(198.0f / 255.0f, 77.0f / 255.0f, 88.0f / 255.0f);
	*(palette + 4) = vec3(199.0f / 255.0f, 244.0f / 255.0f, 100.0f / 255.0f);

	return;
}

v3 get_random_colour_in_pallete(void)
{
	u32 index = (u32)(nrand() * NUM_COLOURS);

	return palette[index];
}

void setup_scene(void)
{
	spheres = malloc(NUM_SPHERES * sizeof(Sphere));

	spheres->position = vec3(0.0f, -100000.0f, -0.0f);
	spheres->radius = 99999.995f;
	spheres->material.type = LAMBERT;
	spheres->material.albedo = vec3(0.2f, 0.2f, 0.2f);

	u32 sphere_count = 1;

	while (1)
	{
		if (sphere_count == NUM_SPHERES) break;

		f32 xpos = (nrand() * 5.0f) - 2.5f;;

		(spheres + sphere_count)->radius = (nrand() * 0.25f) + 0.05f;
		(spheres + sphere_count)->position.x = xpos;
		(spheres + sphere_count)->position.y = (spheres + sphere_count)->radius;
		(spheres + sphere_count)->position.z = (nrand() * 2.5f);
		(spheres + sphere_count)->material.type = (nrand() > 0.60f) ? ((nrand() > 0.35f) ? LIGHT : METAL) : LAMBERT;
		(spheres + sphere_count)->material.intensity = nrand();

		f32 brightness = 1.0f;

		(spheres + sphere_count)->material.albedo = (spheres + sphere_count)->material.type == METAL ? vec3(brightness, brightness, brightness) : viridis((xpos + 2.5) / 5.0f);
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

	v3 final = v3_div(col, (f32)NUM_AA_SAMPLES);

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
	for (s16 y = offset; y < OUTPUT_HEIGHT; y += inc)
	{
		for (s16 x = 0; x < OUTPUT_WIDTH; x++)
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

				cpu_time_used = ((f64)(end - start)) / CLOCKS_PER_SEC;
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

f32 viridis_data[256][3] =
{
{ 0.267004f, 0.004874f, 0.329415f },
{ 0.268510f, 0.009605f, 0.335427f },
{ 0.269944f, 0.014625f, 0.341379f },
{ 0.271305f, 0.019942f, 0.347269f },
{ 0.272594f, 0.025563f, 0.353093f },
{ 0.273809f, 0.031497f, 0.358853f },
{ 0.274952f, 0.037752f, 0.364543f },
{ 0.276022f, 0.044167f, 0.370164f },
{ 0.277018f, 0.050344f, 0.375715f },
{ 0.277941f, 0.056324f, 0.381191f },
{ 0.278791f, 0.062145f, 0.386592f },
{ 0.279566f, 0.067836f, 0.391917f },
{ 0.280267f, 0.073417f, 0.397163f },
{ 0.280894f, 0.078907f, 0.402329f },
{ 0.281446f, 0.084320f, 0.407414f },
{ 0.281924f, 0.089666f, 0.412415f },
{ 0.282327f, 0.094955f, 0.417331f },
{ 0.282656f, 0.100196f, 0.422160f },
{ 0.282910f, 0.105393f, 0.426902f },
{ 0.283091f, 0.110553f, 0.431554f },
{ 0.283197f, 0.115680f, 0.436115f },
{ 0.283229f, 0.120777f, 0.440584f },
{ 0.283187f, 0.125848f, 0.444960f },
{ 0.283072f, 0.130895f, 0.449241f },
{ 0.282884f, 0.135920f, 0.453427f },
{ 0.282623f, 0.140926f, 0.457517f },
{ 0.282290f, 0.145912f, 0.461510f },
{ 0.281887f, 0.150881f, 0.465405f },
{ 0.281412f, 0.155834f, 0.469201f },
{ 0.280868f, 0.160771f, 0.472899f },
{ 0.280255f, 0.165693f, 0.476498f },
{ 0.279574f, 0.170599f, 0.479997f },
{ 0.278826f, 0.175490f, 0.483397f },
{ 0.278012f, 0.180367f, 0.486697f },
{ 0.277134f, 0.185228f, 0.489898f },
{ 0.276194f, 0.190074f, 0.493001f },
{ 0.275191f, 0.194905f, 0.496005f },
{ 0.274128f, 0.199721f, 0.498911f },
{ 0.273006f, 0.204520f, 0.501721f },
{ 0.271828f, 0.209303f, 0.504434f },
{ 0.270595f, 0.214069f, 0.507052f },
{ 0.269308f, 0.218818f, 0.509577f },
{ 0.267968f, 0.223549f, 0.512008f },
{ 0.266580f, 0.228262f, 0.514349f },
{ 0.265145f, 0.232956f, 0.516599f },
{ 0.263663f, 0.237631f, 0.518762f },
{ 0.262138f, 0.242286f, 0.520837f },
{ 0.260571f, 0.246922f, 0.522828f },
{ 0.258965f, 0.251537f, 0.524736f },
{ 0.257322f, 0.256130f, 0.526563f },
{ 0.255645f, 0.260703f, 0.528312f },
{ 0.253935f, 0.265254f, 0.529983f },
{ 0.252194f, 0.269783f, 0.531579f },
{ 0.250425f, 0.274290f, 0.533103f },
{ 0.248629f, 0.278775f, 0.534556f },
{ 0.246811f, 0.283237f, 0.535941f },
{ 0.244972f, 0.287675f, 0.537260f },
{ 0.243113f, 0.292092f, 0.538516f },
{ 0.241237f, 0.296485f, 0.539709f },
{ 0.239346f, 0.300855f, 0.540844f },
{ 0.237441f, 0.305202f, 0.541921f },
{ 0.235526f, 0.309527f, 0.542944f },
{ 0.233603f, 0.313828f, 0.543914f },
{ 0.231674f, 0.318106f, 0.544834f },
{ 0.229739f, 0.322361f, 0.545706f },
{ 0.227802f, 0.326594f, 0.546532f },
{ 0.225863f, 0.330805f, 0.547314f },
{ 0.223925f, 0.334994f, 0.548053f },
{ 0.221989f, 0.339161f, 0.548752f },
{ 0.220057f, 0.343307f, 0.549413f },
{ 0.218130f, 0.347432f, 0.550038f },
{ 0.216210f, 0.351535f, 0.550627f },
{ 0.214298f, 0.355619f, 0.551184f },
{ 0.212395f, 0.359683f, 0.551710f },
{ 0.210503f, 0.363727f, 0.552206f },
{ 0.208623f, 0.367752f, 0.552675f },
{ 0.206756f, 0.371758f, 0.553117f },
{ 0.204903f, 0.375746f, 0.553533f },
{ 0.203063f, 0.379716f, 0.553925f },
{ 0.201239f, 0.383670f, 0.554294f },
{ 0.199430f, 0.387607f, 0.554642f },
{ 0.197636f, 0.391528f, 0.554969f },
{ 0.195860f, 0.395433f, 0.555276f },
{ 0.194100f, 0.399323f, 0.555565f },
{ 0.192357f, 0.403199f, 0.555836f },
{ 0.190631f, 0.407061f, 0.556089f },
{ 0.188923f, 0.410910f, 0.556326f },
{ 0.187231f, 0.414746f, 0.556547f },
{ 0.185556f, 0.418570f, 0.556753f },
{ 0.183898f, 0.422383f, 0.556944f },
{ 0.182256f, 0.426184f, 0.557120f },
{ 0.180629f, 0.429975f, 0.557282f },
{ 0.179019f, 0.433756f, 0.557430f },
{ 0.177423f, 0.437527f, 0.557565f },
{ 0.175841f, 0.441290f, 0.557685f },
{ 0.174274f, 0.445044f, 0.557792f },
{ 0.172719f, 0.448791f, 0.557885f },
{ 0.171176f, 0.452530f, 0.557965f },
{ 0.169646f, 0.456262f, 0.558030f },
{ 0.168126f, 0.459988f, 0.558082f },
{ 0.166617f, 0.463708f, 0.558119f },
{ 0.165117f, 0.467423f, 0.558141f },
{ 0.163625f, 0.471133f, 0.558148f },
{ 0.162142f, 0.474838f, 0.558140f },
{ 0.160665f, 0.478540f, 0.558115f },
{ 0.159194f, 0.482237f, 0.558073f },
{ 0.157729f, 0.485932f, 0.558013f },
{ 0.156270f, 0.489624f, 0.557936f },
{ 0.154815f, 0.493313f, 0.557840f },
{ 0.153364f, 0.497000f, 0.557724f },
{ 0.151918f, 0.500685f, 0.557587f },
{ 0.150476f, 0.504369f, 0.557430f },
{ 0.149039f, 0.508051f, 0.557250f },
{ 0.147607f, 0.511733f, 0.557049f },
{ 0.146180f, 0.515413f, 0.556823f },
{ 0.144759f, 0.519093f, 0.556572f },
{ 0.143343f, 0.522773f, 0.556295f },
{ 0.141935f, 0.526453f, 0.555991f },
{ 0.140536f, 0.530132f, 0.555659f },
{ 0.139147f, 0.533812f, 0.555298f },
{ 0.137770f, 0.537492f, 0.554906f },
{ 0.136408f, 0.541173f, 0.554483f },
{ 0.135066f, 0.544853f, 0.554029f },
{ 0.133743f, 0.548535f, 0.553541f },
{ 0.132444f, 0.552216f, 0.553018f },
{ 0.131172f, 0.555899f, 0.552459f },
{ 0.129933f, 0.559582f, 0.551864f },
{ 0.128729f, 0.563265f, 0.551229f },
{ 0.127568f, 0.566949f, 0.550556f },
{ 0.126453f, 0.570633f, 0.549841f },
{ 0.125394f, 0.574318f, 0.549086f },
{ 0.124395f, 0.578002f, 0.548287f },
{ 0.123463f, 0.581687f, 0.547445f },
{ 0.122606f, 0.585371f, 0.546557f },
{ 0.121831f, 0.589055f, 0.545623f },
{ 0.121148f, 0.592739f, 0.544641f },
{ 0.120565f, 0.596422f, 0.543611f },
{ 0.120092f, 0.600104f, 0.542530f },
{ 0.119738f, 0.603785f, 0.541400f },
{ 0.119512f, 0.607464f, 0.540218f },
{ 0.119423f, 0.611141f, 0.538982f },
{ 0.119483f, 0.614817f, 0.537692f },
{ 0.119699f, 0.618490f, 0.536347f },
{ 0.120081f, 0.622161f, 0.534946f },
{ 0.120638f, 0.625828f, 0.533488f },
{ 0.121380f, 0.629492f, 0.531973f },
{ 0.122312f, 0.633153f, 0.530398f },
{ 0.123444f, 0.636809f, 0.528763f },
{ 0.124780f, 0.640461f, 0.527068f },
{ 0.126326f, 0.644107f, 0.525311f },
{ 0.128087f, 0.647749f, 0.523491f },
{ 0.130067f, 0.651384f, 0.521608f },
{ 0.132268f, 0.655014f, 0.519661f },
{ 0.134692f, 0.658636f, 0.517649f },
{ 0.137339f, 0.662252f, 0.515571f },
{ 0.140210f, 0.665859f, 0.513427f },
{ 0.143303f, 0.669459f, 0.511215f },
{ 0.146616f, 0.673050f, 0.508936f },
{ 0.150148f, 0.676631f, 0.506589f },
{ 0.153894f, 0.680203f, 0.504172f },
{ 0.157851f, 0.683765f, 0.501686f },
{ 0.162016f, 0.687316f, 0.499129f },
{ 0.166383f, 0.690856f, 0.496502f },
{ 0.170948f, 0.694384f, 0.493803f },
{ 0.175707f, 0.697900f, 0.491033f },
{ 0.180653f, 0.701402f, 0.488189f },
{ 0.185783f, 0.704891f, 0.485273f },
{ 0.191090f, 0.708366f, 0.482284f },
{ 0.196571f, 0.711827f, 0.479221f },
{ 0.202219f, 0.715272f, 0.476084f },
{ 0.208030f, 0.718701f, 0.472873f },
{ 0.214000f, 0.722114f, 0.469588f },
{ 0.220124f, 0.725509f, 0.466226f },
{ 0.226397f, 0.728888f, 0.462789f },
{ 0.232815f, 0.732247f, 0.459277f },
{ 0.239374f, 0.735588f, 0.455688f },
{ 0.246070f, 0.738910f, 0.452024f },
{ 0.252899f, 0.742211f, 0.448284f },
{ 0.259857f, 0.745492f, 0.444467f },
{ 0.266941f, 0.748751f, 0.440573f },
{ 0.274149f, 0.751988f, 0.436601f },
{ 0.281477f, 0.755203f, 0.432552f },
{ 0.288921f, 0.758394f, 0.428426f },
{ 0.296479f, 0.761561f, 0.424223f },
{ 0.304148f, 0.764704f, 0.419943f },
{ 0.311925f, 0.767822f, 0.415586f },
{ 0.319809f, 0.770914f, 0.411152f },
{ 0.327796f, 0.773980f, 0.406640f },
{ 0.335885f, 0.777018f, 0.402049f },
{ 0.344074f, 0.780029f, 0.397381f },
{ 0.352360f, 0.783011f, 0.392636f },
{ 0.360741f, 0.785964f, 0.387814f },
{ 0.369214f, 0.788888f, 0.382914f },
{ 0.377779f, 0.791781f, 0.377939f },
{ 0.386433f, 0.794644f, 0.372886f },
{ 0.395174f, 0.797475f, 0.367757f },
{ 0.404001f, 0.800275f, 0.362552f },
{ 0.412913f, 0.803041f, 0.357269f },
{ 0.421908f, 0.805774f, 0.351910f },
{ 0.430983f, 0.808473f, 0.346476f },
{ 0.440137f, 0.811138f, 0.340967f },
{ 0.449368f, 0.813768f, 0.335384f },
{ 0.458674f, 0.816363f, 0.329727f },
{ 0.468053f, 0.818921f, 0.323998f },
{ 0.477504f, 0.821444f, 0.318195f },
{ 0.487026f, 0.823929f, 0.312321f },
{ 0.496615f, 0.826376f, 0.306377f },
{ 0.506271f, 0.828786f, 0.300362f },
{ 0.515992f, 0.831158f, 0.294279f },
{ 0.525776f, 0.833491f, 0.288127f },
{ 0.535621f, 0.835785f, 0.281908f },
{ 0.545524f, 0.838039f, 0.275626f },
{ 0.555484f, 0.840254f, 0.269281f },
{ 0.565498f, 0.842430f, 0.262877f },
{ 0.575563f, 0.844566f, 0.256415f },
{ 0.585678f, 0.846661f, 0.249897f },
{ 0.595839f, 0.848717f, 0.243329f },
{ 0.606045f, 0.850733f, 0.236712f },
{ 0.616293f, 0.852709f, 0.230052f },
{ 0.626579f, 0.854645f, 0.223353f },
{ 0.636902f, 0.856542f, 0.216620f },
{ 0.647257f, 0.858400f, 0.209861f },
{ 0.657642f, 0.860219f, 0.203082f },
{ 0.668054f, 0.861999f, 0.196293f },
{ 0.678489f, 0.863742f, 0.189503f },
{ 0.688944f, 0.865448f, 0.182725f },
{ 0.699415f, 0.867117f, 0.175971f },
{ 0.709898f, 0.868751f, 0.169257f },
{ 0.720391f, 0.870350f, 0.162603f },
{ 0.730889f, 0.871916f, 0.156029f },
{ 0.741388f, 0.873449f, 0.149561f },
{ 0.751884f, 0.874951f, 0.143228f },
{ 0.762373f, 0.876424f, 0.137064f },
{ 0.772852f, 0.877868f, 0.131109f },
{ 0.783315f, 0.879285f, 0.125405f },
{ 0.793760f, 0.880678f, 0.120005f },
{ 0.804182f, 0.882046f, 0.114965f },
{ 0.814576f, 0.883393f, 0.110347f },
{ 0.824940f, 0.884720f, 0.106217f },
{ 0.835270f, 0.886029f, 0.102646f },
{ 0.845561f, 0.887322f, 0.099702f },
{ 0.855810f, 0.888601f, 0.097452f },
{ 0.866013f, 0.889868f, 0.095953f },
{ 0.876168f, 0.891125f, 0.095250f },
{ 0.886271f, 0.892374f, 0.095374f },
{ 0.896320f, 0.893616f, 0.096335f },
{ 0.906311f, 0.894855f, 0.098125f },
{ 0.916242f, 0.896091f, 0.100717f },
{ 0.926106f, 0.897330f, 0.104071f },
{ 0.935904f, 0.898570f, 0.108131f },
{ 0.945636f, 0.899815f, 0.112838f },
{ 0.955300f, 0.901065f, 0.118128f },
{ 0.964894f, 0.902323f, 0.123941f },
{ 0.974417f, 0.903590f, 0.130215f },
{ 0.983868f, 0.904867f, 0.136897f },
{ 0.993248f, 0.906157f, 0.143936f } };

v3 viridis(f32 f)
{
	s32 index = (s32)(f * 256);

	return vec3(viridis_data[index][0], viridis_data[index][1], viridis_data[index][2]);
}