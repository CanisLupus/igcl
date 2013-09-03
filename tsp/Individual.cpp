#include "Individual.hpp"

// ----------------------------------------------------------------------------

Individual::Individual()
{
	hasChanged = false;
}

Individual::~Individual()
{
}

// ----------------------------------------------------------------------------

IndividualSortingFunctor::~IndividualSortingFunctor()
{
}

bool IndividualSortingFunctor::operator() (const Individual * indiv1, const Individual * indiv2) const
{
	return indiv1->fitness < indiv2->fitness;
}

// ----------------------------------------------------------------------------
