#include "igcl/igcl.hpp"

#include <iostream>
#include <sstream>
#include <string>

#define PROBLEM 4		// EVOLUTIONARY:0, MATRIX_MULT:1, RAYTRACER:2, SORT:3, TSP:4

#if (PROBLEM == 0)
	#include "MainIslandModel.hpp"
#elif (PROBLEM == 15)
	#include "MainMatrixMultiplication_Buffered.hpp"
#elif (PROBLEM == 18)
	#include "MainMatrixMultiplication_VsMPI.hpp"
#elif (PROBLEM == 19)
	#include "MainMatrixMultiplication_Threads.hpp"
#elif (PROBLEM == 2)
	#include "MainRaytracing.hpp"
#elif (PROBLEM == 25)
	#include "MainRaytracing_Threads.hpp"
#elif (PROBLEM == 28)
	#include "MainRaytracing_Small.hpp"
#elif (PROBLEM == 3)
	#include "MainSort.hpp"
#elif (PROBLEM == 4)
	#include "MainParallelTSP.hpp"
#endif


int main(int argc, char ** argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	int isCoordinator = 1;
	char * coordinatorIp = NULL;
	int ownPort = -1, coordinatorPort = -1;

	bool correctArgs = false;

	if (argc >= 3) {
		std::stringstream ss;

		isCoordinator = stoi(argv[1]);
		ownPort = stoi(argv[2]);
		ss << isCoordinator << ' ' << ownPort;

		if (!isCoordinator and argc >= 5) {
			coordinatorIp = argv[3];
			coordinatorPort = stoi(argv[4]);
			ss << ' ' << coordinatorIp << ' ' << coordinatorPort;
		}

		if (isCoordinator or (!isCoordinator and argc >= 5)) {
			correctArgs = true;
			dbg("command line args:", ss.str());
		}
	}

	if (!correctArgs) {
		std::cout << argc << std::endl;
		std::cout << "Usage (  coordinator  ): executable  1  ownPort" << std::endl;
		std::cout << "Usage (non-coordinator): executable  0  ownPort  coordinatorIp  coordinatorPort" << std::endl;
		return 0;
	}

#ifdef TEST_READY
	if (isCoordinator and argc > 3) setSize  (stoi(argv[3]));
	if (isCoordinator and argc > 4) setNTests(stoi(argv[4]));
	if (isCoordinator and argc > 5) setNNodes(stoi(argv[5]));
	if (!isCoordinator and argc > 5) setSize  (stoi(argv[5]));
	if (!isCoordinator and argc > 6) setNTests(stoi(argv[6]));
	if (!isCoordinator and argc > 7) setNNodes(stoi(argv[7]));
#endif

	if (isCoordinator) {
		auto node = new igcl::Coordinator(ownPort);
		runCoordinator(node);
		delete node;
	} else {
		auto node = new igcl::Peer(ownPort, coordinatorIp, coordinatorPort);
		runPeer(node);
		delete node;
	}

	return 0;
}
