//#include "ttmath/ttmath.h"
#include <algorithm>
#include <cstdlib>
#include <stack>
#include <iostream>

#include "igcl/igcl.hpp"

using namespace std;

#define TEST_READY
//#define NUMSIZE 1024

//typedef ttmath::UInt<NUMSIZE> DATATYPE;
typedef unsigned int DATATYPE;

int ARRAYSIZE = 30000000;
int nTests = 33;
int nParticipants = 4;
void setSize(int val)   { ARRAYSIZE = val; }
void setNTests(int val) { nTests = val; }
void setNNodes(int val) { nParticipants = val; }


void fill(DATATYPE * array, uint size)
{
	for (uint i = 0; i < size; i++) {
		array[i] = (int) (double(rand()) / RAND_MAX * INT_MAX);
	}
}


void joinSort(const DATATYPE * arrayA, const uint sizeA, const DATATYPE * arrayB, const uint sizeB, DATATYPE * finalArray)
{
	uint a = 0, b = 0, i = 0;

	while (a < sizeA and b < sizeB)
	{
		if (arrayA[a] < arrayB[b])
			finalArray[i++] = arrayA[a++];
		else
			finalArray[i++] = arrayB[b++];
	}

	while (a < sizeA)
		finalArray[i++] = arrayA[a++];

	while (b < sizeB)
		finalArray[i++] = arrayB[b++];
}


bool confirm(DATATYPE * array)
{
	for (int i = 0; i < ARRAYSIZE-1; i++) {
		if (array[i] > array[i+1]) {
			return false;
		}
	}
	return true;
}



void work(igcl::Node * node)
{
for (int test=0; test<nTests; ++test)
{
	//printf("STARTED\n");
	igcl::peer_id id = node->getId();

	timeval iniTimeGlobal, endTimeGlobal;

	DATATYPE * array, * finalArray;
	uint originalSize = 0, size;
	igcl::peer_id parent;

	if (id == 0)
	{
		array = (DATATYPE*) malloc(ARRAYSIZE*sizeof(DATATYPE));
		originalSize = ARRAYSIZE;

		srand(0);
		fill(array, originalSize);

		//cout << "STARTED" << endl;
		gettimeofday(&iniTimeGlobal, NULL);

	}

	if (id > 0)
	{
		node->recvBranch(array, originalSize, parent);
	}

	node->branch<2>(array, originalSize, 1, size);

	std::sort(array, array+size);

	if (node->nDownstreamPeers() > 0)
	{
		finalArray = (DATATYPE*) malloc(originalSize*sizeof(DATATYPE));
		igcl::result_type res = node->merge(finalArray, originalSize, 1, array, size, joinSort);
		if (res != igcl::SUCCESS) {
			return;
		}
		free(array);
		array = finalArray;
	}

	if (id > 0)
	{
		node->returnBranch(array, originalSize, 1, parent);
	}

	if (id == 0)
	{
		gettimeofday(&endTimeGlobal, NULL);
		printf("Time = %ld ms (parallel)\n", timeDiff(iniTimeGlobal, endTimeGlobal));

		if (!confirm(array)) {
			printf("ARRAY IS NOT SORTED!!!!!!!\n");
			/*for (int i=0; i<ARRAYSIZE; i++) {
				std::cout << array[i] << " ";
			}
			std::cout << std::endl;*/
		//} else {
			//printf("ARRAY IS SORTED\n");
		}

		// fill array again and test sequential speed for comparison
		/*srand(0);
		fill(array, ARRAYSIZE);

		timeval iniTime, endTime;
		gettimeofday(&iniTime, NULL);
		std::sort(array, array+ARRAYSIZE);
		gettimeofday(&endTime, NULL);
		printf("[%d] time = %ld ms (sequential)\n", id, timeDiff(iniTime, endTime));*/
	}

	//printf("ENDED\n");

	free(array);
}
}


void runCoordinator(igcl::Coordinator * coord)
{
	GroupLayout layout = GroupLayout::getSortTreeLayout(nParticipants, 2);
	//GroupLayout layout = GroupLayout::getAllToAllLayout(nParticipants);
	coord->setLayout(layout);

	coord->start();
	coord->waitForNodes(nParticipants);
	work(coord);
	//coord->hang();
	coord->terminate();
}


void runPeer(igcl::Peer * peer)
{
	peer->start();
	work(peer);
	peer->hang();
}
