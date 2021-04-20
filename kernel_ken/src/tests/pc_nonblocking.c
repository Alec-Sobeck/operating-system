#include "app.h"
#include "kernel_ken.h"

// This test is similar to pc_blocking.c except it relies on non-blocking IO instead of semaphores 
// (infact, it doesn't use any semaphores at all). This test once again does a lot of automated 
// testing with assertions. If it succeeds, something like this should get printed out 
// (order of the lines and the exact PIDs may vary - the important thing is that all 6 processes exit):
// Process #2 is exiting
// Process #4 is exiting
// Process #6 is exiting
// Process #5 is exiting
// Process #3 is exiting
// Process #7 is exiting

#define PIPE_BUFFER_SIZE 12
#define PC3_NUM_ITER 20000


#define assert(b) ((b) ? (void)0 : u_assert(__FILE__, __LINE__, #b))
void u_assert(const char *file, unsigned int line, const char *desc) 
{
    // An assertion failed, kill the user process, I suppose.
    print("USER ASSERTION FAILED ("); 
    print(desc);
    print(") at "); 
    print(file);
    print(": ");
    print_dec(line);
    print("\n");
    for(;;);
}

unsigned int rand(unsigned int *seed)
{
    return (*seed = *seed * 214013L + 2531011L);
}

void try_add_num(int pipe, void *buff, unsigned int len, unsigned int num_fails)
{
    // Linear backoff
    int ret = write(pipe, buff, len);
    if(ret == 0){
        sleep(1 * (num_fails + 1));
        try_add_num(pipe, buff, len, num_fails + 1);
    }
}

void my_app()
{
    const int BUFFER_SIZE = 8;
    const int PRODUCER_COUNT = 5;

    print("Starting test \n");

    int pipe = open_pipe();
    int ret = fork();

    // This writes a sequence of random numbers to the pipe.
    // Because this uses a Pseudo-RNG, the Consumer can recreate the sequence of numbers generated by the Producer thread, to
    // check that the values are correct.
    if(ret == 0){
        // Processes are seeded with their PIDs, which makes recreating their sequence of random
        // numbers easy. Note that not all elements of this array are used.
        unsigned int lots_of_space = (PRODUCER_COUNT + 10 + (unsigned int)getpid());
        unsigned int *seeds = alloc(sizeof(unsigned int) * lots_of_space, 0);
        int i;
        for(i = 0; i < lots_of_space; i++){
            seeds[i] = (unsigned int)i;
        }

        // This reads values and checks return codes at the same time.
        // Change from related tests: this doesn't block with semaphores, it busy waits
        i = 0;
        while(i < PRODUCER_COUNT * PC3_NUM_ITER){
            char buff[BUFFER_SIZE];
            // Busy wait instead of a semaphore
            while(read(pipe, buff, BUFFER_SIZE) == 0) {
                yield();
            }

            // buff[0] is the process that wrote this element to the pipe
            // buff[1] is the random number that process just generated.
            unsigned int *buffer_cast = (unsigned int *)buff;
            int pid = (int)buffer_cast[0];
            unsigned int value = buffer_cast[1];

            // Next is the value that should've been written to the pipe.
            unsigned int next = rand(seeds + pid);
            // If there's a mismatch, that's an error - something went wrong when writing/reading from the pipe
            assert(next == value);
            i++;
        }
    } else {
        // Create (N - 1) producers (we already have one)
        int i;
        for(i = 0; i < (PRODUCER_COUNT - 1); i++){
            if(fork() == 0)
                break;
        }

        // Each producer seeds its RNG with its own PID to make this easier to automatically test with assertions
        unsigned int seed = (unsigned int)getpid();
        i = 0;
        while(i < PC3_NUM_ITER) {
            // Generate a random number
            unsigned int next = rand(&seed);

            // buff[0] is this producers PID
            // buff[1] is the random value
            char buff[BUFFER_SIZE];
            unsigned int *buffer_cast = (unsigned int *)buff;
            buffer_cast[0] = (unsigned int)getpid();
            buffer_cast[1] = next;

            // If the number cannot be written, sleep for a bit, so we don't waste a lot of CPU time
            try_add_num(pipe, buff, BUFFER_SIZE, 0);

            i++;

        }
    }

    print("Process #"); print_dec(getpid()); print(" is exiting\n");
}
