#include "igcl/igcl.hpp"

#include <cstring>
#include <iomanip>

using namespace std;


typedef unsigned int DATATYPE;

#define N_THREADS 2
#define MATSIZE 2048
int nTests = 32;


void fillMatrix(DATATYPE * mat, uint rows, uint cols) {
	for (uint i = 0; i < rows; i++) {
		for (uint j = 0; j < cols; j++) {
			mat[i*cols+j] = (unsigned int) (double(rand()) / RAND_MAX * 10);
		}
	}
}


void transpose(DATATYPE * mat, uint size) {	// only works for square matrices!
	for (uint i = 0; i < size-1; i++) {
		for (uint j = i+1; j < size; j++) {
			swap(mat[i*size+j], mat[j*size+i]);
		}
	}
}

bool confirm(const DATATYPE * mat_a, const DATATYPE * mat_b, const DATATYPE * result, uint size) {	// only works for square matrices!
	#pragma omp parallel for
	for (uint i = 0; i < size; i++) {
		for (uint j = 0; j < size; j++) {
			DATATYPE sum = 0;
			for (uint k = 0; k < size; k++) {
				sum += mat_a[i*size+k] * mat_b[j*size+k];
			}
			//cout << setw(5) << sum << ' ';
			if (sum != result[i*size+j]) {
				cout << "WROOOOOOOOOOOOOOOOOOONG!!! expected: " << sum << " got: " << result[i*size+j] << endl;
				i = size;
				break;
			}
		}
		//cout << endl;
	}
	return true;
}


void runCoordinator(igcl::Coordinator * coord)
{
for(int test=0; test<nTests; test++) {
	DATATYPE * mat_a, * mat_b, * mat_result;
	mat_a = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));
	mat_b = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));
	mat_result = (DATATYPE *) malloc(MATSIZE * MATSIZE * sizeof(DATATYPE));

	srand(0);
	fillMatrix(mat_a, MATSIZE, MATSIZE);
	fillMatrix(mat_b, MATSIZE, MATSIZE);

	transpose(mat_b, MATSIZE);

	timeval iniTime, endTime;
	gettimeofday(&iniTime, NULL);

	#pragma omp parallel for firstprivate(mat_a, mat_b, mat_result) num_threads(N_THREADS)
	for (uint i = 0; i < MATSIZE; i++) {
		for (uint j = 0; j < MATSIZE; j++) {
			DATATYPE sum = 0;
			for (uint k = 0; k < MATSIZE; k++) {
				sum += mat_a[i*MATSIZE+k] * mat_b[j*MATSIZE+k];
			}
			mat_result[i*MATSIZE+j] = sum;
		}
	}

	gettimeofday(&endTime, NULL);
	printf("Time = %ld ms\n", timeDiff(iniTime, endTime));

	//confirm(mat_a, mat_b, mat_result, MATSIZE);

	free(mat_a);
	free(mat_b);
	free(mat_result);
}
}


void runPeer(igcl::Peer * peer)
{

}
