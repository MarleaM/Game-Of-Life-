/*
 * Hello! Welcome to this simulator :) Just as a note: all of this runs within
 * the terminal. Feel free to take a peak at some of the example files provided in 
 * the test_example_files folder for formatting. I hope you enjoy!
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation
 * ./gol file1.txt  2  # run with config file file1.txt, ParaVis animation
 *
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE (0)  // with no animation
#define OUTPUT_ASCII (1) // with ascii animation
#define OUTPUT_VISI (2)  // with ParaVis animation

// #define SLEEP_USECS  (100) (feel free to change this to be as slow or fast as you'd like)
#define SLEEP_USECS (100000)

static int total_live = 0;

struct gol_data{
    int rows;        // the row dimension
    int cols;        // the column dimension
    int iters;       // number of iterations to run the gol simulation
    int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI
    int *world;      // hard coded as blank, not sure how to make this dynamic
    int *next_world;
    int *temp;       // temporary pointer to hold data for when we switch boards
    int init_cells;
    int num_threads;
    int thread_id;
    int thread_row_start;
    int thread_row_end;
    int thread_col_start;
    int thread_col_end;
    int partition_yes_no;
    int round;
    /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
    visi_handle handle;
    color3 *image_buff;
};

/****************** Function Prototypes **********************/
/* making an arrtay that each partition should have*/
int *number_partition(struct gol_data *data, char **argv);
/* the main gol game playing loop (prototype must match this) */
void *play_gol(void *arg);
/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char **argv);
/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);
/* set the color of the cell if they are alive or dead*/
void update_colors(void *arg);
/* set the cell to alive or dead based on the alive live numbers and the rules*/
int set_cell_cond(struct gol_data *data, int alive_cells, int x, int y);
/* dynamically init matrix from the input file */
void init_matrix(struct gol_data *data);
/* examine the board to see how many cells are alive surrounding one cell */
int check_alive_cells(struct gol_data *data, int cell_x, int cell_y);
/************ Definitions for using ParVisi library ***********/
/* initialization for the ParaVisi library (DO NOT MODIFY) */
int setup_animation(struct gol_data *data);
/* register animation with ParaVisi library (DO NOT MODIFY) */
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";

static pthread_barrier_t barrierTime;
// initializing mutex
static pthread_mutex_t my_mutex;

