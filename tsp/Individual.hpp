#ifndef INDIVIDUAL_HPP_
#define INDIVIDUAL_HPP_

// ----------------------------------------------------------------------------

class Individual
{
public:
	float fitness;
	bool hasChanged;

	Individual();
	virtual ~Individual();
	virtual Individual * clone() = 0;
};

// ----------------------------------------------------------------------------

class IndividualSortingFunctor
{
public:
	virtual ~IndividualSortingFunctor();
	virtual bool operator() (const Individual * indiv1, const Individual * indiv2) const;
};

// ----------------------------------------------------------------------------

#endif /* INDIVIDUAL_HPP_ */
