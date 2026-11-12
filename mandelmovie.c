/*********************************************************************
* @file mandelmovie.c
* @brief Generates a series of Mandelbrot frames in parallel by forking child processes that each exec the mandel program
* @author Mac Patterson
* @date 11/11/2026
* @note CPE-2600 Lab 11 Multiprocessing, Section XXX
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// Path to the mandel executable each child runs
#define MANDEL_PATH "./mandel"

// Size of the char buffers used to turn numbers into argv strings
#define ARG_BUF_SIZE 64

// Default settings. The default point is in "Seahorse Valley", which
// stays interesting all the way down the zoom.
#define DEFAULT_PROCS   1
#define DEFAULT_FRAMES  50
#define DEFAULT_X       -0.743643887037151
#define DEFAULT_Y       0.131825904205330
#define DEFAULT_SCALE   4.0
#define DEFAULT_ZOOM    0.82
#define DEFAULT_WIDTH   1000
#define DEFAULT_HEIGHT  1000
#define DEFAULT_MAX     1000
#define DEFAULT_PREFIX  "mandel"

// local routines
static void show_help(void);
static int reap_one_child(void);
static pid_t launch_frame(int frame, double xcenter, double ycenter,
                          double scale, int width, int height, int max,
                          const char *prefix);


int main(int argc, char *argv[])
{
    // getopt returns an int; storing it in a char breaks the -1 check
    // on platforms where char is unsigned
    int c;

    int nprocs = DEFAULT_PROCS;
    int nframes = DEFAULT_FRAMES;
    double xcenter = DEFAULT_X;
    double ycenter = DEFAULT_Y;
    double scale = DEFAULT_SCALE;
    double zoom = DEFAULT_ZOOM;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int max = DEFAULT_MAX;
    const char *prefix = DEFAULT_PREFIX;

    while ((c = getopt(argc, argv, "p:n:x:y:s:z:W:H:m:o:h")) != -1)
    {
        switch (c)
        {
            case 'p':
                nprocs = atoi(optarg);
                break;
            case 'n':
                nframes = atoi(optarg);
                break;
            case 'x':
                xcenter = atof(optarg);
                break;
            case 'y':
                ycenter = atof(optarg);
                break;
            case 's':
                scale = atof(optarg);
                break;
            case 'z':
                zoom = atof(optarg);
                break;
            case 'W':
                width = atoi(optarg);
                break;
            case 'H':
                height = atoi(optarg);
                break;
            case 'm':
                max = atoi(optarg);
                break;
            case 'o':
                prefix = optarg;
                break;
            case 'h':
                show_help();
                return 0;
            default:
                // getopt already printed what was wrong
                show_help();
                return 1;
        }
    }

    // Sanity check everything before forking anything
    if (nframes < 1)
    {
        fprintf(stderr, "mandelmovie: number of frames must be at least 1\n");
        return 1;
    }
    if (nprocs < 1)
    {
        fprintf(stderr, "mandelmovie: number of processes must be at least 1\n");
        return 1;
    }
    if (nprocs > nframes)
    {
        // More children than frames is pointless, so clamp it
        fprintf(stderr, "mandelmovie: %d processes requested but only %d frames, using %d\n",
                nprocs, nframes, nframes);
        nprocs = nframes;
    }
    if (scale <= 0.0 || zoom <= 0.0 || width < 1 || height < 1 || max < 1)
    {
        fprintf(stderr, "mandelmovie: scale, zoom, width, height and max must all be positive\n");
        return 1;
    }

    printf("mandelmovie: %d frames, %d processes, center=(%.15g, %.15g), start scale=%g, zoom=%g\n",
           nframes, nprocs, xcenter, ycenter, scale, zoom);

    int running = 0;     // children currently alive
    int failures = 0;    // children that did not exit cleanly
    double frame_scale = scale;

    for (int frame = 0; frame < nframes; frame++)
    {
        // If every slot is busy, block until one child finishes
        if (running >= nprocs)
        {
            failures += reap_one_child();
            running--;
        }

        pid_t pid = launch_frame(frame, xcenter, ycenter, frame_scale,
                                 width, height, max, prefix);
        if (pid < 0)
        {
            // fork failed; stop launching but still wait for the
            // children that are already running
            failures += nframes - frame;
            break;
        }
        running++;

        // Each frame is a fixed fraction of the size of the previous one,
        // which makes the zoom look smooth instead of speeding up
        frame_scale *= zoom;
    }

    // Wait for every child that is still working
    while (running > 0)
    {
        failures += reap_one_child();
        running--;
    }

    if (failures > 0)
    {
        fprintf(stderr, "mandelmovie: %d frame(s) failed\n", failures);
        return 1;
    }

    printf("mandelmovie: done, frames written as %s0.jpg through %s%d.jpg\n",
           prefix, prefix, nframes - 1);
    return 0;
}


static pid_t launch_frame(int frame, double xcenter, double ycenter,
                          double scale, int width, int height, int max,
                          const char *prefix)
{
    // Build all of the argument strings before forking so the child
    // has nothing to do but exec
    char xbuf[ARG_BUF_SIZE];
    char ybuf[ARG_BUF_SIZE];
    char sbuf[ARG_BUF_SIZE];
    char wbuf[ARG_BUF_SIZE];
    char hbuf[ARG_BUF_SIZE];
    char mbuf[ARG_BUF_SIZE];
    char outbuf[256];

    // %.17g keeps full double precision, which matters once the
    // scale gets tiny deep in the zoom
    snprintf(xbuf, sizeof(xbuf), "%.17g", xcenter);
    snprintf(ybuf, sizeof(ybuf), "%.17g", ycenter);
    snprintf(sbuf, sizeof(sbuf), "%.17g", scale);
    snprintf(wbuf, sizeof(wbuf), "%d", width);
    snprintf(hbuf, sizeof(hbuf), "%d", height);
    snprintf(mbuf, sizeof(mbuf), "%d", max);
    snprintf(outbuf, sizeof(outbuf), "%s%d.jpg", prefix, frame);

    // Flush so buffered parent output is not duplicated into the child
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("mandelmovie: fork");
        return -1;
    }

    if (pid == 0)
    {
        // Child: replace this process with mandel
        execl(MANDEL_PATH, "mandel",
              "-x", xbuf,
              "-y", ybuf,
              "-s", sbuf,
              "-W", wbuf,
              "-H", hbuf,
              "-m", mbuf,
              "-o", outbuf,
              (char *)NULL);

        // execl only returns on failure
        perror("mandelmovie: execl " MANDEL_PATH);
        _exit(127);
    }

    // Parent
    return pid;
}

static int reap_one_child(void)
{
    int status;
    pid_t pid;

    // Retry if a signal interrupts the wait
    do
    {
        pid = wait(&status);
    } while (pid < 0 && errno == EINTR);

    if (pid < 0)
    {
        perror("mandelmovie: wait");
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        return 0;
    }

    fprintf(stderr, "mandelmovie: child %d did not finish cleanly\n", (int)pid);
    return 1;
}

static void show_help(void)
{
    printf("Use: mandelmovie [options]\n");
    printf("Where options are:\n");
    printf("-p <procs>   Number of child processes to run at once. (default=%d)\n", DEFAULT_PROCS);
    printf("-n <frames>  Number of frames to generate. (default=%d)\n", DEFAULT_FRAMES);
    printf("-x <coord>   X coordinate of the zoom center. (default=%.15g)\n", DEFAULT_X);
    printf("-y <coord>   Y coordinate of the zoom center. (default=%.15g)\n", DEFAULT_Y);
    printf("-s <scale>   Starting scale in Mandelbrot coordinates. (default=%g)\n", DEFAULT_SCALE);
    printf("-z <factor>  Scale multiplier per frame, < 1 zooms in. (default=%g)\n", DEFAULT_ZOOM);
    printf("-W <pixels>  Width of each frame in pixels. (default=%d)\n", DEFAULT_WIDTH);
    printf("-H <pixels>  Height of each frame in pixels. (default=%d)\n", DEFAULT_HEIGHT);
    printf("-m <max>     Maximum iterations per point. (default=%d)\n", DEFAULT_MAX);
    printf("-o <prefix>  Output file prefix, frames are <prefix>N.jpg. (default=%s)\n", DEFAULT_PREFIX);
    printf("-h           Show this help text.\n");
    printf("\nExamples:\n");
    printf("mandelmovie -p 4\n");
    printf("mandelmovie -p 10 -n 100 -z 0.9\n");
    printf("mandelmovie -p 2 -x -0.5 -y 0 -s 3 -z 0.85\n\n");
}