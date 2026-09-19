
// A simplified simulator of ocean surface temperatures
// Given an N+2 x N+2 grid, where N is a power of 2, compute the value of the interior points
// by averaging with NEWS neighbors.  Perimeter points contribute values but are not updated.

// Program arguments:
// (1) dimensions (total number of rows and columns, including border points),
//     must be N+2 where N is a power of 2
// (2) time steps, must be an integer
// (3) name of file for initial data
// (4) name of file for output data, must only contain interior points (not the border)
// Files are expected to be properly sized. For your own sanity, do something sensible if the
// input file does not match the matrix dimensions or if either file cannot be opened.
// (We will not test this.)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

void initGrid(const char * infile, int dim, int pad, double a[dim][(dim-2)+2*pad]);
void saveGrid(const char *outfile, int dim, int pad, double a[dim][(dim-2)+2*pad]);
void sim(int steps, int dim, int pad, double grid[2][dim][(dim-2)+2*pad]);

int main(int argc, char **argv) {
    // arguments are x-dimension, y-dimension, and number of time steps
    if (argc != 5) {
        printf("Arguments: <size> <nsteps> <infile> <outfile>\n");
        return 0;
    }
    const int dim = atoi(argv[1]); // NOLINT(*-err34-c), avoid compiler warning
    const int t = atoi(argv[2]); // NOLINT(*-err34-c)
    printf("%d x %d array, %d time steps\n", dim, dim, t);
    int inner = dim-2;
    if ((inner <= 0) || ((inner & (inner-1)) != 0)) printf("size must be N+2, where N is a positive power of 2\n");
    if (t <= 0) printf("Number of time steps must be > 0\n");
    const int pad = 8;  // possible size of L1 cache block
    const int colDim = inner + 2 * pad;

    // allocate grid array
    double (*grid)[dim][colDim] = malloc(sizeof (double[2][dim][colDim]));
    // initialize grid points
    struct timespec startTime, endTime;
    initGrid(argv[3], dim, pad, grid[0]);
    initGrid(argv[3], dim, pad, grid[1]);
    timespec_get(&startTime, TIME_UTC);
    sim(t, dim, pad, grid);
    timespec_get(&endTime, TIME_UTC);
    saveGrid(argv[4], dim, pad, grid[t%2]);
    long long seconds = endTime.tv_sec - startTime.tv_sec;
    long nanoseconds = endTime.tv_nsec - startTime.tv_nsec;
    double s = (double) seconds + (double) nanoseconds * 1e-9;
    printf("Time = %.9lf seconds\n", s);
    free(grid);   // to avoid warning about a memory leak
    return 0;
}

void initGrid(const char *infile, int dim, int pad, double a[dim][(dim-2) + 2*pad]) {
    // grid includes boundaries: one extra row on top and bottom, pad extra columns on left and right
    // pad is supposed to improve cache block alignment
    FILE *in = fopen(infile, "r");
    if (!in) {
        printf("Cannot open file: %s\n", infile);
    }
    int colStart = pad-1;
    int colEnd = colStart + dim;
    for (int row = 0; row < dim; row++) {
        for (int col = colStart; col < colEnd; col++) {
            double temp;  // read one value from the file
            if (fscanf(in, "%lf", &temp) != EOF) {
                a[row][col] = temp;
            }
            else {
                printf("File does not have enough data points: %s\n", infile);
                fclose(in);
                return;
            }
        }
    }
    fclose(in);
}

void saveGrid(const char *outfile, int dim, int pad, double a[dim][(dim-2)+2*pad]) {
    FILE * out = fopen(outfile, "w");
    if (!out) {
        printf("Cannot open output file: %s\n", outfile);
        return;
    }
    //int count = 0;
    // skip first and last row/column (don't save boundary points)
    for (size_t row = 1; row < dim-1; row++) {
        for (size_t col = pad; col < pad+dim-2; col++) {
            // printing three decimal places
            // data values expected to be between 0.0 and 30.0,
            // but this code still works if the values are outside that range
            fprintf(out, "%6.3lf  ", a[row][col]);
        }
        fprintf(out, "\n");
    }
    //printf("\n%d\n", count);
    fclose(out);
}

void sim(int steps, int dim, int pad, double grid[2][dim][(dim-2)+2*pad]) {
    int readLayer = 1, writeLayer = 0;
    for (int time = 0; time < steps; time++) {
        readLayer ^= 1;   // alternate read layers, starting with 0
        writeLayer ^= 1;  // alternate write layers, starting with 1
#pragma omp parallel for schedule(static)
        for (int row = 1; row < dim-1; row++) {
            for (int col = pad; col < pad+dim-2; col++) {
                double n = grid[readLayer][row-1][col];
                double s = grid[readLayer][row+1][col];
                double e = grid[readLayer][row][col+1];
                double w = grid[readLayer][row][col-1];
                grid[writeLayer][row][col] = (grid[readLayer][row][col] + n + s + e + w) / 5.0;
            }
        }
    }
}