#include "../stdafx.h"

#include "cm_terragen.hpp"

#include "3rdparty/delaunator.hpp"

#include "../clear_map.h"
#include "../core/math_func.hpp"
#include "../core/random_func.hpp"
#include "../genworld.h"
#include "../gfx_type.h"
#include "../tgp.h" // TODO remove
#include "../void_map.h"

#include <fftw3.h>

#include <cmath>
#include <map>
#include <vector>
#include <queue>

#include "../safeguards.h"


extern CommandCost CmdBuildObject(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text);

typedef int16 height_t;
/** Height map - allocated array of heights (MapSizeX() + 1) x (MapSizeY() + 1) */
struct HeightMap
{
	height_t *h;         //< array of heights
	/* Even though the sizes are always positive, there are many cases where
	 * X and Y need to be signed integers due to subtractions. */
	int      dim_x;      //< height map size_x MapSizeX() + 1
	int      total_size; //< height map total size
	int      size_x;     //< MapSizeX()
	int      size_y;     //< MapSizeY()

	/**
	 * Height map accessor
	 * @param x X position
	 * @param y Y position
	 * @return height as fixed point number
	 */
	inline height_t &height(uint x, uint y)
	{
		return h[x + y * dim_x];
	}
};
/** Global height map instance */

/** Global height map instance */
static HeightMap _height_map = {nullptr, 0, 0, 0, 0};

static const int height_decimal_bits = 4;

/** Conversion: int to height_t */
#define I2H(i) ((i) << height_decimal_bits)
/** Conversion: height_t to int */
#define H2I(i) ((i) >> height_decimal_bits)

/** Walk through all items of _height_map.h */
#define FOR_ALL_TILES_IN_HEIGHT(h) for (h = _height_map.h; h < &_height_map.h[_height_map.total_size]; h++)

static inline bool AllocHeightMap()
{
	height_t *h;

	_height_map.size_x = MapSizeX();
	_height_map.size_y = MapSizeY();

	/* Allocate memory block for height map row pointers */
	_height_map.total_size = (_height_map.size_x + 1) * (_height_map.size_y + 1);
	_height_map.dim_x = _height_map.size_x + 1;
	_height_map.h = CallocT<height_t>(_height_map.total_size);

	/* Iterate through height map and initialise values. */
	FOR_ALL_TILES_IN_HEIGHT(h) *h = 0;

	return true;
}

/** Free height map */
static inline void FreeHeightMap()
{
	free(_height_map.h);
	_height_map.h = nullptr;
}

/** A small helper function to initialize the terrain */
static void TgenSetTileHeight(TileIndex tile, int height)
{
	SetTileHeight(tile, height);

	/* Only clear the tiles within the map area. */
	if (IsInnerTile(tile)) {
		MakeClear(tile, CLEAR_GRASS, 3);
	}
}

namespace citymania {

typedef void ScreenshotCallback(void *userdata, void *buf, uint y, uint pitch, uint n);
extern bool (*MakePNGImage)(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette);

namespace terragen {

struct Point {
public:
	typedef double coord_t;
	coord_t x;
	coord_t y;

