#pragma once

#include "common.hpp"
#include "constants.hpp"
#include "structs.hpp"
#include "operators.hpp"


class WorldObject
{
public:
	vector_s position;
};


class CollidableObject : public virtual WorldObject
{
public:
	virtual ~CollidableObject() {};

	virtual bool collisionTest(const ray_s & r, vector_s & c) = 0;
	virtual vector_s & getNormal(const vector_s & c, vector_s & v) = 0;	// get normal vector of object at a certain point of the surface
};


class VisibleObject : public CollidableObject
{
public:
	material_s material;
	features_s features;
};


class LightingObject : public virtual WorldObject
{
public:
	color_s color, is;					// diffuse and specular intensity
	attenuation_s attenuation;		// light attenuation values

	LightingObject() {
		setDfltAttenuation();
	}

	void setDfltAttenuation() {
		attenuation.set(DFLT_CONSTANT_ATTENUATION, DFLT_LINEAR_ATTENUATION, DFLT_QUADRATIC_ATTENUATION);
	}
};


class PointLight : public LightingObject
{
public:
	PointLight() {
		setDfltAttenuation();
	}
	
	bool collisionTest(const ray_s & r, vector_s & c) { return true; }
	vector_s & getNormal(const vector_s & c, vector_s & v) { return v; }
};


class Plane : public VisibleObject
{
public:
	vector_s normal;
	
	Plane() {}
	
	Plane(const vector_s & _normal) {
		setNormal(_normal);
	}

	void setNormal(const vector_s & _normal) {
		normal = _normal;
		normalize(normal);
	}

	// see http://www.cl.cam.ac.uk/teaching/1999/AGraphHCI/SMAG/node2.html (slightly modified)
	bool collisionTest(const ray_s & r, vector_s & c) {
		double t = normal * (position - r.origin) / (normal * r.direction);
		if (t < 0)
			return false;
		c = r.origin + t * r.direction;
		return true;
	}

	vector_s & getNormal(const vector_s & c, vector_s & v) {
		v = normal;
		return v;
	}
};


class Sphere : public VisibleObject
{
public:
	double radius;

	Sphere() {}
	
	Sphere(const double & radius) : radius(radius) {}

	// see http://www.cs.unc.edu/~rademach/xroads-RT/RTarticle.html
	bool collisionTest(const ray_s & r, vector_s & c) {
		vector_s to_center = position - r.origin;		// vector from ray origin to center of sphere
		double v = to_center * r.direction;		// projection of to_center over ray vector
		
		if (v < 0)	// (try to) avoid collisions with objects behind ray starting point
			return false;
		
		double disc = radius*radius - (to_center*to_center - v*v);
		
		if (disc < 0)
			return false;
		else {
			double d = sqrt(disc);
			c = r.origin + (v - d) * r.direction;	// determine collision point
			return true;
		}
	}
	
	vector_s & getNormal(const vector_s & c, vector_s & v) {
		v = c - position;	// normal can be the vector from the center to the surface collision point
		normalize(v);
		return v;
	}
};


class Triangle : public VisibleObject
{
public:
	vector_s p1, p2, p3;

	Triangle(const vector_s & p1, const vector_s & p2, const vector_s & p3) : p1(p1), p2(p2), p3(p3) {}

	bool collisionTest(const ray_s & r, vector_s & c) {
		return true;
	}

	vector_s & getNormal(const vector_s & c, vector_s & v) {
		return v;
	}
};


class EmissiveSphere : public LightingObject, public Sphere
{
public:
	EmissiveSphere() {}
};

