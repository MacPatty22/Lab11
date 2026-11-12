/*********************************************************************
* @file mandel.c
* @brief Renders a single Mandelbrot set image to a jpeg file
* @author Mac Patterson
* @date 11/11/2026
*********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "jpegrw.h"

// Number of entries in the color palette
#define PALETTE_SIZE 16

// local routines
static int iteration_to_color(int iters, int max);
static int iterations_at_point(double x, double y, int max);
static void compute_image(imgRawImage *img, double xmin, double xmax,
                          double ymin, double ymax, int max);
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

    // For each command line argument given,
    // override the appropriate configuration value.
    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:h")) != -1)
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

    // Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
    yscale = xscale / image_width * image_height;

    // Display the configuration of the image.
    printf("mandel: x=%lf y=%lf xscale=%lf yscale=%lf max=%d outfile=%s\n",
           xcenter, ycenter, xscale, yscale, max, outfile);

    // Create a raw image of the appropriate size.
    imgRawImage *img = initRawImage(image_width, image_height);
    if (img == NULL)
    {
        fprintf(stderr, "mandel: out of memory\n");
        return 1;
    }

    // Fill it with black
    setImageCOLOR(img, 0);

    // Compute the Mandelbrot image
    compute_image(img, xcenter - xscale / 2, xcenter + xscale / 2,
                  ycenter - yscale / 2, ycenter + yscale / 2, max);

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

static void compute_image(imgRawImage *img, double xmin, double xmax,
                          double ymin, double ymax, int max)
{
    int i;
    int j;

    int width = img->width;
    int height = img->height;

    // For every pixel in the image...
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            // Determine the point in x,y space for that pixel.
            double x = xmin + i * (xmax - xmin) / width;
            double y = ymin + j * (ymax - ymin) / height;

            // Compute the iterations at that point.
            int iters = iterations_at_point(x, y, max);

            // Set the pixel in the bitmap.
            setPixelCOLOR(img, i, j, iteration_to_color(iters, max));
        }
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
    printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
    printf("-x <coord>  X coordinate of image center point. (default=0)\n");
    printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
    printf("-s <scale>  Scale of the image in Mandelbrot coordinates (X-axis). (default=4)\n");
    printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
    printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
    printf("-o <file>   Set output file. (default=mandel.jpg)\n");
    printf("-h          Show this help text.\n");
    printf("\nSome examples are:\n");
    printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
    printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
    printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}