	Point(coord_t x, coord_t y): x{x}, y{y} {}
	Point(): Point(0.0, 0.0) {}
};

static const Point::coord_t SQRT1_2 = 0.7071067811865476;
static const Point::coord_t PI = 3.14159265358979323846;


static const uint DEBUG_IMG_SCALE = 8;

void plot_points_callback(void *userdata, void *buffer, uint y, uint pitch, uint n) {
	auto img = static_cast<uint32 *>(userdata);
	auto buf = static_cast<uint32 *>(buffer);
	for (uint j = 0; j < n; j++)
		for (uint i = 0; i < pitch; i++) {
			buf[j * pitch + i] = img[(j + y) * pitch + i];
		}
}

void plot_points(std::vector<Point> &grid, uint width, uint height, const char *filename) {
	std::vector<uint32> img(width * height * DEBUG_IMG_SCALE * DEBUG_IMG_SCALE);
	for (auto p: grid) {
		if (p.x || p.y) {
			img[(uint)(p.x * DEBUG_IMG_SCALE) + (uint)(p.y * DEBUG_IMG_SCALE) * width * DEBUG_IMG_SCALE] = 0x7f7676;
		}
	}
	MakePNGImage(filename, plot_points_callback, (void *)(&img[0]), width * DEBUG_IMG_SCALE, height * DEBUG_IMG_SCALE, 32, nullptr);
}

void fft_plot_callback(void *userdata, void *buffer, uint y, uint pitch, uint n) {
	auto fft = static_cast<fftw_complex *>(userdata);
	auto buf = static_cast<uint32 *>(buffer);
	for (uint j = 0; j < n; j++)
		for (uint i = 0; i < pitch; i++) {
			uint32 color = (uint8)(fft[(j + y) * pitch + i][0] * 255);
			if (fft[(j + y) * pitch + i][0] < 0.1) color = 0;
			buf[j * pitch + i] = color << 8;
		}
}

size_t next_halfedge(size_t e) { return (e % 3 == 2) ? e - 2 : e + 1; }
size_t prev_halfedge(size_t e) { return (e % 3 == 0) ? e + 2 : e - 1; }

void plot_line(std::vector<uint32> &img, delaunator::Delaunator &delaunay, uint width, size_t a, size_t b, uint32 color, int subdivisions = 1) {
	a *= 2; b *= 2;
	for (uint j = 0; j < DEBUG_IMG_SCALE * subdivisions; j++) {
		auto k = DEBUG_IMG_SCALE * subdivisions - j;
		auto x = (uint)((delaunay.coords[a] * j + delaunay.coords[b] * k) / subdivisions);
		auto y = (uint)((delaunay.coords[a + 1] * j + delaunay.coords[b + 1] * k) / subdivisions);
		img[y * width * DEBUG_IMG_SCALE + x] = color;
	}
}

void plot_edge(std::vector<uint32> &img, delaunator::Delaunator &delaunay, uint width, size_t e, uint32 color, int subdivisions = 1) {
	plot_line(img, delaunay, width, delaunay.triangles[e], delaunay.triangles[next_halfedge(e)], color, subdivisions);
}

void plot_delaunay(delaunator::Delaunator &delaunay, uint width, uint height, const char *filename) {
	std::vector<uint32> img(width * height * DEBUG_IMG_SCALE * DEBUG_IMG_SCALE);
    // for(std::size_t i = 0; i < delaunay.triangles.size(); i+=3) {
    // 	auto tx0 = delaunay.coords[2 * delaunay.triangles[i]];`
    // 	auto ty0 = delaunay.coords[2 * delaunay.triangles[i] + 1];
    // 	auto tx1 = delaunay.coords[2 * delaunay.triangles[i + 1]];
    // 	auto ty1 = delaunay.coords[2 * delaunay.triangles[i + 1] + 1];
    // 	auto tx2 = delaunay.coords[2 * delaunay.triangles[i + 2]];
    // 	auto ty2 = delaunay.coords[2 * delaunay.triangles[i + 2] + 1];
    // 	for (auto j = 0; j < 8; j++) {
    // 		auto k = 8 - j;
    // 		img[(int)(tx0 * j + tx1 * k) + (int)(ty0 * j + ty1 * k) * width * 8] = 1;
    // 		img[(int)(tx2 * j + tx1 * k) + (int)(ty2 * j + ty1 * k) * width * 8] = 1;
    // 		img[(int)(tx0 * j + tx2 * k) + (int)(ty0 * j + ty2 * k) * width * 8] = 1;
    // 	}
    // }
    for (size_t e = 0; e < delaunay.triangles.size(); e++) {
        if (e > delaunay.halfedges[e] || delaunay.halfedges[e] == delaunator::INVALID_INDEX) {
        	plot_line(img, delaunay, width, delaunay.triangles[e], delaunay.triangles[next_halfedge(e)], 0x7f7f7f);
        }
    }

    {
		size_t i = delaunay.hull_start;
		do {
			// plot_line(img, delaunay, width, i, delaunay.hull_next[i], 4, 16);
			plot_edge(img, delaunay, width, delaunay.hull_tri[i], 0xffff00, 16);
			i = delaunay.hull_next[i];
		} while (i != delaunay.hull_start);

		i = delaunay.hull_start;
		do {
			// auto j = delaunay.hull_tri[i];
			// j = delaunay.halfedges[next_halfedge(j)];
			auto j = next_halfedge(delaunay.hull_tri[i]);
			while (delaunay.halfedges[j] != delaunator::INVALID_INDEX) {
				plot_edge(img, delaunay, width, j, 0x00ff00, 16);
				// j = delaunay.halfedges[next_halfedge(j)];
				j = next_halfedge(delaunay.halfedges[j]);
			}
			i = delaunay.hull_next[i];
		} while (i != delaunay.hull_start);
    }

 //    for(size_t i = 0; i < delaunay.triangles.size(); i+=100) {
	//     // auto start = delaunay.halfedges[i];
	//     auto start = i;
	//     if (start == delaunator::INVALID_INDEX) continue;
	// 	auto incoming = start;
	//     do {
	//     	auto prev = incoming;
	//         auto outgoing = next_halfedge(incoming);
	//         incoming = delaunay.halfedges[outgoing];
	//         if (incoming != delaunator::INVALID_INDEX) {
	// 	    	plot_edge(img, delaunay, width, delaunay.triangles[incoming], delaunay.triangles[i], 3, 16);
	// 	    	plot_edge(img, delaunay, width, delaunay.triangles[incoming], delaunay.triangles[prev], 2, 16);
	// 	    }
	//     } while (incoming != delaunator::INVALID_INDEX && incoming != start);
	// }

   	MakePNGImage(filename, plot_points_callback, (void *)(&img[0]), width * DEBUG_IMG_SCALE, height * DEBUG_IMG_SCALE, 32, nullptr);
}

void plot_river_flow(delaunator::Delaunator &dln, std::vector<size_t> &prev, std::vector<double> &flow,
               		 uint width, uint height, const char *filename) {
	std::vector<uint32> img(width * height * DEBUG_IMG_SCALE * DEBUG_IMG_SCALE);
    for (size_t e = 0; e < dln.triangles.size(); e++) {
        if (e > dln.halfedges[e] || dln.halfedges[e] == delaunator::INVALID_INDEX) {
        	plot_line(img, dln, width, dln.triangles[e], dln.triangles[next_halfedge(e)], 0x1f1f1f);
        }
    }
    auto max_flow = flow[0];
    for (auto f : flow) max_flow = std::max(max_flow, f);
    auto fm = std::log(max_flow);
    auto fh = fm / 2;

    for (size_t e = 0; e < dln.coords.size() / 2; e++) {
		if (prev[e] == delaunator::INVALID_INDEX) continue;
		// auto f = 191 * flow[e] / max_flow  + 0x40;
		// auto f2 =  255 * flow[e] / max_flow;
		auto fe = std::log(flow[e] + 1);
		uint32 r =  fe < fh ? 0x40 + 191 * fe / fh : 255;
		uint32 g =  fe < fh ? r : (2 - fe / fh) * 255;
		plot_line(img, dln, width, e, prev[e], r << 16 | g << 8, 2);
	}
   	MakePNGImage(filename, plot_points_callback, (void *)(&img[0]), width * DEBUG_IMG_SCALE, height * DEBUG_IMG_SCALE, 32, nullptr);
}

void plot_river_height(delaunator::Delaunator &dln, std::vector<size_t> &prev, std::vector<double> &rheight,
                       uint width, uint height, const char *filename) {
	std::vector<uint32> img(width * height * DEBUG_IMG_SCALE * DEBUG_IMG_SCALE);
    for (size_t e = 0; e < dln.triangles.size(); e++) {
        if (e > dln.halfedges[e] || dln.halfedges[e] == delaunator::INVALID_INDEX) {
        	plot_line(img, dln, width, dln.triangles[e], dln.triangles[next_halfedge(e)], 0x1f1f1f);
        }
    }
    for (size_t e = 0; e < dln.coords.size() / 2; e++) {
		if (prev[e] == delaunator::INVALID_INDEX) continue;
		auto f = (uint)(191 * rheight[e] + 0x40);
		plot_line(img, dln, width, e, prev[e], f << 16 | f << 8 | f, 2);
	}
   	MakePNGImage(filename, plot_points_callback, (void *)(&img[0]), width * DEBUG_IMG_SCALE, height * DEBUG_IMG_SCALE, 32, nullptr);
}

template <typename T> int sign(T val) {
    return (T(0) < val) - (val < T(0));
}

// https://observablehq.com/@techsparx/an-improvement-on-bridsons-algorithm-for-poisson-disc-samp/2
std::vector<double> poissosn_disc_sampling(uint width, uint height) {
	static int k = 4; // maximum number of samples before rejection
	static const Point::coord_t radius = 1.0;
	static const Point::coord_t radius2 = radius * radius;
	auto cell_size = radius * SQRT1_2;
	auto grid_width = (int)ceil(width / cell_size);
	auto grid_height = (int)ceil(height / cell_size);

	std::vector<Point> grid(grid_width * grid_height);
	std::vector<Point> queue;

	auto sample = [&](Point p) {
		grid[grid_width * (uint)(p.y / cell_size) + (uint)(p.x / cell_size)] = p;
		queue.push_back(p);
	};

	auto far = [&](Point::coord_t x, Point::coord_t y) {
		auto i = (int)(x / cell_size);
		auto j = (int)(y / cell_size);
		auto i0 = std::max(i - 2, 0);
		auto j0 = std::max(j - 2, 0);
		auto i1 = std::min(i + 3, grid_width);
		auto j1 = std::min(j + 3, grid_height);
		for (auto j = j0; j < j1; j++) {
			auto o = j * grid_width;
			for (auto i = i0; i < i1; i++) {
				auto s = grid[o + i];
				if (s.x || s.y) {
					auto dx = s.x - x;
					auto dy = s.y - y;
					if (dx * dx + dy * dy < radius2) {
						return false;
					}
					// if (dx * dx + dy * dy < radius2) return false;
				}
			}
		}
		return true;
	};

	sample(Point(width / 2, height / 2));

	while (!queue.empty()) {
		auto i = Random() % queue.size();
		auto parent = queue[i];
		Point::coord_t seed = static_cast <Point::coord_t> (Random()) / static_cast <Point::coord_t> (UINT32_MAX);
		const Point::coord_t eps = 0.0001;

		bool found = false;
		for (auto j = 0; j < k; j++) {
			auto a = 2 * PI * (seed + 1.0 * j / k);
			auto r = radius + eps;
			auto x = parent.x + r * std::cos(a);
			auto y = parent.y + r * std::sin(a);

			// Accept candidates that are inside the allowed extent
			// and farther than 2 * radius to all existing samples.
			if (0 <= x && x < width && 0 <= y && y < height && far(x, y)) {
				sample(Point(x, y));
				found = true;
				break;
			}
		}

		if (!found) {
			// If none of k candidates were accepted, remove it from the queue.
			auto r = queue.back();
			queue.pop_back();
			if (i < queue.size()) queue[i] = r;
		}
	}

	plot_points(grid, width, height, "poisson_disc.png");

	std::vector<double> dpoints;
	for (auto &p: grid) {
		if (p.x || p.y) {
			dpoints.push_back(p.x);
			dpoints.push_back(p.y);
		}
	}
	return dpoints;
}

void normalize(fftw_complex *begin, fftw_complex *end) {
	fftw_complex *p;
    double max_r = (*begin)[0], min_r = (*begin)[0];
    double max_i = (*begin)[1], min_i = (*begin)[1];
    for (p = begin + 1; p < end; p++) {
    	max_r = std::max(max_r, *p[0]);
    	min_r = std::min(min_r, *p[0]);
    	max_i = std::max(max_i, *p[1]);
    	min_i = std::min(min_i, *p[1]);
    }
    if (max_r == min_r || max_i == min_i) return;
    for (p = begin; p < end; p++) {
    	(*p)[0] = ((*p)[0] - min_r) / (max_r - min_r);
    	(*p)[1] = ((*p)[1] - min_i) / (max_i - min_i);
    }
}

void normalize(std::vector<double> &height) {
	double max_height = height[0];
	double min_height = height[0];
	for (auto x : height) {
		max_height = std::max(max_height, x);
		min_height = std::min(min_height, x);
	}
	if (max_height == min_height) return;
	for (auto &x : height) x = (x - min_height) / (max_height - min_height);
}


double fftfreq(size_t i, size_t n) {
	return (i < n / 2 ? (double)i : (double)i - n);
}

fftw_complex *genenerate_fft_noise(uint width, uint height, double power, double lower, double upper) {
    fftw_plan plan;
    uint size = width * height;
    fftw_complex *in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size);
    fftw_complex *out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size);
    fftw_complex *in_end = in + size;
    fftw_complex *out_end = out + size;
    fftw_complex *p;

