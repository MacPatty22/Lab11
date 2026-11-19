/*********************************************************************
* @file mandel.c
* @brief Renders a single Mandelbrot set image to a jpeg file.
* @author Mac Patterson
* @date 11/19/2026
*********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "jpegrw.h"

// Number of entries in the color palette
#define PALETTE_SIZE 16

// Allowed range for the -t thread count
#define MIN_THREADS 1
#define MAX_THREADS 20

typedef struct
{
    imgRawImage *img;   // shared output image; each thread writes only its own rows
    double xmin;        // left edge of the whole image in the complex plane
    double xmax;        // right edge of the whole image
    double ymin;        // bottom edge of the whole image
    double ymax;        // top edge of the whole image
    int max;            // max iterations per point
    int row_start;      // first row this thread computes (inclusive)
    int row_end;        // row this thread stops at (exclusive)
} thread_args_t;

// local routines
static int iteration_to_color(int iters, int max);
static int iterations_at_point(double x, double y, int max);
static void *compute_region(void *arg);
static void compute_image(imgRawImage *img, double xmin, double xmax,
                          double ymin, double ymax, int max, int num_threads);
static void show_help(void);

int main(int argc, char *argv[])
{
    // getopt returns an int; a char breaks the -1 check on some platforms
    int c;

    // These are the default configuration values used
    // if no command line arguments are given.
    const char *outfile = "mandel.jpg";
    double xcenter = 0;
    double ycenter = 0;
    double xscale = 4;
    double yscale = 0; // calc later
    int image_width = 1000;
    int image_height = 1000;
    int max = 1000;
    int num_threads = 1; // Lab 12: threads used to compute the image

    // For each command line argument given,
    // override the appropriate configuration value.
    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:t:h")) != -1)
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
                outfile = optarg;
                break;
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'h':
                show_help();
                exit(1);
                break;
            default:
                show_help();
                exit(1);
                break;
        }
    }

    if (image_width < 1 || image_height < 1 || max < 1)
    {
        fprintf(stderr, "mandel: width, height and max must be positive\n");
        return 1;
    }

    if (num_threads < MIN_THREADS || num_threads > MAX_THREADS)
    {
        fprintf(stderr, "mandel: thread count must be between %d and %d\n",
                MIN_THREADS, MAX_THREADS);
        return 1;
    }

    // Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
    yscale = xscale / image_width * image_height;

    // Display the configuration of the image.
    printf("mandel: x=%lf y=%lf xscale=%lf yscale=%lf max=%d threads=%d outfile=%s\n",
           xcenter, ycenter, xscale, yscale, max, num_threads, outfile);

    // Create a raw image of the appropriate size.
    imgRawImage *img = initRawImage(image_width, image_height);
    if (img == NULL)
    {
        fprintf(stderr, "mandel: out of memory\n");
        return 1;
    }

    // Fill it with black
    setImageCOLOR(img, 0);

    // Compute the Mandelbrot image, split across num_threads threads
    compute_image(img, xcenter - xscale / 2, xcenter + xscale / 2,
                  ycenter - yscale / 2, ycenter + yscale / 2, max, num_threads);

    // Save the image in the stated file.
    int result = storeJpegImageFile(img, outfile);
    if (result != 0)
    {
        fprintf(stderr, "mandel: could not write %s\n", outfile);
    }

    // free the mallocs
    freeRawImage(img);

    return (result == 0) ? 0 : 1;
}

static int iterations_at_point(double x, double y, int max)
{
    double x0 = x;
    double y0 = y;
    int iter = 0;

    while ((x * x + y * y <= 4) && iter < max)
    {
        double xt = x * x - y * y + x0;
        double yt = 2 * x * y + y0;
        x = xt;
        y = yt;
        iter++;
    }
    return iter;
}

/**
 * Thread entry point. Computes every pixel in rows [row_start, row_end).
 * No locking is needed: each thread writes to a different set of rows,
 * and everything else it touches is read-only.
 */