int main(int argc, char **argv){

    struct gol_data data;
    struct timeval start_time, stop_time; // time struct
    double secs, altogether, micro_secs;
    int ret;
    int r = 0;
    int j = 0;

    /* check number of command line arguments */

    if (argc != 6){
        printf("usage: %s <infile.txt> <output_mode>[0|1|2] \
num_threads partition[0,1] print_partition[0,1]\n",
               argv[0]);
        printf("arg[2] Output mode: 0: no visualization, 1: ASCII, 2: ParaVisi\n");
        printf("arg[3] Number of threads\n");
        printf("arg[4] Partition flag: 0: row wise, 1: column wise\n");
        printf("arg[5] Print partition: 0: don't print configuration info,\
1: print allocation info\n");
        exit(1);
    }

    /* Initialize game state (all fields in data) from information
     * read from input file */
    ret = init_game_data_from_args(&data, argv);
    if (ret != 0){
        printf("Initialization error: file %s, mode %s, threads %s, partition %s, print partition %s\n",
               argv[1], argv[2], argv[3], argv[4], argv[5]);
        exit(1);
    }
    /* Initialize mutex and barrier */
    if (pthread_mutex_init(&my_mutex, NULL)){
        printf("pthread_mutex_init error\n");
        exit(1);
    }
    if (pthread_barrier_init(&barrierTime, NULL, data.num_threads)){
        printf("pthread_barrier_init error\n");
        exit(1);
    }
    int *result = number_partition(&data, argv); // partitioning info
    pthread_t *thread_array = malloc(data.num_threads * sizeof(pthread_t));
    if (!thread_array){
        perror("malloc: pthread_t array");
        exit(1);
    }
    struct gol_data *thread_ids = malloc(data.num_threads * sizeof(struct gol_data));
    if (!thread_ids){
        perror("malloc: pthread_t array");
        exit(1);
    }

    /* initialize ParaVisi animation (if applicable) */
    if (data.output_mode == OUTPUT_VISI)
    {
        setup_animation(&data);
    }

    /* Invoke play_gol in different ways based on the run mode */
    if (data.output_mode == OUTPUT_NONE || data.output_mode == OUTPUT_ASCII) { // run with no animation
        for (j = 0; j < data.num_threads; j++){ // create thread ids
            data.thread_id = j;
            if (atoi(argv[4]) == 1){
                data.thread_col_start = result[r];
                data.thread_col_end = result[r + 1];
                data.thread_row_start = 0;
                data.thread_row_end = data.rows - 1;
            }
            else{
                data.thread_row_start = result[r];
                data.thread_row_end = result[r + 1];
                data.thread_col_start = 0;
                data.thread_col_end = data.cols - 1;
            }
            r += 2;
            thread_ids[j] = data;}

        ret = gettimeofday(&start_time, NULL);
        for (j = 0; j < data.num_threads; j++){
            ret = pthread_create(&thread_array[j], NULL, play_gol, &thread_ids[j]);
            if (ret){ 
                perror("Error pthread_create\n"); 
                exit(1);
            } //checks to see if the thread is valid
        }
        for (j = 0; j < data.num_threads; j++){
            pthread_join(thread_array[j], NULL);
        }
        ret = gettimeofday(&stop_time, NULL);
    }

    else { //output visi w/ animation
        for (j = 0; j < data.num_threads; j++){ // create thread ids
            data.thread_id = j;
            if (atoi(argv[4]) == 1){
                data.thread_col_start = result[r];
                data.thread_col_end = result[r + 1];
                data.thread_row_start = 0;
                data.thread_row_end = data.rows - 1;
            }
            else{
                data.thread_row_start = result[r];
                data.thread_row_end = result[r + 1];
                data.thread_col_start = 0;
                data.thread_col_end = data.cols - 1;
            }
            r += 2;
            thread_ids[j] = data;
            ret = pthread_create(&thread_array[j], NULL, play_gol, &thread_ids[j]);
            if (ret){ 
                perror("Error pthread_create\n"); 
                exit(1);
            } //checks to see if the thread is valid
        }
        run_animation(data.handle, data.iters);
        for (j = 0; j < data.num_threads; j++){
            pthread_join(thread_array[j], NULL);
        }
    
    }

    if (data.output_mode != OUTPUT_VISI){
        secs = 0.0;
        micro_secs = 0.0;
        altogether = 0.0;
        secs = stop_time.tv_sec - start_time.tv_sec;
        micro_secs = stop_time.tv_usec / 1000000.0 - start_time.tv_usec / 1000000.0;
        altogether = secs + micro_secs;
        fprintf(stdout, "Total time: %0.3f seconds\n", altogether);
        fprintf(stdout, "Number of live cells after %d rounds: %d\n\n",
                data.iters, total_live);
    }
    free(thread_ids);
    free(thread_array);
    free(result);
    free(data.world);
    free(data.next_world);
    if (pthread_barrier_destroy(&barrierTime) != 0)
    {
        perror("Error destroying mutex.\n");
        exit(1);
    }
    if (pthread_mutex_destroy(&my_mutex) != 0)
    {
        perror("Error destroying mutex.\n");
        exit(1);
    }
    return 0;
}

/* figures out how to spilt up the function for the board printing/playing
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 * returns: result, an array of ints with partitioning information 
 */
int *number_partition(struct gol_data *data, char **argv){
    int num_threads = atoi(argv[3]);
    int *result = malloc(sizeof(int) * num_threads);
    int *final_result = malloc(sizeof(int) * num_threads * 2);
    int partition, left_over;
    int count = 0;
    int result_count = 0;
    if (atoi(argv[4]) == 0)
    {
        partition = data->rows / num_threads;
        left_over = data->rows % num_threads;
    }
    else
    {
        partition = data->cols / num_threads;
        left_over = data->cols % num_threads;
    }
    for (int i = 0; i < num_threads; i++)
    {
        result[i] = partition;
    }
    if (left_over != 0)
    {
        for (int j = 0; j < left_over; j++)
        {
            result[j] = result[j] + 1;
        }
    }
    final_result[0] = 0;
    for (int x = 1; x < num_threads * 2; ++x)
    {
        if (x % 2 == 0)
        {
            count += 1;
            final_result[x] = count;
        }
        else
        {
            count += result[result_count] - 1;
            final_result[x] = count;
            result_count++;
        }
    }
    free(result);
    return final_result;
}

/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 *       argv[3]: the number of threads
 *       argv[4]: the direction of partition
 *       argv[5]: print the partition
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 *       argv[3]: the number of threads
 *       argv[4]: the direction of partition
 *       argv[5]: print the partition
 * returns: 0 on success, 1 on error
 */