    for (p = in; p < in_end; p++) {
    	double a = 2. * PI * Random() / UINT32_MAX;
    	(*p)[0] = std::cos(a);
    	(*p)[1] = std::sin(a);
    }

    plan = fftw_plan_dft_2d(width, height, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    p = out;
    for (size_t y = 0; y < height; y++) {
    	auto fy = fftfreq(y, height);
	    for (size_t x = 0; x < width; x++, p++) {
	    	auto f = std::hypot(fy, fftfreq(x, width));
	    	auto e = (f > lower ? std::pow(f, power) : 0.);
	   		(*p)[0] *= e;
	   		(*p)[1] *= e;
	    }
	}

    plan = fftw_plan_dft_2d(height, width, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);

    fftw_free(out);

	normalize(in, in_end);

    return in;
}

double get_edge_height(fftw_complex *noise, uint width, delaunator::Delaunator &dln, size_t e) {
	auto x = 2 * dln.triangles[e];
	return noise[(uint)dln.coords[x] + (uint)dln.coords[x + 1] * width][0];
}

double lerp(double x, double y, double a) {
	return (1.0 - a) * x + a * y;
}

std::pair<double, double> get_edge_direction(delaunator::Delaunator &dln, size_t i, size_t j) {
	auto dx = dln.coords[j * 2] - dln.coords[i * 2];
	auto dy = dln.coords[j * 2 + 1] - dln.coords[i * 2 + 1];
	auto d = hypot(dx, dy);
	return std::make_pair(dx / d, dy / d);
}


/**
 * This routine provides the essential cleanup necessary before OTTD can
 * display the terrain. When generated, the terrain heights can jump more than
 * one level between tiles. This routine smooths out those differences so that
 * the most it can change is one level. When OTTD can support cliffs, this
 * routine may not be necessary.
 */
static void HeightMapSmoothSlopes(height_t dh_max)
{
	for (int y = 0; y <= (int)_height_map.size_y; y++) {
		for (int x = 0; x <= (int)_height_map.size_x; x++) {
			height_t h_max = std::min(_height_map.height(x > 0 ? x - 1 : x, y), _height_map.height(x, y > 0 ? y - 1 : y)) + dh_max;
			if (_height_map.height(x, y) > h_max) _height_map.height(x, y) = h_max;
		}
	}
	for (int y = _height_map.size_y; y >= 0; y--) {
		for (int x = _height_map.size_x; x >= 0; x--) {
			height_t h_max = std::min(_height_map.height(x < _height_map.size_x ? x + 1 : x, y), _height_map.height(x, y < _height_map.size_y ? y + 1 : y)) + dh_max;
			if (_height_map.height(x, y) > h_max) _height_map.height(x, y) = h_max;
		}
	}
}

std::vector<std::pair<TileIndex, uint8>> _rivers;

void Generate() {
    double sea_level = 0.15;
    const double directional_inertia = 0.3;
    const double default_water_level = 0.5;
    const double evaporation_rate = 1.0 - 0.04;
    const double river_downcutting_constant = 0.5;
    const double allowed_uphill_delta = 0.0005;

    int w = MapSizeX() + 1;
    int h = MapSizeY() + 1;
    int wh = w * h;
    int whmax = std::max(w, h);

    fftw_complex *noise = genenerate_fft_noise(w, h, -2., 2., 1e100);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            auto d = 2.0 * hypot(x - w / 2, y - h / 2) / whmax;
            auto dm = (d < sea_level) ? 0 : (d - sea_level) / (0.9 - sea_level) + 0.1;
            dm = std::min(dm * dm + 0.05, 1.0);
            // dm = std::min(dm, 1.0);
            auto da = (d < sea_level) ? 0 : (d - sea_level) / 2 + 0.05;
            // auto d = .1;
            noise[y * w + x][0] = noise[y * w + x][0] * dm + da;
            // noise[y * MapSizeX() + x][0] = std::min(1.0, noise[y * MapSizeX() + x][0] + d);
        }

