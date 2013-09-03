#include "GeneticAlgorithm.hpp"

#include <algorithm>

// ----------------------------------------------------------------------------

GeneticAlgorithm::GeneticAlgorithm(int seed)
{
	if (seed == 0) {
		rng.seed(time(NULL));
		srand(time(NULL));
	} else {
		rng.seed(seed);
		srand(seed);
	}

	doneGenerations = 0;
	doneMutations = 0;
	doneRecombinations = 0;
	doneEvaluations = 0;
}

GeneticAlgorithm::~GeneticAlgorithm()
{
	for (Individual * indiv : population)
		delete indiv;
}

void GeneticAlgorithm::start()
{
	generate(population);
	evaluate(population);
	std::sort(population.begin(), population.end(), *sortingFunctor);

	++doneGenerations;
}

void GeneticAlgorithm::loop()
{
	population_type offspring;
	select(population, offspring);	// clones selected population to offspring

	mutate(offspring);
	recombine(offspring); // includes population shuffling
	evaluate(offspring);

	population_type newPopulation;
	unite(population, offspring, newPopulation); // includes population sorting

	// delete every old Individual
	for (Individual * indiv : population)
		delete indiv;
	for (Individual * indiv : offspring)
		delete indiv;

	population = newPopulation;

	++doneGenerations;
}

void GeneticAlgorithm::generate(population_type & population)
{
	population.resize(populationSize);

	for (unsigned i = 0; i < population.size(); i++) {
		population[i] = generateIndividual();
		population[i]->hasChanged = true;
	}
}

// evaluation and in-place fitness setting
void GeneticAlgorithm::evaluate(const population_type & population)
{
	for (unsigned i = 0; i < population.size(); i++) {
		if (population[i]->hasChanged) {
			population[i]->fitness = evaluateIndividual(population[i]);
			population[i]->hasChanged = false;
			++doneEvaluations;
		}
	}
}

// in-place mutation
void GeneticAlgorithm::mutate(population_type & population)
{
	for (unsigned i = 0; i < population.size(); i++) {
		if (random() < mutationChance) {
			mutateIndividual(population[i]);
			population[i]->hasChanged = true;
			++doneMutations;
		}
	}
}

// in-place recombination
void GeneticAlgorithm::recombine(population_type & population)
{
	std::random_shuffle(population.begin(), population.end());

	for (unsigned i = 0; i < population.size() / 2 * 2; i += 2) {
		if (random() < crossoverChance) {
			recombinePair(population[i], population[i+1]);
			population[ i ]->hasChanged = true;
			population[i+1]->hasChanged = true;
			++doneRecombinations;
		}
	}
}

// selection of offspring
void GeneticAlgorithm::select(const population_type & population, population_type & offspring)
{
	offspring.resize(population.size());

	for (unsigned i = 0; i < population.size(); i++)
		offspring[i] = population[i]->clone();
}

void GeneticAlgorithm::unite(population_type & population, population_type & offspring, population_type & newPopulation)
{
	unionStrategy->unite(population, offspring, newPopulation);
}

double GeneticAlgorithm::random()
{
	return ureal_dist(rng);
}

// ----------------------------------------------------------------------------