static void *compute_region(void *arg)
{
    thread_args_t *a = (thread_args_t *)arg;

    int width = a->img->width;
    int height = a->img->height;

    // For every pixel in this thread's band...
    for (int j = a->row_start; j < a->row_end; j++)
    {
        for (int i = 0; i < width; i++)
        {
            // Determine the point in x,y space for that pixel.
            double x = a->xmin + i * (a->xmax - a->xmin) / width;
            double y = a->ymin + j * (a->ymax - a->ymin) / height;

            // Compute the iterations at that point.
            int iters = iterations_at_point(x, y, a->max);

            // Set the pixel in the bitmap.
            setPixelCOLOR(a->img, i, j, iteration_to_color(iters, a->max));
        }
    }

    return NULL;
}

/**
 * Splits the image into num_threads horizontal bands of rows, starts one
 * thread per band, and joins every thread before returning so the image
 * is complete when it gets saved.
 */
static void compute_image(imgRawImage *img, double xmin, double xmax,
                          double ymin, double ymax, int max, int num_threads)
{
    pthread_t threads[MAX_THREADS];
    thread_args_t args[MAX_THREADS]; // separate struct per thread so none share args

    int height = img->height;
    int rows_per_thread = height / num_threads;
    int leftover = height % num_threads; // extra rows go one each to the first threads
    int next_row = 0;

    // Fill in each thread's arguments and start it.
    for (int k = 0; k < num_threads; k++)
    {
        int rows = rows_per_thread + (k < leftover ? 1 : 0);

        args[k].img = img;
        args[k].xmin = xmin;
        args[k].xmax = xmax;
        args[k].ymin = ymin;
        args[k].ymax = ymax;
        args[k].max = max;
        args[k].row_start = next_row;
        args[k].row_end = next_row + rows;
        next_row += rows;

        int rc = pthread_create(&threads[k], NULL, compute_region, &args[k]);
        if (rc != 0)
        {
            fprintf(stderr, "mandel: pthread_create failed (%s)\n", strerror(rc));
            exit(1);
        }
    }

    // Wait for every thread to finish.
    for (int k = 0; k < num_threads; k++)
    {
        pthread_join(threads[k], NULL);
    }
}

static int iteration_to_color(int iters, int max)
{
    // Blue to white to orange to brown palette
    static const int palette[PALETTE_SIZE] =
    {
        0x421E0F, 0x19071A, 0x09012F, 0x040449,
        0x000764, 0x0C2C8A, 0x1852B1, 0x397DD1,
        0x86B5E5, 0xD3ECF8, 0xF1E9BF, 0xF8C95F,
        0xFFAA00, 0xCC8000, 0x995700, 0x6A3403
    };

    if (iters >= max)
    {
        return 0x000000;
    }
    return palette[iters % PALETTE_SIZE];
}

//prints usage info
static void show_help(void)
{
    printf("Use: mandel [options]\n");
    printf("Where options are:\n");
    printf("-m <max>     The maximum number of iterations per point. (default=1000)\n");
    printf("-x <coord>   X coordinate of image center point. (default=0)\n");
    printf("-y <coord>   Y coordinate of image center point. (default=0)\n");
    printf("-s <scale>   Scale of the image in Mandelbrot coordinates (X-axis). (default=4)\n");
    printf("-W <pixels>  Width of the image in pixels. (default=1000)\n");
    printf("-H <pixels>  Height of the image in pixels. (default=1000)\n");
    printf("-o <file>    Set output file. (default=mandel.jpg)\n");
    printf("-t <threads> Number of threads used to compute the image, %d-%d. (default=1)\n",
           MIN_THREADS, MAX_THREADS);
    printf("-h           Show this help text.\n");
    printf("\nSome examples are:\n");
    printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
    printf("mandel -x -.38 -y -.665 -s .05 -m 100 -t 4\n");
    printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000 -t 8\n\n");
}