    for (int i = 0; i < wh; i++) noise[i][0] = (noise[i][0] < sea_level ? 0 : noise[i][0] - sea_level + 0.05);

    normalize(noise, noise + wh);
    MakePNGImage("noise.png", fft_plot_callback, (void *)noise, w, h, 32, nullptr);
    sea_level = 0.0001;

    auto points = poissosn_disc_sampling(w, h);
    delaunator::Delaunator dln(points);
    if (wh <= 1 << 16) plot_delaunay(dln, w, h, "delaunay.png");

    size_t n = dln.coords.size() / 2;
    std::vector<double> hegiht(n);

    typedef std::pair<size_t, double> q_item_t;
    auto cmp = [](const q_item_t &a, const q_item_t &b) { return a.second > b.second; };
    std::priority_queue<q_item_t, std::vector<q_item_t>, decltype(cmp) > q(cmp);
    std::vector<bool> vis(dln.coords.size() / 2);
    std::vector<size_t> prev(dln.coords.size() / 2);
    std::vector<double> height(dln.coords.size() / 2);

    for (size_t i = 0; i  < dln.coords.size() / 2; i++) prev[i] = delaunator::INVALID_INDEX;
    for (size_t e = 0; e < dln.triangles.size(); e++) {
        if (get_edge_height(noise, w, dln, e) > sea_level) continue;
        height[dln.triangles[e]] = 0;
        auto next_e = next_halfedge(e);
        auto e_end = dln.triangles[next_e];
        if (vis[e_end]) continue;
        auto h = get_edge_height(noise, w, dln, next_e);
        vis[e_end] = true;
        if (h >= sea_level) q.push(std::make_pair(e, h));
    }

