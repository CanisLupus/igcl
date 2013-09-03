#ifndef TSPINSTANCE_HPP_
#define TSPINSTANCE_HPP_

#include <vector>
#include <string>

// ----------------------------------------------------------------------------

struct point
{
	float x, y;
	point() {}
	point(float x, float y) : x(x), y(y) {}
};

// ----------------------------------------------------------------------------

class TSPInstance
{
public:
	std::vector<point> points;
	std::vector< std::vector<float> > dists;

	bool initializeFromFile(const std::string & filepath);
	void precalculateDistances();
	void calculatePointLimits(float & minX, float & maxX, float & minY, float & maxY);
};

// ----------------------------------------------------------------------------

#endif /* TSPINSTANCE_HPP_ */