int init_game_data_from_args(struct gol_data *data, char **argv){
    FILE *infile;
    int ret, cell_x, cell_y, i;

    infile = fopen(argv[1], "r"); // opens the file

    // checking input validity
    if (infile == NULL){
        printf("Error: failed to open file: %s \n", argv[1]);
        exit(1);
    }

    if (atoi(argv[2]) < 0 || atoi(argv[2]) > 2){ // checking output validity
        printf("Error: invalid output mode: %s \n", argv[1]);
        exit(1);
    }

    if ((atoi(argv[3]) < 0)){ // checking thread validity
        printf("Error: invalid number of threads: %s \n", argv[1]);
        exit(1);
    }
    else{
        data->num_threads = atoi(argv[3]);
    }

    if (atoi(argv[4]) < 0 || atoi(argv[4]) > 1){ // checking partition validity
        printf("Error: invalid partition mode: %s \n", argv[1]);
        exit(1);
    }

    if (atoi(argv[5]) < 0 || atoi(argv[5]) > 1){ // checking print partition validity
        printf("Error: invalid print partition mode: %s \n", argv[1]);
        exit(1);
    }
    else{
        data->partition_yes_no = atoi(argv[5]);
    }

    ret = fscanf(infile, "%d%d%d%d", &(data->rows), &(data->cols), &(data->iters), &(data->init_cells));

    if (ret != 4){
        printf("fscanf error");
        exit(1);
    }

    data->world = malloc(sizeof(int) * (data->rows) * (data->cols));
    if (data->world == NULL){
        printf("Error: malloc failed\n");
        exit(1);
    }
    data->next_world = malloc(sizeof(int) * (data->rows) * (data->cols));
    if (data->next_world == NULL){
        printf("Error: malloc failed\n");
        exit(1);
    }

    init_matrix(data);
    for (i = 0; i < data->init_cells; i++){
        ret = fscanf(infile, "%d%d", &cell_y, &cell_x);
        if (ret == 2){
            data->world[(data->cols) * cell_y + cell_x] = 1;
        }
        else{
            printf("Error: wrong number of inputs for coordinates: %s\n", argv[1]);
            exit(1);
        }
    }

    ret = fclose(infile); // closes file
    if (ret != 0){
        printf("Error: failed to close the file: %s\n", argv[1]);
        exit(1);
    }
    if (atoi(argv[2]) >= 0 && atoi(argv[2]) <= 3){
        data->output_mode = atoi(argv[2]); // checks to see if the input for
    }                                      // printing is correct, else it exits the program
    else{
        printf("Error: mode not correct: %s\n", argv[2]);
        exit(1);
    }
    data->round = 0;
    // successfull termination
    return 0;
}

/* initialize two matrices using the row and columns stored in data
 * data: pointer to gol_data struct to initialize
 * returns: none
 */
void init_matrix(struct gol_data *data){
    int i, j;

    for (i = 0; i < data->rows; ++i)
    {
        for (j = 0; j < data->cols; ++j)
        {
            data->world[i * data->cols + j] = 0;
        }
    }
    for (i = 0; i < data->rows; ++i)
    {
        for (j = 0; j < data->cols; ++j)
        {
            data->next_world[i * data->cols + j] = 0;
        }
    }
}

/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *    * uses threading to partition the game 
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 *  returns: nothing--void function 
 */

void *play_gol(void *arg){
    struct gol_data *data = ((struct gol_data *)arg);
    int x, y, alive_cells, cell, round_cell, row_difference, col_difference, ret1, ret2;
    cell = 0;
    row_difference = data->thread_row_end - data->thread_row_start + 1;
    col_difference = data->thread_col_end - data->thread_col_start + 1;
    if (data->partition_yes_no == 1){ //checks to print partition info
        printf("tid %d: rows: %d:%d (%d) cols: %d:%d (%d)\n", data->thread_id, data->thread_row_start, data->thread_row_end, row_difference,
               data->thread_col_start, data->thread_col_end, col_difference);
    }
    while (data->round < data->iters){
        for (x = data->thread_row_start; x <= data->thread_row_end; ++x){
            for (y = data->thread_col_start; y <= data->thread_col_end; ++y){
                alive_cells = check_alive_cells(data, x, y);
                round_cell = set_cell_cond(data, alive_cells, x, y);
                cell += round_cell;
            }
        }
        pthread_mutex_lock(&my_mutex); // lock so that only one thread accesses global variable
        total_live += cell;
        pthread_mutex_unlock(&my_mutex); // unlock after one thread chaanges total_live
        data->temp = data->world;        // swapping worlds around
        data->world = data->next_world;
        data->next_world = data->temp;


        ret1 = pthread_barrier_wait(&barrierTime);
        if(ret1 != 0 && ret1 != PTHREAD_BARRIER_SERIAL_THREAD) {
            perror("pthread_barrier_wait");
            exit(1);
            }
        if (data->thread_id == 0){
            if(data->output_mode == 1){
                system("clear");
                print_board(data, data->round);
                usleep(SLEEP_USECS);
            }
            if(data->round < data->iters-1){
                pthread_mutex_lock(&my_mutex); // lock so that only one thread accesses global variable
                total_live = 0;
                pthread_mutex_unlock(&my_mutex);
            } // unlock after one thread chaanges total_live
        }
        ret2 = pthread_barrier_wait(&barrierTime);
        if(ret2 != 0 && ret2 != PTHREAD_BARRIER_SERIAL_THREAD) {
            perror("pthread_barrier_wait");
            exit(1);
            }
        if (data->output_mode == 2){
            update_colors(data);
            draw_ready(data->handle);
            usleep(SLEEP_USECS);
            
        }
        cell = 0;
        round_cell = 0;
        data->round += 1;
    }
    return NULL;
}