    while (!q.empty()) {
        auto x = q.top(); q.pop();
        auto incoming = x.first;
        auto ii = dln.triangles[next_halfedge(incoming)];
        height[ii] = x.second;
        do {
            auto outgoing = next_halfedge(incoming);
            incoming = dln.halfedges[outgoing];
            if (incoming == delaunator::INVALID_INDEX) break;

            auto i = dln.triangles[incoming];
            if (vis[i]) continue;

            vis[i] = true;
            q.push(std::make_pair(outgoing, std::max(get_edge_height(noise, w, dln, incoming), x.second)));
            // q.push(std::make_pair(outgoing, get_edge_height(noise, dln, incoming) + x.second - sea_level));
            prev[i] = ii;

        } while (incoming != x.first);
    }

    normalize(height);

    if (w * h <= 1 << 16) plot_river_height(dln, prev, height , w, h, "initial_river_height.png");

    for (size_t i = 0; i  < dln.coords.size() / 2; i++) prev[i] = delaunator::INVALID_INDEX;

    struct q2_item_t {
        double priority;
        size_t edge;
        std::pair<double, double> direction;
        q2_item_t(double    priority, size_t edge, std::pair<double, double> direction)
            :priority{priority}, edge{edge}, direction{direction} {}
    };

    auto cmp2 = [](const q2_item_t &a, const q2_item_t &b) { return a.priority > b.priority; };
    std::priority_queue<q2_item_t, std::vector<q2_item_t>, decltype(cmp2) > q2(cmp2);
    std::vector<size_t> order;

