#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct
    {
        // this struct is made to send as an input to thread functions

        int **grid;        // sudoku
        int size;          // dimension(Row/Column)
        int t_id;          // thread id
        int no_of_threads; // total number of threads

        int t_offset;   // just a variable to differentiate between threads(which check rows,columns,subgrids)
        bool result;    // whether the result of the thread is valid/invalid.
        char *messages; // a buffer created to display all the messages in the output file

    } t_input;

FILE *out_file; // global declaration of output file

bool check_for_row(int size, int **array, int row)
    {
        // this function validates one row at a time

        bool *numbers = (bool *)malloc(size * sizeof(bool)); // an array which stores whether the numbers 1 to N-1 are encountered or not.

        for (int i = 0; i < size; i++)
            {
                // initialising all numbers as not encountered.

                numbers[i] = false;
            }

        bool output = true; // output variable to store whether entire row is valid/invalid.

        for (int col = 0; col < size; col++) // iterating through all
            {
                int read_num = array[row][col]; // variable that reads the number from sudoku

                if (numbers[read_num - 1])
                    {
                        // case when we read an already encountered number.

                        output = false;
                        break;
                    }

                numbers[read_num - 1] = true; // making the recently read number as encountered.
            }

        free(numbers);
        return output;
    }

bool check_for_col(int size, int **array, int col)
    {
        // this function validates one column at a time. Rest logic is same as that of checking rows.

        bool *numbers = (bool *)malloc(size * sizeof(bool));

        for (int i = 0; i < size; i++)
            {
                numbers[i] = false;
            }

        bool output = true;

        for (int row = 0; row < size; row++)
            {
                int read_num = array[row][col];

                if (numbers[read_num - 1])
                    {
                        output = false;
                        break;
                    }

                numbers[read_num - 1] = true;
            }

        free(numbers);
        return output;
    }

bool check_for_subgrid(int size, int **array, int row, int col)
    {
        // this function validates one subgrid at a time.

        int n = sqrt(size); // variable that stores the dimension(length) of each subgrid

        bool *numbers = (bool *)malloc(size * sizeof(bool));

        for (int i = 0; i < size; i++)
            {
                numbers[i] = false;
            }

        bool output = true;

        // iteration stops once it checks 'n' numbers along any dimension.

        for (int i = row; i < row + n; i++)
            {
                for (int j = col; j < col + n; j++)
                    {
                        int read_num = array[i][j];

                        if (numbers[read_num - 1])
                            {
                                output = false;
                                break;
                            }

                        numbers[read_num - 1] = true;
                    }
            }

        free(numbers);
        return output;
    }

void *check_rows_chunk(void *param)
    {
        // this function checks all rows using chunk method

        t_input *inp = (t_input *)param; // unpacking(typecasting) into our struct.

        int chunk_size = inp->size / inp->no_of_threads; // calculating chunk size, starting point and ending point of iteration
        int starting_pt = inp->t_id * chunk_size;
        int end;

        if (inp->t_id == inp->no_of_threads - 1)
            {
                // case when thread is the last thread.
                end = inp->size;
            }

        else
            {
                end = starting_pt + chunk_size;
            }

        inp->result = true; // assuming by default that all rows are valid rows.

        for (int i = starting_pt; i < end; i++)
            {
                bool validity = check_for_row(inp->size, inp->grid, i); // temporary variable to track if a particular row is valid/invalid

                char temp[1000]; // temporary array to store message buffers
                sprintf(temp, "Thread %d checks row %d and is %s.\n", inp->t_id + 1, i + 1, (validity) ? "valid" : "invalid");
                strcat(inp->messages, temp);

                if (!validity)
                    {
                        // updating the validness of a particular row
                        inp->result = false;
                    }
            }

        return NULL;
    }

void *check_cols_chunk(void *param)
    {
        // this function checks all the columns using chunk method. Underlying logic is same as that of rows

        t_input *inp = (t_input *)param;

        int chunk_size = inp->size / inp->no_of_threads;
        int starting_pt = inp->t_id * chunk_size;
        int end;

        if (inp->t_id == inp->no_of_threads - 1)
            {
                end = inp->size;
            }

        else
            {
                end = starting_pt + chunk_size;
            }

        inp->result = true;

        for (int i = starting_pt; i < end; i++)
            {
                bool validity = check_for_col(inp->size, inp->grid, i);

                char temp[1000];
                sprintf(temp, "Thread %d checks col %d and is %s.\n", inp->t_id + inp->t_offset + 1, i + 1, (validity) ? "valid" : "invalid");
                strcat(inp->messages, temp);

                if (!validity)
                    {
                        inp->result = false;
                    }
            }

        return NULL;
    }

