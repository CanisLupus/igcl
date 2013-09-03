#pragma once

#include "common.hpp"
#include "constants.hpp"
#include "structs.hpp"
#include "operators.hpp"
#include "objects.hpp"

using namespace std;

class Raytracer {
public:
	vector<VisibleObject *> objects;	// objects in the scene
	vector<LightingObject *> lights;	// lights in the scene
	
	color_s ia;				// ambient light component

	color_s bg_color;		// background color
	
	vector_s screen_pos;	// position of the center of the screen in space
	vector_s obs_pos;	// position of the observer in space
	
	vector_s obs_z;			// front; represents which way the observer is looking
	vector_s obs_y;			// up; represents which way is up (effectively rotates the observer's head)
	vector_s obs_x;			// side; calculated using the cross product: obs_z ^ obs_y
	
	Raytracer() {
		ia.set(AMBIENT_R, AMBIENT_G, AMBIENT_B);
		bg_color.set(BG_R, BG_G, BG_B);
		
		obs_z.set(0, 0, 1);
		normalize(obs_z);
		obs_y.set(0, 1, 0);
		normalize(obs_y);
		obs_x = obs_z ^ obs_y;		// cross product of "front" and "up" gives a vector pointing right
		normalize(obs_x);
		
		screen_pos.set(0, 0, 0);
		obs_pos = screen_pos -DIST_FROM_SCREEN * obs_z;
	}
	
	void setupScene() {
		Sphere * s;
		Plane * p;
		PointLight * l;
		EmissiveSphere * e;

		// --------------------------- Planes
		
		p = new Plane();
		p->normal = {-1.0, 1.0, 0.0};
		p->position = {100, 0, 0};
		p->material = materials.at("WHITE");
		p->features = {0.0, 0.0, 0.0};
		objects.push_back(p);

		p = new Plane();
		p->normal = {1, -1, 0};
		p->position = {-100, 0, 0};
		p->material = materials.at("WHITE");
		p->features = {0.0, 0.0, 0.0};
		objects.push_back(p);

		// --------------------------- Spheres
		
		s = new Sphere();
		s->radius = 20;
		s->position = {0, 0, 100};
		s->material = materials.at("GREEN");
		s->features = {0.8, 0.0, 0.0};
		objects.push_back(s);

		s = new Sphere();
		s->radius = 6;
		s->position = {10, -10, 60};
		s->material = materials.at("RED");
		s->features = {0.8, 0.0, 0.0};
		objects.push_back(s);
		
		s = new Sphere();
		s->radius = 4;
		s->position = {10, 10, 60};
		s->material = materials.at("YELLOW");
		s->features = {0.8, 0.0, 0.0};
		objects.push_back(s);

		// --------------------------- Lights
		
		l = new PointLight();
		l->position = {24, 0, 14};
		l->color = {0.4, 0.4, 0.4};
		l->is = {0.6, 0.6, 0.6};
		lights.push_back(l);

		l = new PointLight();
		l->position = {-24, 0, 60};
		l->color = {0.4, 0.4, 0.4};
		l->is = {0.6, 0.6, 0.6};
		lights.push_back(l);
		
		e = new EmissiveSphere();
		e->radius = 0.5;
		e->position = {-20, 0, 40};
		e->material = materials.at("RED");
		e->features = {0.0, 0.0, 0.0};
		objects.push_back(e);
		e->color = {0.8, 0.0, 0.0};
		e->is = {0.0, 0.0, 0.0};
		lights.push_back(e);
	}

	void stepScene() {
		//Light * l = lights[0];
		//l->pos.x += 15;
		//l->pos.y += 15;
		//l->pos.z += 5;
	}
	
