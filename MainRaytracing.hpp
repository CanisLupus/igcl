#include "igcl/igcl.hpp"

#include "raytracing/common.hpp"
#include "raytracing/constants.hpp"
#include "raytracing/structs.hpp"
#include "raytracing/operators.hpp"
#include "raytracing/raytracer.hpp"
#include "raytracing/bmp.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

using namespace std;

#define TEST_READY

typedef pair<int, int> Job;

static const uint blockSize = 10000;
int bufferingLevel = 10;
int nTests = 30;
int nParticipants = 4;

void setSize  (int val) { bufferingLevel = val; }
void setNTests(int val) { nTests = val; }
void setNNodes(int val) { nParticipants = val; }


igcl::Coordinator * coord;
igcl::NBuffering * buffering;

BmpImage * image;
uint imageSize;
Raytracer * raytracer;

std::thread * th;
std::queue<uint> threadJobs;

std::mutex threadJobsAccessMutex;
std::condition_variable threadJobsAccessCondVar;
std::mutex bufferingAccessMutex;

uint countJobs;

void runThread()
{
	while (1)
	{
		std::unique_lock<std::mutex> threadQueueLock(threadJobsAccessMutex);

		while (threadJobs.empty()) {
			threadJobsAccessCondVar.wait(threadQueueLock);
		}
		uint startIndex = threadJobs.front();
		threadJobs.pop();

		threadQueueLock.unlock();

		uint nIndexes = std::min(blockSize, imageSize-startIndex);
		uint endIndex = startIndex + nIndexes;
		countJobs++;
		for (uint i=startIndex; i<endIndex; ++i) {
			int row = i / IMAGE_WIDTH;
			int col = i % IMAGE_WIDTH;
			const color_s c = raytracer->castRayFromScreen(row, col);	// calculate color of pixel (CPU-HEAVY)
			image->setPixel(row, col, c.r, c.g, c.b);
		}

		std::unique_lock<std::mutex> bufferLock(bufferingAccessMutex);
		buffering->completeJob(0);
		buffering->bufferTo(0);
		if (buffering->allJobsCompleted()) {
			coord->sendToSelf((char) 0);
		}
		bufferLock.unlock();
	}
}


void sendJob(igcl::peer_id id, uint sendIndex) {
	//cout << "sendJob() to " << id << " index: " << sendIndex << endl;
	if (id == 0) {
		std::lock_guard<std::mutex> threadQueueLock(threadJobsAccessMutex);
		threadJobs.push(sendIndex);
		threadJobsAccessCondVar.notify_one();
	} else {
		coord->sendTo(id, sendIndex);
	}
}


void receiveResult() {
	//cout << "receiveResult()" << endl;
	igcl::peer_id sourceId;
	color_s * buffer=NULL; uint size;

	coord->waitRecvNewFromAny(sourceId, buffer, size);
	//cout << "got result" << endl;

	if (sourceId == 0) {	// was a coordinator self-send
		//cout << "was a coordinator self-send" << endl;
		free(buffer);
		return;
	}

	std::unique_lock<std::mutex> bufferLock(bufferingAccessMutex);
	uint indexIni = buffering->completeJob(sourceId);
	bufferLock.unlock();
	uint indexEnd = indexIni+size;

	for (uint i = indexIni; i < indexEnd; ++i) {
		const color_s & c = buffer[i-indexIni];
		image->setPixel(i/IMAGE_WIDTH, i%IMAGE_WIDTH, c.r, c.g, c.b);
	}

	free(buffer);
	bufferLock.lock();
	buffering->bufferTo(sourceId);
	bufferLock.unlock();
}


void start(timeval & iniTime)
{
	//printf("----- INFO -----------------------------------\n");
	//printf("Image generated in [%d x %d] resolution.\n", IMAGE_WIDTH, IMAGE_HEIGHT);
	//printf("----------------------------------------------\n");

	gettimeofday(&iniTime, NULL);
}


void finish(const timeval & iniTime) {
	timeval endTime;
	gettimeofday(&endTime, NULL);
	long t = timeDiff(iniTime, endTime);
	cout << "image; time: " << t << " ms";

	char filename[100];
	sprintf(filename, "test%ld.bmp", t);
	//image->writeBmpFile(filename);		// save image to file
	//printf("; saved as \"%s\"", filename);
	printf("\n");

	const color_s c = {0.0, 1.0, 0.0};
	for (uint i = 0; i < imageSize; ++i) {
		image->setPixel(i/IMAGE_WIDTH, i%IMAGE_WIDTH, c.r, c.g, c.b);
	}
	//delete image;
}


void runCoordinator(igcl::Coordinator * coord)
{
	Raytracer raytracer;
	raytracer.setupScene();	// setup scene objects
	::raytracer = &raytracer;

	::coord = coord;
	coord->start();
	coord->waitForNodes(nParticipants);

	th = new std::thread(runThread);
	th->detach();

	image = new BmpImage(IMAGE_WIDTH, IMAGE_HEIGHT);
	imageSize = IMAGE_HEIGHT*IMAGE_WIDTH;

for (int i=0; i<nTests; i++) {
	countJobs = 0;
	std::unique_lock<std::mutex> bufferLock(bufferingAccessMutex);
	igcl::NBuffering buf(bufferingLevel, imageSize, blockSize, sendJob);
	buffering = &buf;
	buffering->addPeers(coord->downstreamPeers());
	buffering->addPeer(0);
	bufferLock.unlock();

	timeval iniTime;
	start(iniTime);

	bufferLock.lock();
	buffering->bufferToAll();
	bufferLock.unlock();

	while (1) {
		//cout << "lock buffering->allJobsCompleted()" << endl;
		bufferLock.lock();
		//cout << buffering->allJobsCompleted() << " " << buffering->getNCompletedJobs() << endl;
		if (buffering->allJobsCompleted()) {
			bufferLock.unlock();
			break;
		}
		bufferLock.unlock();
		receiveResult();
	}
	cout << "count: " << countJobs << endl;

	finish(iniTime);
}

	coord->terminate();
}


void runPeer(igcl::Peer * peer)
{
	peer->start();

	Raytracer raytracer;
	raytracer.setupScene();	// setup scene objects

	timeval globalIni, end;
	gettimeofday(&globalIni, NULL);

	while (1)
	{
		uint startIndex = 0;
		igcl::result_type res = peer->waitRecvFrom(0, startIndex);
		if (res != igcl::SUCCESS)
			break;
		//std::cout << "received " << startIndex << endl;

		uint nIndexes = std::min(blockSize, imageSize-startIndex);
		uint endIndex = startIndex + nIndexes;

		color_s array[nIndexes];

		for (uint i=startIndex; i<endIndex; ++i) {
			int row = i / IMAGE_WIDTH;
			int col = i % IMAGE_WIDTH;
			array[i-startIndex] = raytracer.castRayFromScreen(row, col);		// calculate color of pixel (CPU-HEAVY)
		}

		peer->sendTo(0, array+0, nIndexes);
	}

	gettimeofday(&end, NULL);
	cout << "total time:\t" << timeDiff(globalIni, end) << " ms\n";

	peer->hang();
}
