
// Ocean simulator - square/rectangle chunk decomposition (ECE 506 only)
// Divides the interior grid into sqrt(T) x sqrt(T) chunks where T = num threads.
// Each thread gets one chunk — a square tile of the grid.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <omp.h>

void initGrid(const char * infile, int dim, int pad, double a[dim][(dim-2)+2*pad]);
void saveGrid(const char *outfile, int dim, int pad, double a[dim][(dim-2)+2*pad]);
void sim(int steps, int dim, int pad, double grid[2][dim][(dim-2)+2*pad]);

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("Arguments: <size> <nsteps> <infile> <outfile>\n");
        return 0;
    }
    const int dim = atoi(argv[1]);
    const int t   = atoi(argv[2]);
    printf("%d x %d array, %d time steps\n", dim, dim, t);
    int inner = dim - 2;
    if ((inner <= 0) || ((inner & (inner-1)) != 0))
        printf("size must be N+2, where N is a positive power of 2\n");
    if (t <= 0) printf("Number of time steps must be > 0\n");

    const int pad    = 8;
    const int colDim = inner + 2 * pad;

    double (*grid)[dim][colDim] = malloc(sizeof(double[2][dim][colDim]));
    struct timespec startTime, endTime;
    initGrid(argv[3], dim, pad, grid[0]);
    initGrid(argv[3], dim, pad, grid[1]);
    timespec_get(&startTime, TIME_UTC);
    sim(t, dim, pad, grid);
    timespec_get(&endTime, TIME_UTC);
    saveGrid(argv[4], dim, pad, grid[t%2]);
    long long seconds  = endTime.tv_sec - startTime.tv_sec;
    long nanoseconds   = endTime.tv_nsec - startTime.tv_nsec;
    double s = (double)seconds + (double)nanoseconds * 1e-9;
    printf("Time = %.9lf seconds\n", s);
    free(grid);
    return 0;
}

void initGrid(const char *infile, int dim, int pad, double a[dim][(dim-2)+2*pad]) {
    FILE *in = fopen(infile, "r");
    if (!in) { printf("Cannot open file: %s\n", infile); return; }
    int colStart = pad - 1;
    int colEnd   = colStart + dim;
    for (int row = 0; row < dim; row++) {
        for (int col = colStart; col < colEnd; col++) {
            double temp;
            if (fscanf(in, "%lf", &temp) != EOF) {
                a[row][col] = temp;
            } else {
                printf("File does not have enough data points: %s\n", infile);
                fclose(in); return;
            }
        }
    }
    fclose(in);
}

void saveGrid(const char *outfile, int dim, int pad, double a[dim][(dim-2)+2*pad]) {
    FILE *out = fopen(outfile, "w");
    if (!out) { printf("Cannot open output file: %s\n", outfile); return; }
    for (size_t row = 1; row < dim-1; row++) {
        for (size_t col = pad; col < pad+dim-2; col++) {
            fprintf(out, "%6.3lf  ", a[row][col]);
        }
        fprintf(out, "\n");
    }
    fclose(out);
}

void sim(int steps, int dim, int pad, double grid[2][dim][(dim-2)+2*pad]) {
    int inner = dim - 2;  // number of interior rows and columns
    int readLayer = 1, writeLayer = 0;

    for (int time = 0; time < steps; time++) {
        readLayer  ^= 1;
        writeLayer ^= 1;

        // Tile the 2D interior into square-ish chunks, one per thread.
        // Number of tiles = number of threads. Arrange as grid_t x grid_t tiles
        // where grid_t = sqrt(nthreads), falling back to rectangular if not perfect square.
        #pragma omp parallel
        {
            int nthreads = omp_get_num_threads();
            int tid      = omp_get_thread_num();

            // Find best sqrt decomposition: rows_t * cols_t = nthreads
            int rows_t = (int)round(sqrt((double)nthreads));
            while (nthreads % rows_t != 0) rows_t--;  // find largest divisor <= sqrt
            int cols_t = nthreads / rows_t;

            // Tile sizes (ceiling division so we cover all points)
            int tile_rows = (inner + rows_t - 1) / rows_t;
            int tile_cols = (inner + cols_t - 1) / cols_t;

            // Which tile does this thread own?
            int tile_r = tid / cols_t;
            int tile_c = tid % cols_t;

            // Row and column ranges for this thread (interior coords, 1-indexed)
            int row_start = 1 + tile_r * tile_rows;
            int row_end   = row_start + tile_rows;
            if (row_end > dim - 1) row_end = dim - 1;

            int col_start = pad + tile_c * tile_cols;
            int col_end   = col_start + tile_cols;
            if (col_end > pad + inner) col_end = pad + inner;

            // Each thread computes its tile
            for (int row = row_start; row < row_end; row++) {
                for (int col = col_start; col < col_end; col++) {
                    double n = grid[readLayer][row-1][col];
                    double s = grid[readLayer][row+1][col];
                    double e = grid[readLayer][row][col+1];
                    double w = grid[readLayer][row][col-1];
                    grid[writeLayer][row][col] =
                        (grid[readLayer][row][col] + n + s + e + w) / 5.0;
                }
            }
            // implicit barrier at end of parallel region — all tiles done before swap
        }
    }
}