/* make a cell alive or dead based on the alive_cells and rules
 * data: pointer to gol_data struct to initialize
 * alive_cells: the number of alive cells that is surrounding the cell
 * x: the x coordinate of a cell
 * y: the y coordinate of a cell
 * returns: the number (int) of alive cells on the matrix
 */

int set_cell_cond(struct gol_data *data, int alive_cells, int x, int y){
    int cell;
    cell = 0;
    if (data->world[x * data->cols + y] == 0){
        if (alive_cells == 3){
            data->next_world[x * data->cols + y] = 1;
            cell++;
        }
        else{
            data->next_world[x * data->cols + y] = 0;
        }
    }
    else if (data->world[x * data->cols + y] == 1){
        if (alive_cells == 0 || alive_cells == 1){
            data->next_world[x * data->cols + y] = 0;
        }
        if (alive_cells == 2 || alive_cells == 3){
            data->next_world[x * data->cols + y] = 1;
            cell++;
        }
        if (alive_cells >= 4){
            data->next_world[x * data->cols + y] = 0;
        }
    }
    return cell;
}

/* check how many cells are alive surrounding a cell
 * data: pointer to gol_data struct to initialize
 * cell_x: the x coordinate of a cell
 * cell_y: the y coordinate of a cell
 * returns: the number (int) of alive cells that surrounds a cell
 */
int check_alive_cells(struct gol_data *data, int cell_x, int cell_y){
    int alive, i, j, cell_row, cell_col;
    alive = 0;
    for (i = -1; i < 2; i++){
        for (j = -1; j < 2; j++){
            if (i == 0 && j == 0){
                continue;
            }
            cell_row = (cell_y + i + data->rows) % (data->rows);
            cell_col = (cell_x + j + data->cols) % (data->cols);
            if (data->world[cell_col * data->cols + cell_row] == 1){
                alive++;
            }
        }
    }
    return alive;
}

/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 */
void print_board(struct gol_data *data, int round){

    int i, j;
    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round+1);

    for (i = 0; i < data->rows; ++i){
        for (j = 0; j < data->cols; ++j){
            if (data->world[i * data->rows + j] == 1){// have to use math formula
                fprintf(stderr, " @"); // if its 1 print @ else print .
            }
            else{
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }
    if (round == 0){
        total_live = data->init_cells;
    }
    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}

/* Describes how the pixels in the image buffer should be
 * colored based on the data in the grid.
 * data: pointer to gol_data struct to initialize
 * returns: none
 * 
 */

void update_colors(void *arg){
    struct gol_data *data = ((struct gol_data *)arg); 
    int i, j, r, c, index, buff_i;
    color3 *buff;

    buff = data->image_buff;  // just for readability
    r = data->thread_row_end;
    c = data->thread_col_end;

    for (i = data->thread_row_start; i <= r; ++i){
            for (j = data->thread_col_start; j <= c; ++j){
                index = i*data->cols + j;//indxing the array 
                buff_i = (data->rows - (i+1))*data->cols + j;
        
            if (data->world[index] == 1){
            // update animation buffer
                buff[buff_i] = c3_black;
            }
            else {
                buff[buff_i] = colors[(data->thread_id)%8];
            }
        }
    }
}

/**********************************************************/
/* initialize ParaVisi animation */
int setup_animation(struct gol_data *data){
    /* connect handle to the animation */
    int num_threads = data->num_threads;
    data->handle = init_pthread_animation(num_threads, data->rows,
                                          data->cols, visi_name);
    if (data->handle == NULL)
    {
        printf("ERROR init_pthread_animation\n");
        exit(1);
    }
    // get the animation buffer
    data->image_buff = get_animation_buffer(data->handle);
    if (data->image_buff == NULL)
    {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
    }
    return 0;
}

/* sequential wrapper functions around ParaVis library functions */
void (*mainloop)(struct gol_data *data);

void *seq_do_something(void *args){
    mainloop((struct gol_data *)args);
    return 0;
}
/******************************************************/