    for (size_t e = 0; e < dln.triangles.size(); e++) {
        if (get_edge_height(noise, w, dln, e) > sea_level) continue;
        auto next_e = next_halfedge(e);
        auto e_end = dln.triangles[next_e];
        auto h = get_edge_height(noise, w, dln, next_e);
        if (h >= sea_level) q2.push(q2_item_t(-1.0, e, get_edge_direction(dln, dln.triangles[e], e_end)));
    }

    while (!q2.empty()) {
        auto x = q2.top(); q2.pop();
        auto incoming = x.edge;
        auto j = dln.triangles[next_halfedge(incoming)];

        if (prev[j] != delaunator::INVALID_INDEX) continue;
        prev[j] = dln.triangles[incoming];
        // fprintf(stderr, "IJIJIJIJIJI %lu %lu\n", dln.triangles[incoming], j);

        order.push_back(j);
        do {
            auto outgoing = next_halfedge(incoming);
            incoming = dln.halfedges[outgoing];
            if (incoming == delaunator::INVALID_INDEX) break;

            auto k = dln.triangles[incoming];

            if (prev[k] != delaunator::INVALID_INDEX) continue;
            if (height[k] + allowed_uphill_delta <= height[j]) continue;

            // fprintf(stderr, "A %lu %lu %lu\n", dln.triangles[incoming], j, k);
            // fprintf(stderr, "B %lu %lu %lu\n", x.edge, incoming, outgoing);

            auto direction = get_edge_direction(dln, j, k);

            q2.push(q2_item_t(
                -(direction.first * x.direction.first + direction.second * x.direction.second),
                outgoing,
                std::make_pair(lerp(direction.first, x.direction.first, directional_inertia),
                               lerp(direction.second, x.direction.second, directional_inertia))
            ));
        } while (incoming != x.edge);
    }

    // Compute river flow
    std::vector<double> flow(dln.coords.size() / 2);
    std::reverse(order.begin(), order.end());
    for (size_t i = 0; i  < dln.coords.size() / 2; i++) {
        // auto d = std::min(1.0, 2.0 * hypot(dln.coords[i * 2] - (int)MapSizeX() / 2, dln.coords[i * 2 + 1] - (int)MapSizeY() / 2) / std::max(MapSizeX(), MapSizeY()));
        auto d = noise[(int)dln.coords[i * 2 + 1] * w + (int)dln.coords[i * 2]][0];
        flow[i] = default_water_level * d * d;
    }
    for (auto i : order) {
        if (prev[i] == delaunator::INVALID_INDEX) continue;
        auto j = prev[i];
        // double d = std::hypot(dln.coords[j * 2] - dln.coords[1 * 2], dln.coords[j * 2 + 1] - dln.coords[1 * 2 + 1]);
        flow[j] += flow[i] * evaporation_rate;
    }

