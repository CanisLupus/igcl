#ifndef OPERATORS_HPP_
#define OPERATORS_HPP_

#include "Individual.hpp"

#include <vector>

// ----------------------------------------------------------------------------

class UnionStrategy
{
protected:
	IndividualSortingFunctor * sortingFunctor;

public:
	UnionStrategy(IndividualSortingFunctor * sortingFunctor);
	virtual ~UnionStrategy();
	virtual void unite(std::vector<Individual *> & population, std::vector<Individual *> & offspring, std::vector<Individual *> & newPopulation) = 0;
};

// ----------------------------------------------------------------------------

class HalfAndHalfUnion : public UnionStrategy
{
public:
	HalfAndHalfUnion(IndividualSortingFunctor * sortingFunctor);
	virtual void unite(std::vector<Individual *> & population, std::vector<Individual *> & offspring, std::vector<Individual *> & newPopulation);
};

// ----------------------------------------------------------------------------

class SurvivorBasedUnion : public UnionStrategy
{
private:
	float survivorFraction;

public:
	SurvivorBasedUnion(float survivorFraction, IndividualSortingFunctor * sortingFunctor);
	virtual void unite(std::vector<Individual *> & population, std::vector<Individual *> & offspring, std::vector<Individual *> & newPopulation);
};

// ----------------------------------------------------------------------------

#endif /* OPERATORS_HPP_ */