void *check_subgrids_chunk(void *param)
    {
        // this function checks if all the grids are valid/invalid using Chunk method.

        t_input *inp = (t_input *)param; // unpacking into struct and calculating starting and ending of iteration.

        int n = sqrt(inp->size);
        int chunk_size = inp->size / inp->no_of_threads;
        int starting_pt = inp->t_id * chunk_size;
        int end;

        if (inp->t_id == inp->no_of_threads - 1)
            {
                end = inp->size;
            }

        else
            {
                end = starting_pt + chunk_size;
            }

        inp->result = true; // default assumption

        for (int i = starting_pt; i < end; i++)
            {
                int row = (i / n) * n; // obtaining row and column of grid using iteration variables.
                int col = (i % n) * n;

                bool validity = check_for_subgrid(inp->size, inp->grid, row, col);

                char temp[1000];
                sprintf(temp, "Thread %d checks grid %d and is %s.\n", inp->t_id + inp->t_offset + 1, i + 1, (validity) ? "valid" : "invalid");
                strcat(inp->messages, temp);

                if (!validity)
                    {
                        inp->result = false;
                    }
            }

        return NULL;
    }

void *check_rows_mixed(void *param)
    {
        // this function checks if all rows are valid using mixed method. Here there is nothing to calculate like chunk size in chunk method.

        t_input *inp = (t_input *)param;
        inp->result = true;

        for (int i = inp->t_id; i < inp->size; i += inp->no_of_threads)
            {
                // implementing mixed method

                bool validity = check_for_row(inp->size, inp->grid, i); // to check if a particular row is valid

                char temp[1000];
                sprintf(temp, "Thread %d checks row %d and is %s.\n", inp->t_id + 1, i + 1, (validity) ? "valid" : "invalid");
                strcat(inp->messages, temp);

                if (!validity)
                    {
                        inp->result = false;
                    }
            }

        return NULL;
}

void *check_cols_mixed(void *param)
    {
        // this function checks if all columns are valid using mixed method. Same underlying logic.

        t_input *inp = (t_input *)param;
        inp->result = true;

        for (int i = inp->t_id; i < inp->size; i += inp->no_of_threads)
            {
                bool validity = check_for_col(inp->size, inp->grid, i);

                char temp[1000];
                sprintf(temp, "Thread %d checks col %d and is %s.\n", inp->t_id + inp->t_offset + 1, i + 1, (validity) ? "valid" : "invalid");
                strcat(inp->messages, temp);

                if (!validity)
                    {
                        inp->result = false;
                    }
            }

        return NULL;
    }

void *check_subgrids_mixed(void *param)
    {
        // this functions checks if all the subgrids are valid using mixed method.

        t_input *inp = (t_input *)param;
        int n = sqrt(inp->size);

        inp->result = true;

        for (int i = inp->t_id; i < inp->size; i += inp->no_of_threads)
            {
                int row = (i / n) * n;
                int col = (i % n) * n;

                bool validity = check_for_subgrid(inp->size, inp->grid, row, col);

                char temp[1000];
                sprintf(temp, "Thread %d checks grid %d and is %s.\n", inp->t_id + inp->t_offset + 1, i + 1, (validity) ? "valid" : "invalid");
                strcat(inp->messages, temp);

                if (!validity)
                    {
                        inp->result = false;
                    }
            }

        return NULL;
    }