    if (wh <= 1 << 16) plot_river_flow(dln, prev, flow, w, h, "river_flow_computed.png");

    for (size_t i = 0; i  < dln.coords.size() / 2; i++) vis[i] = false;
    for (size_t e = 0; e < dln.triangles.size(); e++) {
        if (get_edge_height(noise, w, dln, e) > sea_level) continue;
        auto next_e = next_halfedge(e);
        auto e_end = dln.triangles[next_e];
        if (vis[e_end]) continue;
        auto h = get_edge_height(noise, w, dln, next_e);
        vis[e_end] = true;
        if (h >= sea_level) q.push(std::make_pair(e, 0));
    }

    while (!q.empty()) {
        auto x = q.top(); q.pop();
        auto incoming = x.first;
        auto ii = dln.triangles[next_halfedge(incoming)];
        height[ii] = x.second;
        do {
            auto outgoing = next_halfedge(incoming);
            incoming = dln.halfedges[outgoing];
            if (incoming == delaunator::INVALID_INDEX) break;

            auto i = dln.triangles[incoming];
            if (vis[i]) continue;

            vis[i] = true;
            auto v = (prev[i] == ii ? flow[i] : 0.0);
            auto downcut = 1.0 / (1.0 + v * river_downcutting_constant);

            q.push(std::make_pair(outgoing, x.second + (get_edge_height(noise, w, dln, incoming) - sea_level) * downcut));
        } while (incoming != x.first);
    }

    normalize(height);

    if (wh <= 1 << 16) plot_river_height(dln, prev, height, w, h, "final_river_height.png");

    auto x = dln.coords;
    auto y = &dln.coords[1];
    for (size_t e = 0; e < dln.triangles.size(); e += 3) {
        auto i = 2 * dln.triangles[e];
        auto j = 2 * dln.triangles[e + 1];
        auto k = 2 * dln.triangles[e + 2];
        uint min_x = std::max<int>((int)std::floor(std::min(std::min(x[i], x[j]), x[k])), 0);
        uint max_x = std::min<int>((int)std::ceil(std::max(std::max(x[i], x[j]), x[k])), w);
        uint min_y = std::max<int>((int)std::floor(std::min(std::min(y[i], y[j]), y[k])), 0);
        uint max_y = std::min<int>((int)std::ceil(std::max(std::max(y[i], y[j]), y[k])), h);
        // fprintf(stderr, "%u %u %u %u\n", min_x, max_x, min_y, max_y);
        for (auto xx = min_x; xx < max_x; xx++)
            for (auto yy = min_y; yy < max_y; yy++) {
                double *h = noise[yy * w + xx];
                if (*h <= sea_level) continue;
                double d = (y[j] - y[k]) * (x[i] - x[k]) + (x[k] - x[j]) * (y[i] - y[k]);
                double w1 = ((y[j] - y[k]) * (xx + 0.5 - x[k]) + (x[k] - x[j]) * (yy + 0.5 - y[k])) / d;
                double w2 = ((y[k] - y[i]) * (xx + 0.5 - x[k]) + (x[i] - x[k]) * (yy + 0.5 - y[k])) / d;
                double w3 = 1.0 - w1 - w2;
                if (w1 < 0 || w2 < 0 || w3 < 0) continue;
                *h = height[dln.triangles[e]] * w1 + height[dln.triangles[e + 1]] * w2 + height[dln.triangles[e + 2]] * w3 + sea_level;
            }
    }
    normalize(noise, noise + wh);
    for (int i = 0; i < wh; i++) noise[i][0] *= noise[i][0];
    MakePNGImage("terrain.png", fft_plot_callback, (void *)noise, w, h, 32, nullptr);

