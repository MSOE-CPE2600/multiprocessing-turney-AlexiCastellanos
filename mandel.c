/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  
///


/***
* Modified by Alexi Castellaos
* Filename: mandel.c
* Date: 11/26/24
* Description: This program generates animated Mandelbrot set frames as JPEG images using parallel
*  processing with synchronized child processes and customizable parameters for center, scale, resolution, and iterations.
*
***/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "jpegrw.h"
#include <fcntl.h>  // for O_CREAT


#define NUM_FRAMES 50

//prototypes
static int iteration_to_color(int i, int max);
static int iterations_at_point(double x, double y, int max);
static void compute_image(imgRawImage *img, double xmin, double xmax,
                           double ymin, double ymax, int max);
static void show_help();

// Function to generate a single Mandelbrot frame and save it as a JPEG image
void generateMandelFrame(double x, double y, double scale, const char *outfile, int image_width, int image_height, int max)
{
    imgRawImage *img = initRawImage(image_width, image_height);
    setImageCOLOR(img, 0);
    compute_image(img, x - scale / 2, x + scale / 2, y - scale / 2, y + scale / 2, max);
    storeJpegImageFile(img, outfile);
    freeRawImage(img);
}

int main(int argc, char *argv[])
{
    char c;

    // These are the default configuration values used
    // if no command line arguments are given.
    double xcenter = 0;
    double ycenter = 0;
    double xscale = 4;
    int image_width = 1000;
    int image_height = 1000;
    int max = 1000;
    int num_children = 1; // default number of children
    char output_filename[256] = "mandel_frame"; // Default filename prefix

    // For each command line argument given,
    // override the appropriate configuration value.
    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:c:h")) != -1)
    {
        switch (c)
        {
            case 'x':
                xcenter = atof(optarg);
                break;
            case 'y':
                ycenter = atof(optarg);
                break;
            case 's':
                xscale = atof(optarg);
                break;
            case 'W':
                image_width = atoi(optarg);
                break;
            case 'H':
                image_height = atoi(optarg);
                break;
            case 'm':
                max = atoi(optarg);
                break;
            case 'o':
                strncpy(output_filename, optarg, sizeof(output_filename) - 1);
                output_filename[sizeof(output_filename) - 1] = '\0'; // Ensure null-termination
                break;
            case 'c':
                num_children = atoi(optarg);
                break;
            case 'h':
                show_help();
                exit(1);
                break;
            default:
                fprintf(stderr, "Unknown option: -%c\n", c);
                show_help();
                return 1;
        }
    }

    printf("Generating Mandel movie with %d images using %d children...\n", NUM_FRAMES, num_children);

    // Calculate frames assigned to each child process
    int frames_per_child = NUM_FRAMES / num_children;
    int remaining_frames = NUM_FRAMES % num_children;

    // Create semaphores to enforce order
    sem_t *semaphores[num_children];
    for (int i = 0; i < num_children; i++) {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/child_sem_%d", i);
        semaphores[i] = sem_open(sem_name, O_CREAT, 0644, (i == 0) ? 1 : 0);
    }

    for (int child = 0; child < num_children; child++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }

        if (pid == 0) {
            // Child process code

            // Wait for its turn
            sem_wait(semaphores[child]);

            // Determine the frames this child will generate
            int start_frame = child * frames_per_child + (child < remaining_frames ? child : remaining_frames) + 1;
            int end_frame = start_frame + frames_per_child - 1;

            if (child < remaining_frames) {
                end_frame++;
            }

            // Print assigned frames
            printf("Child %d assigned frames from %d to %d\n", child, start_frame, end_frame);

            // Generate frames for this child process
            for (int frame = start_frame; frame <= end_frame; frame++) {
                double scale = xscale / (1 + frame * 0.1);
                char frame_outfile[300];
                snprintf(frame_outfile, sizeof(frame_outfile), "%s_%d.jpg", output_filename, frame);

                generateMandelFrame(xcenter, ycenter, scale, frame_outfile, image_width, image_height, max);

                printf("Child %d generated frame %d\n", child, frame);
            }

            // Signal the next child, if it exists
            if (child + 1 < num_children) {
                sem_post(semaphores[child + 1]);
            }

            // Close the semaphore
            sem_close(semaphores[child]);
            exit(0);
        }
    }

    // Parent process waiting for all child processes to finish
    while (wait(NULL) > 0) {}

    // Clean up semaphores
    for (int i = 0; i < num_children; i++) {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "/child_sem_%d", i);
        sem_unlink(sem_name);
    }

    printf("All images generated\n");

    return 0;
}


    // to collect runtime 
//    clock_t end_time = clock();
//    double elapsed_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
//   printf("Runtime for %d children: %.4f seconds\n", num_children, elapsed_time);



/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max )
{
	int i,j;

	int width = img->width;
	int height = img->height;

	// For every pixel in the image...

	for(j=0;j<height;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
		}
	}
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
