#include "igcl/igcl.hpp"
#include "tsp/TSPInstance.hpp"

#include <algorithm>
#include <iostream>
#include <cmath>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

using namespace std;

#define TEST_READY
int size = 16;
int nTests = 33;
int nParticipants = 8;
void setSize  (int val) { size = val; }
void setNTests(int val) { nTests = val; }
void setNNodes(int val) { nParticipants = val; }

#define INF 100000000.0
#define END_MINDIST_TAG -1.0f

#define BITSET_OF_ONES(n) ((1 << n) - 1)
#define GET_BIT(set,i)    ((set >> i) & 1)
#define SET_BIT(set,i)    (set | (1 << i))
#define UNSET_BIT(set,i)  (set & (~(1 << i)))

typedef unsigned long long visited_type;

//#define DISABLE_EXCHANGE


uint n;
visited_type allVisited;
std::vector< std::vector<float> > dists;

igcl::Node * node;
atomic<bool> isEnding;
float mindist;
uint nFinishedPeers;

std::mutex distMutex;
std::mutex quittingMutex;


void updateBound()
{
	float lastValue = INF;
	igcl::peer_id id;
	float * value;
	uint valueSize;
	bool sentLast = false;

	std::unique_lock<std::mutex> mindistLock(distMutex);
	mindistLock.unlock();

	while (1)
	{
		if (isEnding) {
			if (!sentLast) {
				if (node->getId() != 0 or nFinishedPeers == node->getNPeers()-1) {
					//cout << "sent end" << endl;
					float value[2];
					value[0] = mindist;
					value[1] = END_MINDIST_TAG;
					node->sendToAll(value);
					//cout << "sent to all" << endl;
					sentLast = true;

					if (nFinishedPeers == node->getNPeers()-1) {
						//cout << "returning" << endl;
						return;
					}
				}
			}
		} else {
#ifndef DISABLE_EXCHANGE
			mindistLock.lock();
			float tmp = mindist;
			mindistLock.unlock();

			if (tmp < lastValue) {
				node->sendToAll(tmp);
				lastValue = tmp;
			}
#endif
		}

		while (node->tryRecvNewFromAny(id, value, valueSize) == igcl::SUCCESS)
		{
			//cout << "(from " << id << ") ";
			if (valueSize == 2) {
				//cout << "tag: " << value[1] << ", value: " << value[0] << endl;
				nFinishedPeers++;
				//cout << "end finished:" << nFinishedPeers << " id: " << id << endl;
			} else {
				//assert(valueSize == 1);
				//cout << "value: " << value[0] << endl;
			}

			float received = value[0];
			free(value);

#ifndef DISABLE_EXCHANGE
			mindistLock.lock();
			if (mindist > received) {
				mindist = received;
				lastValue = mindist;
			}
			mindistLock.unlock();
#endif

			if (nFinishedPeers == node->getNPeers()-1 and sentLast) {
				//cout << "returning" << endl;
				return;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


void TSP(visited_type visited, uint ind, float curdist)
{
	if (visited == allVisited) {	// no place left to go to (and distance previously confirmed to be smaller than current min)
#ifndef DISABLE_EXCHANGE
		std::unique_lock<std::mutex> minDistLock(distMutex);
#endif
		if (mindist > curdist) {
			mindist = curdist;
		}
#ifndef DISABLE_EXCHANGE
		minDistLock.unlock();
#endif
		return;
	}

	for (uint i = 0; i < n; i++)	// for each not-visited place
	{
		if (GET_BIT(visited, i) == 0)
		{
			float res = curdist + dists[ind][i];
			if (res < mindist) {	// check if it's possible to find a smaller (than mindist) distance this way
				TSP(SET_BIT(visited, i), i, res);	/* next call, with place set as visited */
			}
		}
	}
}


void solve(uint iniI, uint endI)
{
	// reset
	mindist = INF;
	nFinishedPeers = 0;
	isEnding = false;
	std::thread th(updateBound);

	timeval globalIniTime, end;
	//cout << "START" << endl;
	gettimeofday(&globalIniTime, NULL);

	visited_type visited = 0;

	// initial call. chooses the origin for the remaining algorithm
	for(uint i = iniI; i < endI; i++) {
		TSP(SET_BIT(visited, i), i, 0);
	}

	isEnding = true;
	th.join();
	// when thread joins, we are guaranteed to have received everything from other nodes in this test

	gettimeofday(&end, NULL);
	double diff = timeDiff(globalIniTime, end) / 1000.0;
	cout << "time elapsed: " << diff << " sec" << endl;
	//cout << "mindist: " << mindist << endl;
}


void work(igcl::Node * node)
{
	TSPInstance * instance = new TSPInstance();
	//bool valid = instance->initializeFromFile("tsp/tsp_datasets/wi29.tsp");
	bool valid = instance->initializeFromFile("tsp/tsp_datasets/berlin52.tsp");
	if (!valid)
		valid = instance->initializeFromFile("../tsp/tsp_datasets/berlin52.tsp");
	if (!valid)
		return;

	instance->precalculateDistances();

	dists = instance->dists;
	n = std::min((uint) ::size, (uint) instance->points.size());
	allVisited = BITSET_OF_ONES(n);

	uint iniI, endI;
	uint sizePerPeer = n / node->getNPeers();
	int remainder = n % node->getNPeers();

	if (node->getId() < remainder) {
		iniI = sizePerPeer * node->getId() + node->getId();
		endI = iniI + sizePerPeer + 1;
	} else {
		iniI = sizePerPeer * node->getId() + remainder;
		endI = iniI + sizePerPeer;
	}

	//cout << iniI << " " << endI << endl;

	for (int test=0; test<nTests; ++test)
	{
		//std::this_thread::sleep_for(std::chrono::milliseconds(4000));

		float dummy;
		igcl::peer_id dummyId;

		if (node->getId() == 0) {		// synchronize everything for next test
			node->sendToAll((float) 12345.5);

			for (int i=0; i<nParticipants-1; i++) {
				node->waitRecvFromAny(dummyId, dummy);
				if (!(dummy == (float) 54321.5)) {
					cout << dummy << " != 54321.5 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
					uint a;
					std::cin >> a;
				}
			}

			node->sendToAll((float) 34512.5);
		} else {
			node->waitRecvFrom(0, dummy);
			if (!(dummy == (float) 12345.5)) {
				cout << dummy << " != 12345.5 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
				uint a;
				std::cin >> a;
			}
			node->sendTo(0, (float) 54321.5);
			node->waitRecvFrom(0, dummy);
			if (!(dummy == (float) 34512.5)) {
				cout << dummy << " != 34512.5 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
				uint a;
				std::cin >> a;
			}
		}

		solve(iniI, endI);
	}
}


void runCoordinator(igcl::Coordinator * coord)
{
	node = coord;
	coord->setLayout(GroupLayout::getAllToAllLayout(nParticipants));
	coord->start();
	coord->waitForNodes(nParticipants);
	work(coord);
	//coord->hang();
	coord->terminate();
}


void runPeer(igcl::Peer * peer)
{
	node = peer;
	peer->start();
	work(peer);
	peer->hang();
	//peer->terminate();
}


/*
15895.509
time elapsed: 9.393 sec

14: 3489.8
15: 3491.76

./thesis 1 44100 14 30 3
./thesis 0 12002 127.0.0.1 44100 14 30 3
*/