	if (!AllocHeightMap()) return;
	GenerateWorldSetAbortCallback(FreeHeightMap);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)
    		_height_map.height(x, y) = (uint8)(noise[y * w + x][0] * 255);
    }

	HeightMapSmoothSlopes(I2H(1));

	/* First make sure the tiles at the north border are void tiles if needed. */
	if (_settings_game.construction.freeform_edges) {
		for (uint x = 0; x < MapSizeX(); x++) MakeVoid(TileXY(x, 0));
		for (uint y = 0; y < MapSizeY(); y++) MakeVoid(TileXY(0, y));
	}

	std::vector<bool> visible(dln.coords.size() / 2);
    for (auto e : order) {
		if (prev[e] == delaunator::INVALID_INDEX) continue;
		if (flow[e] > default_water_level * 30 || visible[e]) {
			visible[e] = true;
			visible[prev[e]] = true;
		}
	}

    std::reverse(order.begin(), order.end());
	std::map<TileIndex, uint8> rivers;
    // for (size_t e = 0; e < dln.coords.size() / 2; e++) {
    for (auto e : order) {
		if (prev[e] == delaunator::INVALID_INDEX) continue;
		// if (flow[e] < default_water_level * 30) continue;
		if (!visible[e]) continue;

		// grid traversal algo:
		// https://gamedev.stackexchange.com/questions/81267/how-do-i-generalise-bresenhams-line-algorithm-to-floating-point-endpoints/182143#182143

		auto a = e * 2, b = prev[e] * 2;
		auto ax = dln.coords[a], ay = dln.coords[a + 1];
		auto bx = dln.coords[b], by = dln.coords[b + 1];
		uint x = (uint)floor(ax), y = (uint)floor(ay);
		auto dx = bx - ax, dy = by - ay;
		int sx = sign(dx), sy = sign(dy);
		if (x >= MapSizeX()) continue;
		if (y >= MapSizeY()) continue;

		auto xofs = ((bx > ax) ? ceil(ax) - ax : ax - floor(ax));
		auto yofs = ((by > ay) ? ceil(ay) - ay : ay - floor(ay));
		auto aa = atan2(-dy, dx);

		// TODO possible div by 0
		auto tmaxx = xofs / cos(aa);
		auto tmaxy = yofs / sin(aa);
		auto tdx = 1.0 / cos(aa);
		auto tdy = 1.0 / sin(aa);

		auto md = (int)(abs(floor(bx) - floor(ax)) + abs(floor(by) - floor(ay)));

		// fprintf(stderr, "RIVER %f,%f - %f,%f %d  d=%f,%f s=%d,%d\n", ax, ay, bx, by, md, (double)dx, (double)dy, sx, sy);
		for (auto t = 0; t < md; t++) {
			if (abs(tmaxx) < abs(tmaxy)) {
				tmaxx += tdx;
				uint nx = x + sx;
				if (nx >= MapSizeX()) break;
				auto side = (sx > 0 ? 1 : 3);
				rivers[TileXY(x, y)] |= (2 << ((side ^ 2) * 2));
				rivers[TileXY(nx, y)] |= (1 << (side * 2));
				auto mx = std::max(x, nx);
				auto minh = std::min(_height_map.height(mx, y), _height_map.height(mx, y + 1));
				_height_map.height(mx, y) = _height_map.height(mx, y + 1) = minh;
				x = nx;
			} else {
				tmaxy += tdy;
				uint ny = y + sy;
				if (ny >= MapSizeY()) break;
				auto side = (sy > 0 ? 0 : 2);
				rivers[TileXY(x, y)] |= (2 << ((side ^ 2) * 2));
				rivers[TileXY(x, ny)] |= (1 << (side * 2));
				auto my = std::max(y, ny);
				auto minh = std::min(_height_map.height(x, my), _height_map.height(x + 1, my));
				_height_map.height(x, my) = _height_map.height(x + 1, my) = minh;
				y = ny;
			}
		}
	}

	_rivers.clear();
	for (auto kv : rivers) {
		_rivers.emplace_back(kv.first,
			std::min(2, kv.second % 4) +
			std::min(2, (kv.second >> 2) % 4) * 3 +
			std::min(2, (kv.second >> 4) % 4) * 9 +
			std::min(2, kv.second >> 6) * 27
		);
	}

	/* Transfer height map into OTTD map */
	for (int y = 0; y < _height_map.size_y; y++) {
		for (int x = 0; x < _height_map.size_x; x++) {
			TgenSetTileHeight(TileXY(x, y), Clamp(H2I(_height_map.height(x, y)), 0, 255));
		}
	}

	FreeHeightMap();
    fftw_free(noise);
}

void GenerateRivers() {
	for (auto r : _rivers) {
		// fprintf(stderr, "BUILDOBJ %d(%d %d) %d\n", (int)r.first, (int)TileX(r.first), (int)TileY(r.first), (int)r.second);
		CmdBuildObject(r.first, DC_EXEC | DC_AUTO | DC_NO_TEST_TOWN_RATING | DC_NO_MODIFY_TOWN_RATING, 6 + r.second, 0, nullptr);
	}
}

} // namespace terragen

} // namespace citymania
