#pragma once

// ------------------------------------------
// ----------- VECTOR OPERATORS -------------
// ------------------------------------------

inline vector_s operator - (const vector_s & c1, const vector_s & c2) {		// vector - vector (coordinates - coordinates)
	return vector_s(c1.x-c2.x, c1.y-c2.y, c1.z-c2.z);
}

inline vector_s operator + (const vector_s & c, const vector_s & v) {		// vector + vector (coordinates + vector)
	return vector_s(c.x+v.x, c.y+v.y, c.z+v.z);
}

inline vector_s operator - (const vector_s & v) {							// -vector
	return vector_s(-v.x, -v.y, -v.z);
}

inline vector_s operator * (const vector_s & v, const double & n) {			// vector * number
	return vector_s(v.x*n, v.y*n, v.z*n);
}

inline vector_s operator / (const vector_s & v, const double & n) {			// vector / number
	return vector_s(v.x/n, v.y/n, v.z/n);
}

inline vector_s operator * (const double & n, const vector_s & v) {			// number * vector
	return vector_s(v.x*n, v.y*n, v.z*n);
}

inline vector_s & operator *= (vector_s & v, const double & n) {			// vector *= number
	v.set(v.x*n, v.y*n, v.z*n);
	return v;
}

inline vector_s & operator /= (vector_s & v, const double & n) {			// vector /= number
	v.set(v.x/n, v.y/n, v.z/n);
	return v;
}

inline double operator * (const vector_s & v1, const vector_s & v2) {		// dot product of vectors (length of v1's projection over v2)
	return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

inline vector_s operator ^ (const vector_s & v1, const vector_s & v2) {		// cross product of vectors
	return vector_s( v1.y * v2.z - v1.z * v2.y,
					 v1.z * v2.x - v1.x * v2.z,
					 v1.x * v2.y - v1.y * v2.x );
}

inline double operator ~ (const vector_s & v) {								// length of vector
	return sqrt(v * v);
}

inline vector_s & normalize (vector_s & v) {								// normalize vector
	v /= ~v;
	return v;
}

// -----------------------------------------
// ----------- COLOR OPERATORS -------------
// -----------------------------------------

inline color_s operator + (const color_s & c1, const color_s & c2) {	// color + color
	return color_s(c1.r+c2.r, c1.g+c2.g, c1.b+c2.b);
}

inline color_s operator * (const color_s & c, const double & n) {		// color * number
	return color_s(c.r*n, c.g*n, c.b*n);
}

inline color_s operator * (const double & n, const color_s & c) {		// number * color
	return color_s(c.r*n, c.g*n, c.b*n);
}

inline color_s & operator *= (color_s & c, const double & n) {			// color *= number
	c.set(c.r*n, c.g*n, c.b*n);
	return c;
}

inline color_s & operator += (color_s & c1, const color_s & c2) {		// color += color
	c1.set(c1.r+c2.r, c1.g+c2.g, c1.b+c2.b);
	return c1;
}

// ------------------------------------------
// ----------- STREAM OPERATORS -------------
// ------------------------------------------

inline std::ostream & operator << (std::ostream & stream, const vector_s & c) {			// to print coordinates (and vectors)
	return (stream << "(" << c.x << ", " << c.y << ", " << c.z << ")");
}

inline std::ostream & operator << (std::ostream & stream, const ray_s & r) {			// to print rays (start point and vector)
	return (stream << "coords: " << r.origin << " vect:" << r.direction);
}

inline std::ostream & operator << (std::ostream & stream, const color_s & color) {		// to print rays (start point and vector)
	return (stream << "#" << color.r << "#" << color.g << "#" << color.b);
}
