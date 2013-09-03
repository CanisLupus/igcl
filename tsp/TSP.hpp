#ifndef TSP_HPP_
#define TSP_HPP_

#include "GeneticAlgorithm.hpp"
#include "TSPInstance.hpp"

#include <vector>
#include <set>
#include <random>

typedef short order_type;

// ----------------------------------------------------------------------------

class TSPIndividual : public Individual
{
public:
	std::vector<order_type> pointOrder;

	TSPIndividual(std::vector<order_type> & pointOrder);
	TSPIndividual * clone();

	friend std::ostream & operator << (std::ostream & os, TSPIndividual & p);
};

// ----------------------------------------------------------------------------

class TSPIndividualSortingFunctor : public IndividualSortingFunctor
{
public:
	bool operator() (const Individual * indiv1, const Individual * indiv2) const;
};

// ----------------------------------------------------------------------------

class TSP : public GeneticAlgorithm
{
private:
	TSPInstance instance;
	std::uniform_int_distribution<uint32_t> uint_dist;

public:
	TSP(TSPInstance & instance, int seed=0);
	float evaluateExternalIndividual(const TSPIndividual * indiv);

protected:
	virtual Individual * generateIndividual();
	virtual float evaluateIndividual(const Individual * indiv);
	virtual void mutateIndividual(Individual * indiv);
	virtual void recombinePair(Individual * indiv1, Individual * indiv2);

private:
	void ox(TSPIndividual & indiv1, TSPIndividual & indiv2);

	inline void getRandomOrderedPairOfIndexes(int & ini, int & end)
	{
		ini = uint_dist(rng);
		while (ini == (end = uint_dist(rng)));

		if (ini > end)	// ini must come before end
		{
			int t = ini;
			ini = end;
			end = t;
		}
	}

	inline void oxAuxiliar(std::set<order_type> & used, TSPIndividual & indiv, unsigned & curr, const order_type value, const unsigned size)
	{
		if (used.count(value) == 0)
		{
			indiv.pointOrder[curr] = value;
			used.insert(value);
			if (++curr == size)
				curr = 0;
		}
	}
};

// ----------------------------------------------------------------------------

#endif /* TSP_HPP_ */
