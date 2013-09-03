#pragma once

#include "constants.hpp"

extern const double DFLT_REFRACTIVE_INDEX;

// ---------------------------------
// ----------- STRUCTS -------------
// ---------------------------------

// 3D space vector/coordinates
struct vector_s
{
	double x, y, z;
	
	vector_s() {}
	vector_s(const double & x, const double & y, const double & z) : x(x), y(y), z(z) {}
	inline void set(const double & _x, const double & _y, const double & _z) { x = _x; y = _y; z = _z; }
};

// ray with an origin (c) and a direction (v) in 3D space
struct ray_s
{
	vector_s origin;
	vector_s direction;
	
	ray_s() {}
	ray_s(const vector_s & c, const vector_s & v) : origin(c), direction(v) {}
	inline void set(const vector_s & _c, const vector_s & _v) { origin = _c; direction = _v; }
};

// RGB color
struct color_s
{
	double r, g, b;
	
	color_s() {}
	color_s(const double & r, const double & g, const double & b) : r(r), g(g), b(b) {}
	inline void set(const double & _r, const double & _g, const double & _b) { r = _r; g = _g; b = _b; }
};

// light reflection characteristics of a material
struct material_s
{
	color_s ke, kd, ks;		// ambient, diffuse and specular reflection values of the material
	double shininess;		// when large, specular highlight is small
	
	material_s() {}
	material_s(const color_s & ka, const color_s & kd, const color_s & ks, const double & shininess)
		: ke(ka), kd(kd), ks(ks), shininess(shininess) {}
};

// reflection and refraction features of a material
struct features_s
{
	double reflection;			// percentage of reflection when a ray hits the object
	double refraction;			// percentage of refraction when a ray hits the object
	double refractive_index;	// refractive index of the object (affects refraction angle)
	
	features_s() {}
	features_s(const double & reflection, const double & refraction, const double & refractive_index=DFLT_REFRACTIVE_INDEX)
		: reflection(reflection), refraction(refraction), refractive_index(refractive_index) {}
	inline void set(const double & _reflection, const double & _refraction, const double & _refractive_index=DFLT_REFRACTIVE_INDEX)
		{ reflection = _reflection; refraction = _refraction; refractive_index = _refractive_index; }
};

// light attenuation values
struct attenuation_s
{
	double constant, linear, quadratic;		// constant, linear and quadratic light attenuation values
	
	attenuation_s() {}
	attenuation_s(const double & cons, const double & lin, const double & quad) : constant(cons), linear(lin), quadratic(quad) {}
	inline void set(const double & _cons, const double & _lin, const double & _quad)
		{ constant = _cons; linear = _lin; quadratic = _quad; }
};
