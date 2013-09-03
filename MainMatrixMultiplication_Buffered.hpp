#include "igcl/igcl.hpp"

#include "MainMatrixMultiplicationAux.hpp"

//#include <ttmath/ttmath.h>

using namespace std;

#define TEST_READY
#define NUMSIZE 1024
#define MATSIZE 1024

//typedef ttmath::UInt<NUMSIZE> DATATYPE;
typedef unsigned int DATATYPE;

int bufferingLevel = 100;
int nTests = 1;
int nParticipants = 2;
void setSize(int val)   { /*std::cout << "setSize with val=" << val << std::endl;*/ bufferingLevel = val; }
void setNTests(int val) { /*std::cout << "setNTests with val=" << val << std::endl;*/ nTests = val; }
void setNNodes(int val) { /*std::cout << "setNNodes with val=" << val << std::endl;*/ nParticipants = val; }


igcl::Coordinator * coord;
igcl::NBuffering * buffering;
DATATYPE * mat1, * mat2, * matRes;


void generateMatrix(DATATYPE * & matrix, const uint size)
{
	matrix = new DATATYPE[size*size];

	for (uint i = 0; i < size; i++) {
		for (uint j = 0; j < size; j++) {
			matrix[i*size+j] = (int) (double(rand()) / RAND_MAX * 10);//INT_MAX);
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


void sendJob(igcl::peer_id id, uint row)
{
	//cout << "sendJob " << nSentIndexes << endl;
	coord->sendTo(id, DATA_ROW);
	coord->sendTo(id, mat1+row*MATSIZE, MATSIZE);
}

void receiveResult()
{
	//cout << "receiveJob" << endl;
	igcl::peer_id sourceId;
	DATATYPE * result = NULL;
	coord->waitRecvNewFromAny(sourceId, result);

	uint row = buffering->completeJob(sourceId);

	for (uint i=0; i<MATSIZE; ++i) {
		matRes[row*MATSIZE+i] = result[i];
	}

	buffering->bufferTo(sourceId);
}


void start(timeval & iniTime)
{
	srand(0);
	generateMatrix(mat1, MATSIZE);
	generateMatrix(mat2, MATSIZE);
	transposeMatrix(mat2, MATSIZE);
	matRes = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));

	gettimeofday(&iniTime, NULL);
}

void finish(const timeval & iniTime) {
	timeval endTime;
	gettimeofday(&endTime, NULL);
	long t = timeDiff(iniTime, endTime);
	cout << "total time: " << t << " ms" << endl;

	confirm(mat1, mat2, matRes, MATSIZE);

	//cout << "finished. resulting matrix:\n";
	//printMatrix(matRes, MATSIZE);
}


void runCoordinator(igcl::Coordinator * me_)
{
	coord = me_;
	coord->start();
	coord->waitForNodes(nParticipants);

for (int i=0; i<nTests; i++) {
	igcl::NBuffering buf(bufferingLevel, MATSIZE, 1, sendJob);
	buffering = &buf;
	buffering->addPeers(coord->downstreamPeers());

	timeval iniTime;
	start(iniTime);

	coord->sendToAll(DATA_COL);
	coord->sendToAll(mat2, MATSIZE * MATSIZE);

	buffering->bufferToAll();
	while (!buffering->allJobsCompleted())
		receiveResult();

	finish(iniTime);
}

	coord->terminate();
}


void runPeer(igcl::Peer * peer)
{
	peer->start();

	timeval ini, end;
	gettimeofday(&ini, NULL);

	DATATYPE * mat2 = NULL;
	DATATYPE * result = (DATATYPE *) malloc(MATSIZE*sizeof(DATATYPE));

	while (1)
	{
		char type = -1;
		peer->waitRecvFrom(0, type);

		if (type == DATA_COL)
		{
			if (mat2 != NULL)
				free(mat2);
			peer->waitRecvNewFrom(0, mat2);
		}
		else
		{
			DATATYPE * row = NULL;
			peer->waitRecvNewFrom(0, row);
			//cout << "received column" << endl;

			for (uint col=0; col<MATSIZE; ++col) {
				multiplyRows(mat2+col*MATSIZE, row, MATSIZE, result[col]);
			}
			//cout << "sending result" << endl;
			peer->sendTo(0, result, MATSIZE);
			free(row);
		}
	}

	free(result);

	gettimeofday(&end, NULL);
	long t = timeDiff(ini, end);
	cout << "time: " << t << " ms" << endl;

	peer->hang();
}
