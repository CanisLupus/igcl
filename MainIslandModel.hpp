#include "igcl/igcl.hpp"

#include "tsp/Operators.hpp"
#include "tsp/GeneticAlgorithm.hpp"
#include "tsp/TSP.hpp"

#include <iomanip>

using namespace std;


TSPInstance * loadProblemInstance()
{
	TSPInstance * instance = new TSPInstance();

	bool valid = false;
	valid = instance->initializeFromFile("tsp/tsp_datasets/berlin52.tsp");
	if (!valid)
		return NULL;

	instance->precalculateDistances();

	cout << "Dataset size: " << instance->points.size() << endl;

	return instance;
}


TSP * loadAlgorithm(TSPInstance * instance)
{
	TSP * tsp = new TSP(*instance);
	tsp->sortingFunctor = new TSPIndividualSortingFunctor();
	tsp->populationSize = 100;
	tsp->mutationChance = 0.3;
	tsp->crossoverChance = 0.8;
	tsp->unionStrategy = new HalfAndHalfUnion(tsp->sortingFunctor);
	return tsp;
}


void work(igcl::Peer * peer)
{
	TSPInstance * instance = loadProblemInstance();
	TSP * tsp = loadAlgorithm(instance);

	timeval globalIniTime, ini, end;
	cout << "START" << endl;
	gettimeofday(&globalIniTime, NULL);

	tsp->start();

	TSPIndividual * bestIndiv = (TSPIndividual *) tsp->population[0];
	cout << "Generation " << tsp->doneGenerations << ": best fitness " << bestIndiv->fitness << endl;
	cout << *bestIndiv << endl;

	double realTime = 0;
	uint individualSize = instance->points.size();

	while (1)
	{
		gettimeofday(&ini, NULL);
		tsp->loop();
		gettimeofday(&end, NULL);
		realTime += timeDiff(ini, end) / 1000.0;

		if (tsp->doneGenerations % ((int) (100 * 400.0/individualSize)) == 0)
		{
			gettimeofday(&end, NULL);

			bestIndiv = (TSPIndividual *) tsp->population[0];
			cout << "\nGeneration " << tsp->doneGenerations << ": best fitness " << bestIndiv->fitness << endl;
			cout << *bestIndiv << endl;

			double diff = timeDiff(globalIniTime, end) / 1000.0;
			cout << "time elapsed: " << diff << " sec" << endl;
			cout << "useful time:  " << realTime << " sec" << endl;

			cout << "doneGenerations:    " << setw(9) << tsp->doneGenerations		<< " (" << setw(8) << tsp->doneGenerations/diff		<< " p/sec)" << endl;
			cout << "doneMutations:      " << setw(9) << tsp->doneMutations			<< " (" << setw(8) << tsp->doneMutations/diff		<< " p/sec)" << endl;
			cout << "doneRecombinations: " << setw(9) << tsp->doneRecombinations	<< " (" << setw(8) << tsp->doneRecombinations/diff	<< " p/sec)" << endl;
			cout << "doneEvaluations:    " << setw(9) << tsp->doneEvaluations		<< " (" << setw(8) << tsp->doneEvaluations/diff		<< " p/sec)" << endl;

			int count = 0;
			int * received = NULL;
			uint matSize = 0;
			igcl::peer_id id;

			while (peer->tryRecvNewFromAny(id, received, matSize) == igcl::SUCCESS)
			{
				std::cout << "received stuff from: " << id << std::endl;
				vector<order_type> v(received, received+matSize);
				free(received);
				TSPIndividual * indiv = new TSPIndividual(v);
				std::cout << "received indiv: " << *indiv << std::endl;

				indiv->fitness = tsp->evaluateExternalIndividual(indiv);

				uint index = tsp->population.size()-1-count;
				delete tsp->population[index];
				tsp->population[index] = indiv;

				++count;
			}

			int indiv_arr[individualSize];
			copy(bestIndiv->pointOrder.begin(), bestIndiv->pointOrder.end(), indiv_arr+0);
			if (peer->sendToAll(indiv_arr+0, individualSize) == igcl::FAILURE) {
			//if (peer->sendToAllPeersThroughCoordinator(indiv_arr+0, individualSize) == igcl::FAILURE) {
				std::cout << "FAILURE" << std::endl;
				return;
			}

			//peer->barrier();
		}
	}
}


void runCoordinator(igcl::Coordinator * coord)
{
	coord->setLayout(GroupLayout::getFreeAllToAllLayout());
	coord->start();
	/* do nothing special */
	coord->hang();
}


void runPeer(igcl::Peer* peer)
{
	peer->start();
	work(peer);
	peer->terminate();
}