bool check_for_sudoku(int size, int **array, int no_of_threads, bool do_chunk)
    {
        // this function checks if entire Sudoku is valid using both the chunk and mixed methods.

        int msg_size = 200 * size; // declaring the size of each message to be buffered.

        int threads_for_rows = no_of_threads / 3; // dividing total number of threads into three sets
        int threads_for_cols = no_of_threads / 3;
        int threads_for_subgrids = no_of_threads - threads_for_cols - threads_for_rows;

        pthread_t *row_t_ids = malloc(threads_for_rows * sizeof(pthread_t)); // creating arrays to store thread ids.
        pthread_t *col_t_ids = malloc(threads_for_cols * sizeof(pthread_t));
        pthread_t *subgrid_t_ids = malloc(threads_for_subgrids * sizeof(pthread_t));

        t_input *row_inps = malloc(threads_for_rows * sizeof(t_input)); // creating an array of structs in order to input to the thread functions
        t_input *col_inps = malloc(threads_for_cols * sizeof(t_input));
        t_input *subgrid_inps = malloc(threads_for_subgrids * sizeof(t_input));

        pthread_attr_t attr; // obtaining default thread attributes
        pthread_attr_init(&attr);

        struct timeval start, end; // starting the time
        gettimeofday(&start, NULL);

        for (int i = 0; i < threads_for_rows; i++)
            {
                // this for loop creates all threads that validate rows

                row_inps[i].grid = array; // initialising the parameters present in struct
                row_inps[i].size = size;
                row_inps[i].t_id = i;
                row_inps[i].no_of_threads = threads_for_rows;

                row_inps[i].t_offset = 0;
                row_inps[i].messages = (char *)malloc(msg_size * sizeof(char)); // dynamically allocating memory for message buffer
                row_inps[i].messages[0] = '\0';                                 // initialising it to a null character

                if (do_chunk)
                    {
                        // if we want to validate using chunk method

                        pthread_create(&row_t_ids[i], &attr, check_rows_chunk, &row_inps[i]);
                    }

                else
                    {
                        pthread_create(&row_t_ids[i], &attr, check_rows_mixed, &row_inps[i]);
                    }
            }

        for (int i = 0; i < threads_for_cols; i++)
            {
                // column thread creation similar to row threads

                col_inps[i].grid = array;
                col_inps[i].size = size;
                col_inps[i].t_id = i;
                col_inps[i].no_of_threads = threads_for_cols;

                col_inps[i].t_offset = threads_for_rows;
                col_inps[i].messages = (char *)malloc(msg_size * sizeof(char));
                col_inps[i].messages[0] = '\0';

                if (do_chunk)
                    {
                        pthread_create(&col_t_ids[i], &attr, check_cols_chunk, &col_inps[i]);
                    }

                else
                    {
                        pthread_create(&col_t_ids[i], &attr, check_cols_mixed, &col_inps[i]);
                    }
            }

        for (int i = 0; i < threads_for_subgrids; i++)
            {
                // subgrid threads creation

                subgrid_inps[i].grid = array;
                subgrid_inps[i].size = size;
                subgrid_inps[i].t_id = i;
                subgrid_inps[i].no_of_threads = threads_for_subgrids;

                subgrid_inps[i].t_offset = threads_for_rows + threads_for_cols;
                subgrid_inps[i].messages = (char *)malloc(msg_size * sizeof(char));
                subgrid_inps[i].messages[0] = '\0';

                if (do_chunk)
                    {
                        pthread_create(&subgrid_t_ids[i], &attr, check_subgrids_chunk, &subgrid_inps[i]);
                    }

                else
                    {
                        pthread_create(&subgrid_t_ids[i], &attr, check_subgrids_mixed, &subgrid_inps[i]);
                    }
            }

        bool validity = true; // variable to check if the entire sudoku is valid

        for (int i = 0; i < threads_for_rows; i++)
            {
                // waiting for row threads to join

                pthread_join(row_t_ids[i], NULL);

                fprintf(out_file, "%s", row_inps[i].messages); // printing the message of threads in order
                free(row_inps[i].messages);                      // freeing the memory allocated to messages while thread creation

                if (!row_inps[i].result)
                    {
                        // if any row is invalid, updating the validness of Sudoku

                        validity = false;
                    }
            }

        for (int i = 0; i < threads_for_cols; i++)
            {
                // Waiting for column threads to join and printing the output

                pthread_join(col_t_ids[i], NULL);

                fprintf(out_file, "%s", col_inps[i].messages);
                free(col_inps[i].messages);

                if (!col_inps[i].result)
                    {
                        validity = false;
                    }
            }

        for (int i = 0; i < threads_for_subgrids; i++)
            {
                // similar tp above loops, but applies for subgrids

                pthread_join(subgrid_t_ids[i], NULL);

                fprintf(out_file, "%s", subgrid_inps[i].messages);
                free(subgrid_inps[i].messages);

                if (!subgrid_inps[i].result)
                    {
                        validity = false;
                    }
            }

        gettimeofday(&end, NULL); // end the time and calculate the time taken to validate
        double time_calculated = (end.tv_sec - start.tv_sec) * 1e6;
        time_calculated = (time_calculated + (end.tv_usec - start.tv_usec));

        fprintf(out_file, "The total time taken is %.2f microseconds.\n", time_calculated); // printing the total time taken

        free(row_inps); // Cleaning all the memories
        free(col_inps);
        free(subgrid_inps);
        free(row_t_ids);
        free(col_t_ids);
        free(subgrid_t_ids);

        return validity; // returning the validness of Sudoku
    }

