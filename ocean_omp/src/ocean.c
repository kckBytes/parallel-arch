// A simplified simulator of ocean surface temperatures
// Given an N+2 x N+2 grid, where N is a power of 2, compute the value of the interior points
// by averaging with North, East, West, and South (NEWS) neighbors.
// Perimeter points contribute values but are not updated.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

void initGrid(const char *infile, int dim, double a[dim][dim]);
void saveGrid(const char *outfile, int dim, double a[dim][dim]);

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("Arguments: <dimension> <nsteps> <infile> <outfile>\n");
        return 0;
    }
    const int dim = atoi(argv[1]); // NOLINT(*-err34-c)
    const int t   = atoi(argv[2]); // NOLINT(*-err34-c)
    printf("%d x %d array, %d time steps\n", dim, dim, t);
    int check = dim - 2;
    if ((check <= 0) || ((check & (check - 1)) != 0))
        printf("dim must be N+2, where N is a positive power of 2\n");
    if (t <= 0) printf("Number of time steps must be > 0\n");

    // Two grids for double-buffering (read from cur, write to nxt, then swap)
    double (*cur)[dim] = malloc(sizeof(double[dim][dim]));
    double (*nxt)[dim] = malloc(sizeof(double[dim][dim]));
    if (!cur || !nxt) { printf("Memory allocation failed\n"); return 1; }

    initGrid(argv[3], dim, cur);

    // Copy boundary rows/cols into nxt so they are always correct after swaps
    for (int i = 0; i < dim; i++) {
        nxt[0][i]       = cur[0][i];
        nxt[dim-1][i]   = cur[dim-1][i];
        nxt[i][0]       = cur[i][0];
        nxt[i][dim-1]   = cur[i][dim-1];
    }

    struct timespec startTime, endTime;
    timespec_get(&startTime, TIME_UTC);

    for (int step = 0; step < t; step++) {
        #pragma omp parallel for schedule(static) collapse(2)
        for (int row = 1; row < dim - 1; row++) {
            for (int col = 1; col < dim - 1; col++) {
                nxt[row][col] = (cur[row][col] +
                                 cur[row-1][col] +
                                 cur[row+1][col] +
                                 cur[row][col-1] +
                                 cur[row][col+1]) / 5.0;
            }
        }
        // Swap buffers
        double (*tmp)[dim] = cur;
        cur = nxt;
        nxt = tmp;
    }

    timespec_get(&endTime, TIME_UTC);
    long long seconds   = endTime.tv_sec - startTime.tv_sec;
    long nanoseconds    = endTime.tv_nsec - startTime.tv_nsec;
    double s = (double)seconds + (double)nanoseconds * 1e-9;
    printf("Time = %.9lf seconds\n", s);

    saveGrid(argv[4], dim, cur);

    free(cur);
    free(nxt);
    return 0;
}

void initGrid(const char *infile, int dim, double a[dim][dim]) {
    FILE *in = fopen(infile, "r");
    if (!in) { printf("Cannot open file: %s\n", infile); return; }
    for (int row = 0; row < dim; row++) {
        for (int col = 0; col < dim; col++) {
            double temp;
            if (fscanf(in, "%lf", &temp) != EOF) {
                a[row][col] = temp;
            } else {
                printf("File does not have enough data points: %s\n", infile);
                fclose(in);
                return;
            }
        }
    }
    fclose(in);
}

void saveGrid(const char *outfile, int dim, double a[dim][dim]) {
    FILE *out = fopen(outfile, "w");
    if (!out) { printf("Cannot open output file: %s\n", outfile); return; }
    for (int row = 1; row < dim - 1; row++) {
        for (int col = 1; col < dim - 1; col++) {
            fprintf(out, "%6.3lf  ", a[row][col]);
        }
        fprintf(out, "\n");
    }
    fclose(out);
}