	color_s castRay(const ray_s & r, const int recursion_level = 0) {
		if (recursion_level > MAX_RECURSION)
			return bg_color;
		
		VisibleObject * hit_obj = NULL;	// reference of possible hit object. None (NULL) by default (no collision).
		vector_s hit_point;				// coordinates where collision happens
		double squared_dist;			// (squared) distance to intersected object
		double min_squared_dist = std::numeric_limits<double>::infinity();	// (squared) distance to current closest intersected object
		vector_s temp;
		
		for (VisibleObject * &obj : objects) {	// find closest object that the ray intersects (if any)
			if (obj->collisionTest(r, hit_point)) {
				temp = hit_point - r.origin;
				squared_dist = temp * temp;
				if (squared_dist < min_squared_dist) {	// update with closest object
					min_squared_dist = squared_dist;
					hit_obj = obj;
				}
			}
		}

		if (hit_obj == NULL)		// if no object was intersected, return background color
			return bg_color;

		vector_s N;						// N: normal of the surface at this point
		vector_s V = -r.direction;		// V: vector from point to the screen
		vector_s L;						// L: vector from point to the light
		vector_s R;						// R: perfectly reflected light ray at point

		hit_obj->getNormal(hit_point, N);		// N now has the normal vector of the surface at hit point

		color_s color(hit_obj->material.ke.r * ia.r, hit_obj->material.ke.g * ia.g, hit_obj->material.ke.b * ia.b);	// start with ambient light only
		color_s reflect_color(0.0, 0.0, 0.0);
		color_s refract_color(0.0, 0.0, 0.0);

		LightingObject * l = dynamic_cast<LightingObject *>(hit_obj);
		if (l != NULL) {
			color = l->color;
		}

		if (hit_obj->features.reflection > 0) {
			vector_s reflect_v = r.direction - 2 * (N * r.direction) * N;
			ray_s reflect_r(hit_point, reflect_v);
			reflect_color = castRay(reflect_r, recursion_level+1);
		}
		
		if (hit_obj->features.refraction > 0) {
			/* */
		}
		
		VisibleObject * shadow_ray_hit_obj = NULL;
		vector_s shadow_ray_hit_point;
		ray_s shadow_ray;
		double point_to_light_dist;
		
		for (LightingObject * &light : lights) {		// for each light in the scene
			L = light->position - hit_point;
			point_to_light_dist = ~L;
			normalize(L);
			
			shadow_ray.set(hit_point, L);	// ray directed at light (to find shadows in the way)
			
			for (VisibleObject * &obj : objects) {
				if (hit_obj != obj && (WorldObject *) light != (WorldObject *) obj && obj->collisionTest(shadow_ray, shadow_ray_hit_point)) {	// any collision means there is shadow (light is blocked)
					vector_s vector_between = shadow_ray_hit_point - hit_point;
					if (~vector_between < point_to_light_dist) {
						shadow_ray_hit_obj = obj;
						break;
					}
				}
			}
			
			if (shadow_ray_hit_obj != NULL) {
				shadow_ray_hit_obj = NULL;
				continue;
			}
			
			double LN = L * N;
			
			if (LN > 0) {	// if angle between vectors < 90�, then light reaches point
				double attenuation = 1.0 / (point_to_light_dist*point_to_light_dist*light->attenuation.quadratic
											+ point_to_light_dist*light->attenuation.linear
											+ light->attenuation.constant);
				
				color.r += hit_obj->material.kd.r * LN * light->color.r * attenuation;
				color.g += hit_obj->material.kd.g * LN * light->color.g * attenuation;
				color.b += hit_obj->material.kd.b * LN * light->color.b * attenuation;

				R = 2 * LN * N - L;
				normalize(R);

				double RV = R * V;

				if (RV > 0) {
					double RV_pow = pow(RV, hit_obj->material.shininess);
					color.r += hit_obj->material.ks.r * RV_pow * light->is.r * attenuation;
					color.g += hit_obj->material.ks.g * RV_pow * light->is.g * attenuation;
					color.b += hit_obj->material.ks.b * RV_pow * light->is.b * attenuation;
				}
			}
		}
		
		reflect_color *= hit_obj->features.reflection;
		refract_color *= hit_obj->features.refraction;
		color *= (1 - (hit_obj->features.reflection + hit_obj->features.refraction));
		
		color_s final_color = color + reflect_color + refract_color;
		return final_color;
	}


	color_s castRayFromScreen(const int image_y, const int image_x) {
		vector_s to_pixel = (image_x*WIDTH_RATIO-HALF_WIDTH)*obs_x + (HALF_HEIGHT-image_y*HEIGHT_RATIO)*obs_y;	// vector pointing from center of screen to current pixel
		vector_s pixel = screen_pos - to_pixel;
		vector_s v = pixel - obs_pos;
		normalize(v);
		ray_s r(pixel, v);
		return castRay(r);
	}
};