bool check_sudoku_sequential(int size, int **array)
    {
        // this function checks if the entire sudoku is valid without any threads

        bool validity = true;
        int n = sqrt(size);

        struct timeval start, end;
        gettimeofday(&start, NULL);

        for (int i = 0; i < size; i++)
            {
                // this loop checks if any one row or column is invalid

                if (!check_for_row(size, array, i) || !check_for_col(size, array, i))
                    {
                        // if any row/column is invalid, update the validness

                        validity = false;
                    }
            }

        for (int i = 0; i < size; i += n) // this loop checks if any subgrid is invalid and if so then update the validness
            {
                for (int j = 0; j < size; j += n)
                    {
                        if (!check_for_subgrid(size, array, i, j))
                            {
                                validity = false;
                            }
                    }
            }

        gettimeofday(&end, NULL); // time calculation
        double time_calculated = (end.tv_sec - start.tv_sec) * 1e6;
        time_calculated = (time_calculated + (end.tv_usec - start.tv_usec));

        fprintf(out_file, "The total time taken is %.2f microseconds.\n", time_calculated); // printing the output

        return validity;
    }

int main()
    {
        FILE *inp_file = fopen("inp.txt", "r");

        if (inp_file == NULL)
            {
                // if input file couldn't be opened

                printf("ERROR: Something went wrong opening input.txt\n");
                return -1;
            }

        int no_of_threads, grid_size = 0;
        fscanf(inp_file, "%d %d", &no_of_threads, &grid_size); // reading number of threads and sudoku size from input file

        int n = sqrt(grid_size);

        if (n * n != grid_size)
            {
                // if the value of N is not a perfect square

                printf("ERROR: Invalid input format.\n");
                fclose(inp_file);
                return -1;
            }

        int **sudoku = (int **)malloc(grid_size * sizeof(int *)); // allocating memory for sudoku

        for (int i = 0; i < grid_size; i++)
            {
                sudoku[i] = (int *)malloc(grid_size * sizeof(int));
            }

        for (int i = 0; i < grid_size; i++)
            {
                for (int j = 0; j < grid_size; j++)
                    {
                        fscanf(inp_file, "%d", &sudoku[i][j]); // reading the sudoku from the file
                    }
            }

        fclose(inp_file);

        out_file = fopen("output.txt", "w");

        if (out_file == NULL)
            {
                printf("ERROR: Something went wrong opening output.txt\n");
            }

        fprintf(out_file, "\t\tSequential method: \n");
        bool sequential_result = check_sudoku_sequential(grid_size, sudoku);
        fprintf(out_file, "Validation result: %s.\n", (sequential_result) ? "valid" : "invalid");

        fprintf(out_file, "\n\t\tChunk method: \n");
        bool chunk_result = check_for_sudoku(grid_size, sudoku, no_of_threads, true);
        fprintf(out_file, "Validation result: %s.\n", (chunk_result) ? "valid" : "invalid");

        fprintf(out_file, "\n\t\tMixed method: \n");
        bool mixed_result = check_for_sudoku(grid_size, sudoku, no_of_threads, false);
        fprintf(out_file, "Validation result: %s.\n", (mixed_result) ? "valid" : "invalid");

        for (int i = 0; i < grid_size; i++) // Clearing the memory
            {
                free(sudoku[i]);
            }

        fclose(out_file);
        free(sudoku);
        return 0;
    }