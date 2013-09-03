#include "raytracing/common.hpp"
#include "raytracing/constants.hpp"
#include "raytracing/structs.hpp"
#include "raytracing/operators.hpp"
#include "raytracing/raytracer.hpp"
#include "raytracing/bmp.hpp"

#include <algorithm>

using namespace std;

#define N_THREADS 2

BmpImage * image;
uint imageSize;


void start(timeval & iniTime)
{
	image = new BmpImage(IMAGE_WIDTH, IMAGE_HEIGHT);
	imageSize = IMAGE_HEIGHT*IMAGE_WIDTH;

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

	delete image;
}


void runCoordinator(igcl::Coordinator * node)
{
	const uint blockSize = 10000;
	Raytracer raytracer;
	raytracer.setupScene();	// setup scene objects

	for (int t=0; t<30; t++) {
		timeval iniTime;
		start(iniTime);

		//#pragma omp parallel for num_threads(N_THREADS)
		#pragma omp parallel for schedule(dynamic, 10000) num_threads(N_THREADS)
		for (uint i=0; i<imageSize; i++)
		{
			int row = i / IMAGE_WIDTH;
			int col = i % IMAGE_WIDTH;
			color_s c = raytracer.castRayFromScreen(row, col);	// calculate color of pixel (CPU-HEAVY)
			image->setPixel(row, col, c.r, c.g, c.b);
		}

		finish(iniTime);
	}
}


void runPeer(igcl::Peer * peer)
{

}
