#include "TSPInstance.hpp"

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

// ----------------------------------------------------------------------------

bool TSPInstance::initializeFromFile(const std::string & filepath)
{
	std::ifstream file;
	file.open(filepath);
	if (file.fail())
		return false;
	std::string line, elem, search;
	std::istringstream iss;

	//std::cout << "START" << '\n';
	search = "DIMENSION";
	do {
		std::getline(file, line);
		//std::cout << line << '\n';
	} while (line.compare(0, search.length(), search) != 0);

	int ind = line.find_last_of(" ");
	//std::cout << "ind: " << ind << '\n';
	int size;
	sscanf(line.substr(ind+1).c_str(), "%d", &size);
	//std::cout << "substr: " << line.substr(ind+1) << '\n';
	points.resize(size);

	//std::cout << size << '\n';

	search = "NODE_COORD_SECTION";
	do {
		std::getline(file, line);
	} while (line.compare(0, search.length(), search) != 0);

	int dummy;
	for (int i = 0;  i < size;  ++i)
	{
		std::getline(file, line);
		//std::cout << line << '\n';
		sscanf(line.c_str(), "%d %f %f", &dummy, &points[i].x, &points[i].y);
		//std::cout << dummy << ' '<< points[i].x << ' ' << points[i].y << '\n';
	}

	file.close();
	return true;
}


void TSPInstance::precalculateDistances()
{
	dists.resize(points.size());

	for (unsigned i = 0;  i < points.size();  ++i)
		dists[i].resize(points.size());

	for (unsigned i = 0;  i < points.size()-1;  ++i)
	{
		for (unsigned j = i+1;  j < points.size();  ++j)
		{
			float xx = points[i].x - points[j].x;
			float yy = points[i].y - points[j].y;
			dists[i][j] = dists[j][i] = (float) sqrt(xx * xx + yy * yy);
		}
	}
}


void TSPInstance::calculatePointLimits(float & minX, float & maxX, float & minY, float & maxY)
{
	const auto & fx = [](const point & p1, const point & p2) { return p1.x < p2.x; };
	const auto & fy = [](const point & p1, const point & p2) { return p1.y < p2.y; };

	minX = min_element(points.begin(), points.end(), fx)->x;
	maxX = max_element(points.begin(), points.end(), fx)->x;
	minY = min_element(points.begin(), points.end(), fy)->y;
	maxY = max_element(points.begin(), points.end(), fy)->y;
}

// ----------------------------------------------------------------------------
