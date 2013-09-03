/*
 * GeneticAlgorithm.hpp
 *
 *  Created on: 27 de Fev de 2013
 *      Author: Daniel Lobo
 */

#ifndef GENETICALGORITHM_HPP_
#define GENETICALGORITHM_HPP_

#include "Operators.hpp"
#include "Individual.hpp"

#include <random>
#include <vector>

typedef std::vector<Individual *> population_type;

// ----------------------------------------------------------------------------

class GeneticAlgorithm
{
public:
	int populationSize;
	float mutationChance;
	float crossoverChance;

	UnionStrategy * unionStrategy;
	IndividualSortingFunctor * sortingFunctor;
	population_type population;

	uint doneGenerations;
	uint doneMutations;
	uint doneRecombinations;
	uint doneEvaluations;

protected:
	std::mt19937 rng;

private:
	std::uniform_real_distribution<double> ureal_dist;

public:
	GeneticAlgorithm(int seed = 0);
	virtual ~GeneticAlgorithm();
	void start();
	void loop();

protected:
	void generate(population_type & population);
	void evaluate(const population_type & population);
	void mutate(population_type & population);
	void recombine(population_type & population);

	void select(const population_type & population, population_type & offspring);
	void unite(population_type & population, population_type & offspring, population_type & newPopulation);

	virtual Individual * generateIndividual() = 0;
	virtual float evaluateIndividual(const Individual * indiv) = 0;
	virtual void mutateIndividual(Individual * indiv) = 0;
	virtual void recombinePair(Individual * indiv1, Individual * indiv2) = 0;

private:
	double random();
};

// ----------------------------------------------------------------------------

#endif /* GENETICALGORITHM_HPP_ */
