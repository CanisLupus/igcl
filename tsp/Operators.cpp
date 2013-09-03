#include "Operators.hpp"

#include <algorithm>

// ----------------------------------------------------------------------------

UnionStrategy::UnionStrategy(IndividualSortingFunctor * sortingFunctor) : sortingFunctor(sortingFunctor)
{
}

UnionStrategy::~UnionStrategy()
{
}

// ----------------------------------------------------------------------------

HalfAndHalfUnion::HalfAndHalfUnion(IndividualSortingFunctor * sortingFunctor) : UnionStrategy(sortingFunctor)
{
}

void HalfAndHalfUnion::unite(std::vector<Individual *> & population, std::vector<Individual *> & offspring, std::vector<Individual *> & newPopulation)
{
	std::vector<Individual *> joinedPopulation(population.size() + offspring.size());
	std::copy(population.begin(), population.end(), joinedPopulation.begin());
	std::copy(offspring.begin(), offspring.end(), joinedPopulation.begin()+population.size());
	std::sort(joinedPopulation.begin(), joinedPopulation.end(), *sortingFunctor);

	// clone all to new population
	newPopulation.resize(population.size());
	for (unsigned i = 0;  i < population.size();  i++)
		newPopulation[i] = joinedPopulation[i]->clone();
}

// ----------------------------------------------------------------------------

SurvivorBasedUnion::SurvivorBasedUnion(float survivorFraction, IndividualSortingFunctor * sortingFunctor) : UnionStrategy(sortingFunctor)
{
	this->survivorFraction = survivorFraction;
}

void SurvivorBasedUnion::unite(std::vector<Individual *> & population, std::vector<Individual *> & offspring, std::vector<Individual *> & newPopulation)
{
	std::sort(population.begin(), population.end(), *sortingFunctor);
	std::sort(offspring.begin(), offspring.end(), *sortingFunctor);

	// clone all to new population
	newPopulation.resize(population.size());
	unsigned nSurvivors = (int) (survivorFraction * population.size());
	
	for (unsigned i = 0;  i < nSurvivors;  i++)
		newPopulation[i] = population[i]->clone();
	for (unsigned i = 0;  i < population.size()-nSurvivors;  i++)
		newPopulation[nSurvivors+i] = offspring[i]->clone();

	std::sort(newPopulation.begin(), newPopulation.end(), *sortingFunctor);
}

// ----------------------------------------------------------------------------
