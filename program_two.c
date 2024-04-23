#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define FLOWER_COUNT 10
#define SHM_NAME "/flower_shm"

typedef struct {
    int state;  // 0 = healthy, 1 = withering, 2 = withered, 3 = overflowed
    sem_t semaphore;  // Unnamed semaphore for each flower
} Flower;

Flower *flowers;  // This will point to our shared memory
int shm_fd;

void cleanup_resources(int signum) {
    // Close and unmap the shared memory
    if (flowers) {
        for (int i = 0; i < FLOWER_COUNT; ++i) {
            sem_destroy(&flowers[i].semaphore);  // Destroy the semaphore
        }
        munmap(flowers, sizeof(Flower) * FLOWER_COUNT);
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(SHM_NAME);  // Unlink the shared memory object
    }
    exit(signum);
}

void initialize_resources() {
    // Create shared memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, sizeof(Flower) * FLOWER_COUNT) == -1) {
        perror("ftruncate");
        cleanup_resources(EXIT_FAILURE);
    }

    // Map shared memory
    flowers = mmap(NULL, sizeof(Flower) * FLOWER_COUNT, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (flowers == MAP_FAILED) {
        perror("mmap");
        cleanup_resources(EXIT_FAILURE);
    }

    // Initialize flower states and semaphores
    for (int i = 0; i < FLOWER_COUNT; ++i) {
        flowers[i].state = 0; // All flowers start healthy
        // flowers[i].being_watered = 0;
        sem_init(&flowers[i].semaphore, 1, 1);  // Initialize semaphore in shared memory
    }
}

void *gardener_routine(void *arg) {
    int gardener_id = *(int *)arg;
    while (1) {
        for (int i = 0; i < FLOWER_COUNT; ++i) {
            sem_wait(&flowers[i].semaphore);
            if (flowers[i].state == 1) {
                printf("Gardener %d is watering flower %d.\n", gardener_id, i);
                flowers[i].state = 0;
            }
            sem_post(&flowers[i].semaphore);
            usleep(100000);  // Simulate delay
        }
    }
    return NULL;
}

void simulate_flower(int flower_index) {
    srand(time(NULL) + flower_index);  // Seed random number generator
    while (1) {
        int sleep_time = rand() % 5000000 + 1000000;  // Flower changes state randomly between 1 to 6 seconds
        usleep(sleep_time);
        sem_wait(&flowers[flower_index].semaphore);
        if (flowers[flower_index].state == 0) {
            flowers[flower_index].state = 1;  // Change state to withering
            printf("Flower %d has started withering.\n", flower_index);
        }
        sem_post(&flowers[flower_index].semaphore);
    }
}

int main() {
    // Set up signal handling for graceful termination
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = cleanup_resources;
    sigaction(SIGINT, &action, NULL);

    initialize_resources();

    pthread_t gardeners[2];
    int gardener_ids[2] = {1, 2};

    // Creating gardener threads
    for (int i = 0; i < 2; ++i) {
        if (pthread_create(&gardeners[i], NULL, gardener_routine, &gardener_ids[i]) != 0) {
            perror("Failed to create gardener thread");
            cleanup_resources(EXIT_FAILURE);
        }
    }

    // Fork flower processes
    pid_t pids[FLOWER_COUNT];
    for (int i = 0; i < FLOWER_COUNT; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("Failed to fork flower process");
            cleanup_resources(EXIT_FAILURE);
        } else if (pids[i] == 0) {  // Child process
            simulate_flower(i);
            exit(EXIT_SUCCESS);
        }
    }

    // Wait for gardener threads to finish (they never will in this setup)
    for (int i = 0; i < 2; ++i) {
        pthread_join(gardeners[i], NULL);
    }

    pause();  // The main process waits here until interrupted
    cleanup_resources(EXIT_FAILURE);
    return 0;
}
