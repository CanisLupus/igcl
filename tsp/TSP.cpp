#include "TSP.hpp"

#include <algorithm>
#include <sstream>

// ----------------------------------------------------------------------------

TSPIndividual::TSPIndividual(std::vector<order_type> & pointOrder)
{
	this->pointOrder = pointOrder;
}

TSPIndividual * TSPIndividual::clone()
{
	TSPIndividual * newIndiv = new TSPIndividual(pointOrder);
	newIndiv->fitness = fitness;
	return newIndiv;
}

// ----------------------------------------------------------------------------

bool TSPIndividualSortingFunctor::operator() (const Individual * indiv1, const Individual * indiv2) const
{
	return indiv1->fitness > indiv2->fitness;
}

// ----------------------------------------------------------------------------

TSP::TSP(TSPInstance & instance, int seed) : GeneticAlgorithm(seed)
{
	this->instance = instance;
	this->uint_dist = std::uniform_int_distribution<uint32_t>(0, instance.points.size()-1);
}


float TSP::evaluateExternalIndividual(const TSPIndividual * indiv)
{
	return evaluateIndividual(indiv);
}


Individual * TSP::generateIndividual()
{
	std::vector<order_type> order(instance.points.size());

	for (unsigned i = 0;  i < instance.points.size();  i++)
		order[i] = i;

	std::random_shuffle(order.begin(), order.end());

	return new TSPIndividual(order);
}


float TSP::evaluateIndividual(const Individual * indivPointer)
{
	const TSPIndividual & indiv = *(const TSPIndividual *) indivPointer;

	float fitness = 0;

	for (unsigned i = 0;  i < indiv.pointOrder.size()-1;  i++) {
		fitness += instance.dists[ indiv.pointOrder[i] ][ indiv.pointOrder[i+1] ];
	}
	fitness += instance.dists[ indiv.pointOrder[indiv.pointOrder.size()-1] ][ indiv.pointOrder[0] ];

	return fitness;
}


void TSP::mutateIndividual(Individual * indivPointer)
{
	TSPIndividual & indiv = *(TSPIndividual *) indivPointer;

	int ini, end;
	getRandomOrderedPairOfIndexes(ini, end);

	for (int i = ini;  i < end-i;  i++)	// reverse items between ini and end
	{
		int t = indiv.pointOrder[i];
		indiv.pointOrder[i] = indiv.pointOrder[end-i];
		indiv.pointOrder[end-i] = t;
	}
}


void TSP::recombinePair(Individual * indiv1, Individual * indiv2)
{
	ox(*(TSPIndividual *) indiv1, *(TSPIndividual *) indiv2);
}


void TSP::ox(TSPIndividual & indiv1, TSPIndividual & indiv2)
{
	unsigned size = instance.points.size();
	int ini, end;
	getRandomOrderedPairOfIndexes(ini, end);

	std::set<order_type> used1(indiv2.pointOrder.begin()+ini, indiv2.pointOrder.begin()+end+1);
	std::set<order_type> used2(indiv1.pointOrder.begin()+ini, indiv1.pointOrder.begin()+end+1);

	unsigned curr1, curr2;
	curr1 = curr2 = (end+1) % size;

	for (int ind = end+1-size;  ind < end+1;  ind++)
	{
		int i = ind;
		if (i < 0)
			i = size + i;

		oxAuxiliar(used1, indiv1, curr1, indiv1.pointOrder[i], size);
		oxAuxiliar(used2, indiv2, curr2, indiv2.pointOrder[i], size);
	}

	for (int i = ini;  i < end+1;  i++)
	{
		order_type t = indiv1.pointOrder[i];
		indiv1.pointOrder[i] = indiv2.pointOrder[i];
		indiv2.pointOrder[i] = t;
	}
}


std::ostream & operator << (std::ostream & os, TSPIndividual & indiv)
{
	os << indiv.pointOrder[0];
	for (unsigned i = 1;  i < indiv.pointOrder.size();  i++)
		os << ',' << indiv.pointOrder[i];
	return os;
}

// ----------------------------------------------------------------------------
