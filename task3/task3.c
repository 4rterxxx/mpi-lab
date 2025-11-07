#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

double* allocate_matrix_1d(int rows, int cols) {
    return (double*)malloc(rows * cols * sizeof(double));
}

void free_matrix_1d(double* matrix) {
    free(matrix);
}

void initialize_matrix_1d(double* matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i * cols + j] = (double)rand() / RAND_MAX * 10.0;
        }
    }
}

double get_element(double* matrix, int cols, int i, int j) {
    return matrix[i * cols + j];
}

void set_element(double* matrix, int cols, int i, int j, double value) {
    matrix[i * cols + j] = value;
}

double sequential_matrix_multiply(int n) {
    double* A = allocate_matrix_1d(n, n);
    double* B = allocate_matrix_1d(n, n);
    double* C = allocate_matrix_1d(n, n);
    
    initialize_matrix_1d(A, n, n);
    initialize_matrix_1d(B, n, n);
    
    double start = MPI_Wtime();
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += get_element(A, n, i, k) * get_element(B, n, k, j);
            }
            set_element(C, n, i, j, sum);
        }
    }
    
    double end = MPI_Wtime();
    
    free_matrix_1d(A);
    free_matrix_1d(B);
    free_matrix_1d(C);
    
    return end - start;
}

void cannon_matrix_multiply(int n, int grid_size, int my_rank) {
    int block_size = n / grid_size;
    int block_elements = block_size * block_size;
    
    double* local_A = allocate_matrix_1d(block_size, block_size);
    double* local_B = allocate_matrix_1d(block_size, block_size);
    double* local_C = allocate_matrix_1d(block_size, block_size);
    
    srand(time(NULL) + my_rank);
    initialize_matrix_1d(local_A, block_size, block_size);
    initialize_matrix_1d(local_B, block_size, block_size);
    
    for (int i = 0; i < block_elements; i++) {
        local_C[i] = 0.0;
    }
    
    int row = my_rank / grid_size;
    int col = my_rank % grid_size;
    
    MPI_Status status;
    
    int left = (col - 1 + grid_size) % grid_size;
    int right = (col + 1) % grid_size;
    int dest_A = row * grid_size + left;
    int source_A = row * grid_size + right;
    
    int up = (row - 1 + grid_size) % grid_size;
    int down = (row + 1) % grid_size;
    int dest_B = up * grid_size + col;
    int source_B = down * grid_size + col;
    
    if (row > 0) {
        MPI_Sendrecv_replace(local_A, block_elements, MPI_DOUBLE,
                           dest_A, 0, source_A, 0, MPI_COMM_WORLD, &status);
    }
    
    if (col > 0) {
        MPI_Sendrecv_replace(local_B, block_elements, MPI_DOUBLE,
                           dest_B, 1, source_B, 1, MPI_COMM_WORLD, &status);
    }
    
    for (int step = 0; step < grid_size; step++) {
        for (int i = 0; i < block_size; i++) {
            for (int j = 0; j < block_size; j++) {
                double sum = 0.0;
                for (int k = 0; k < block_size; k++) {
                    sum += get_element(local_A, block_size, i, k) * 
                           get_element(local_B, block_size, k, j);
                }
                set_element(local_C, block_size, i, j, 
                           get_element(local_C, block_size, i, j) + sum);
            }
        }
        
        MPI_Sendrecv_replace(local_A, block_elements, MPI_DOUBLE,
                           dest_A, 2, source_A, 2, MPI_COMM_WORLD, &status);
        
        MPI_Sendrecv_replace(local_B, block_elements, MPI_DOUBLE,
                           dest_B, 3, source_B, 3, MPI_COMM_WORLD, &status);
    }
    
    if (my_rank == 0) {
        double* global_C = allocate_matrix_1d(n, n);
        
        for (int i = 0; i < n * n; i++) {
            global_C[i] = 0.0;
        }
        
        for (int i = 0; i < block_size; i++) {
            for (int j = 0; j < block_size; j++) {
                set_element(global_C, n, i, j, get_element(local_C, block_size, i, j));
            }
        }
        
        for (int proc = 1; proc < grid_size * grid_size; proc++) {
            double* temp_block = allocate_matrix_1d(block_size, block_size);
            MPI_Recv(temp_block, block_elements, MPI_DOUBLE,
                    proc, 4, MPI_COMM_WORLD, &status);
            
            int src_row = proc / grid_size;
            int src_col = proc % grid_size;
            int start_row = src_row * block_size;
            int start_col = src_col * block_size;
            
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    set_element(global_C, n, start_row + i, start_col + j,
                               get_element(temp_block, block_size, i, j));
                }
            }
            
            free_matrix_1d(temp_block);
        }
              
        free_matrix_1d(global_C);
    } else {
        MPI_Send(local_C, block_elements, MPI_DOUBLE, 0, 4, MPI_COMM_WORLD);
    }
    
    free_matrix_1d(local_A);
    free_matrix_1d(local_B);
    free_matrix_1d(local_C);
}

int main(int argc, char** argv) {
    int comm_sz, my_rank;
    double start_time, end_time;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    int grid_size = (int)sqrt(comm_sz);
    if (grid_size * grid_size != comm_sz) {
        if (my_rank == 0) {
            printf("Error: Number of processes must be a perfect square.\n");
            printf("Current: %d, use 1, 4, 9, 16.\n", comm_sz);
        }
        MPI_Finalize();
        return 1;
    }
    
    if (my_rank == 0) {
        printf("Cannon's Algorithm Performance\n");
        printf("Processes: %d (grid: %dx%d)\n\n", comm_sz, grid_size, grid_size);
    }
    
    int sizes[] = {256, 512, 1024, 2048};
    //int sizes[] = {243, 729, 2187}; //для 9 процессов
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        
        if (n % grid_size != 0) {
            if (my_rank == 0) {
                printf("Size %d not divisible by grid %d. Skipping.\n", n, grid_size);
            }
            continue;
        }
        
        double sequential_time = 0.0;
        if (my_rank == 0) {
            sequential_time = sequential_matrix_multiply(n);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        start_time = MPI_Wtime();
        
        cannon_matrix_multiply(n, grid_size, my_rank);
        
        MPI_Barrier(MPI_COMM_WORLD);
        end_time = MPI_Wtime();
        
        if (my_rank == 0) {
            double parallel_time = end_time - start_time;
            double speedup = sequential_time / parallel_time;
            double efficiency = speedup / comm_sz;
            
            printf("Size: %dx%d\n", n, n);
            printf("  Sequential: %.4f s\n", sequential_time);
            printf("  Parallel: %.4f s\n", parallel_time);
            printf("  Speedup: %.2f\n", speedup);
            printf("  Efficiency: %.2f%%\n", efficiency * 100);
            printf("  Block: %dx%d\n\n", n/grid_size, n/grid_size);
        }
    }
    
    if (my_rank == 0) {
        printf("All tests completed!\n");
    }
    
    MPI_Finalize();
    return 0;
}