#include "igcl/igcl.hpp"

//#include "ttmath/ttmath.h"
#include <cstring>
#include <iomanip>

using namespace std;

#define TEST_READY
//#define NUMSIZE 1024

//typedef ttmath::UInt<NUMSIZE> DATATYPE;
typedef unsigned int DATATYPE;

int MATSIZE =  1024;
int nTests = 30;
int nParticipants = 1;
void setSize(int val)   { /*std::cout << "setSize with val=" << val << std::endl;*/ MATSIZE = val; }
void setNTests(int val) { /*std::cout << "setNTests with val=" << val << std::endl;*/ nTests = val; }
void setNNodes(int val) { /*std::cout << "setNNodes with val=" << val << std::endl;*/ nParticipants = val; }


void fillMatrix(DATATYPE * mat, uint rows, uint cols) {
	for (uint i = 0; i < rows; i++) {
		for (uint j = 0; j < cols; j++) {
			mat[i*cols+j] = (unsigned int) (double(rand()) / RAND_MAX * 10);//INT_MAX);
		}
	}
}

void printMatrix(const DATATYPE * mat, uint rows, uint cols) {
	for (uint i = 0; i < rows; i++) {
		for (uint j = 0; j < cols; j++)
			cout << setw(5) << mat[i*cols+j] << ' ';
		cout << endl;
	}
	cout << endl;
}

void transpose(DATATYPE * mat, uint size) {	// only works for square matrices!
	for (uint i = 0; i < size-1; i++) {
		for (uint j = i+1; j < size; j++) {
			swap(mat[i*size+j], mat[j*size+i]);
		}
	}
}

bool confirm(const DATATYPE * mat_a, const DATATYPE * mat_b, const DATATYPE * result, uint size) {	// only works for square matrices!
	for (uint i = 0; i < size; i++) {
		for (uint j = 0; j < size; j++) {
			DATATYPE sum = 0;
			for (uint k = 0; k < size; k++) {
				sum += mat_a[i*size+k] * mat_b[j*size+k];
			}
			//cout << setw(5) << sum << ' ';
			if (sum != result[i*size+j]) {
				cout << "WROOOOOOOOOOOOOOOOOOONG!!! expected: " << sum << " got: " << result[i*size+j] << endl;
				return false;
			}
		}
		//cout << endl;
	}
	return true;
}




void work(igcl::Node * node)
{
for(int test=0; test<nTests; test++) {
	igcl::peer_id id = node->getId();

	timeval iniTime, endTime;

	uint iniRowIndex, endRowIndex;
	DATATYPE * mat_a, * mat_b, * mat_result;
	igcl::peer_id masterId;

	mat_result = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));
	//cout << "STARTED" << endl;


	if (id == 0)
	{
		mat_a = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));
		mat_b = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));

		srand(0);
		fillMatrix(mat_a, MATSIZE, MATSIZE);
		fillMatrix(mat_b, MATSIZE, MATSIZE);

		transpose(mat_b, MATSIZE);

		gettimeofday(&iniTime, NULL);
	}

	if (id == 0)	// master distributes data to slaves
	{
		node->sendToAll(mat_b, MATSIZE * MATSIZE);
		node->distribute(mat_a, MATSIZE, MATSIZE, iniRowIndex, endRowIndex);
	}

	if (id > 0)
	{
		node->waitRecvNewFromAny(masterId, mat_b);
		node->recvSection(mat_a, iniRowIndex, endRowIndex, masterId);
	}

	for (uint i = 0; i < endRowIndex-iniRowIndex; i++) {			// do row-column multiplications
		for (int j = 0; j < MATSIZE; j++) {
			DATATYPE sum = 0;
			for (int k = 0; k < MATSIZE; k++) {
				sum += mat_a[i*MATSIZE+k] * mat_b[j*MATSIZE+k];
			}
			mat_result[i*MATSIZE+j] = sum;
		}
	}

	if (id > 0)
	{
		node->sendResult(mat_result, endRowIndex-iniRowIndex, MATSIZE, iniRowIndex, masterId);
	}

	if (id == 0)	// master gathers results from all slaves
	{
		node->collect(mat_result, MATSIZE, MATSIZE);

		gettimeofday(&endTime, NULL);
		printf("Time = %ld ms (parallel)\n", timeDiff(iniTime, endTime));

		//std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		//gettimeofday(&iniTime, NULL);
		//confirm(mat_a, mat_b, mat_result, MATSIZE);
		//gettimeofday(&endTime, NULL);
		//printf("Time = %ld ms (sequential)\n", timeDiff(iniTime, endTime));

		/*if (MATSIZE < 20) {
			printMatrix(mat_a, MATSIZE, MATSIZE);
			transpose(mat_b, MATSIZE);
			printMatrix(mat_b, MATSIZE, MATSIZE);
			printMatrix(mat_result, MATSIZE, MATSIZE);
		}

		printMatrix(mat_result, MATSIZE, MATSIZE);*/
	}

	//cout << "ENDED" << endl;

	free(mat_a);
	free(mat_b);
	free(mat_result);
}
}


void runCoordinator(igcl::Coordinator * coord)
{
	//GroupLayout layout = GroupLayout::getSortTreeLayout(nPeers, 2);
	//GroupLayout layout = GroupLayout::getAllToAllLayout(nPeers);
	//coord->setLayout(layout);

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
