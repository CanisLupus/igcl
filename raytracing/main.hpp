#include "common.hpp"
#include "constants.hpp"
#include "structs.hpp"
#include "operators.hpp"
#include "raytracer.hpp"
#include "bmp.hpp"

using namespace std;


//Function executed concurrently by threads. Casts a ray from each pixel.
void threadedRaytracing(Raytracer & raytracer, BmpImage & image)
{
	int limit = IMAGE_HEIGHT*IMAGE_WIDTH;
	
	#pragma omp parallel for
	for (int pixel = 0; pixel < limit; pixel++)
	{
		int row = pixel / IMAGE_WIDTH;
		int col = pixel % IMAGE_WIDTH;
		color_s color = raytracer.castRayFromScreen(row, col);		// calculate color of pixel (CPU-HEAVY)
		image.setPixel(row, col, color.r, color.g, color.b);		// place color in the RGB image's pixel
	}
}


// Main function. Initializes, starts and manages ray-tracer threads; saves images; prints information.
int main_(int argc, char** argv)
{
	// ------------------ preliminary setup
	
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	// ------------------ set number of threads to use
	
	// disable dynamic adjustment of the number of threads (i.e.: always use starting number)
	if (omp_get_dynamic())
		omp_set_dynamic(0);

	// override number of threads if given
	if (N_THREADS != 0)
		omp_set_num_threads(N_THREADS);

	// ------------------ allocate data structures

	BmpImage image(IMAGE_WIDTH, IMAGE_HEIGHT);		// prepare RGB image (will be reused for each generated image)
	Raytracer raytracer;
	
	// ------------------ main loop that generates and saves images

	// counting time
	time_t ini, end;
	double timediff, useful_time = 0;
	struct tm * tm;

	char filename[50];
	char executePath[50];
	
	printf("----- INFO -----------------------------------\n");
	printf("Images generated in [%d x %d] resolution.\n", IMAGE_WIDTH, IMAGE_HEIGHT);
	printf("----------------------------------------------\n");

	raytracer.setupScene();	// setup scene objects
	
	for (int currentImg=1; currentImg <= (BENCHMARK ? NUM_IMAGES : 1); currentImg++)
	{
		// -------------- preparations

		raytracer.stepScene();	// setup next frame of scene (in case the scene changes over time)
		
		printf("image #%03d", currentImg);
		ini = clock();
		
		// -------------- image generation (massive work)
		
		threadedRaytracing(raytracer, image);
		
		// -------------- printing and saving

		end = clock();
		timediff = (end - ini) / (double) CLOCKS_PER_SEC;
		useful_time += timediff;
		printf("; %.3fs", timediff);
		
		if (!BENCHMARK)
		{
			time(&end);
			tm = localtime(&end);
			sprintf(filename, "images/%04d-%02d-%02d %02dh%02dm%02ds (%03d) %.3f.bmp",
					tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, currentImg, timediff);
			image.writeBmpFile(filename);		// save image to file
			printf("; saved as \"%s\"\n", filename);
			
			sprintf(executePath, "\"%s\"", filename);
			//system(executePath);		// show image using the associated system application
		}
		else
		{
			printf("\n");
		}
	}
	
	if (BENCHMARK)
	{
		printf("TOTAL TIME: %.3fs\nMEAN TIME: %.3fs", useful_time, useful_time / NUM_IMAGES);
	}
	
	return 0;
}
