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
#define SEM_NAME_PREFIX "/flower_sem_"

typedef struct {
    int state;  // 0 = healthy, 1 = withering, 2 = withered, 3 = overflowed
    // int being_watered;
} Flower;

Flower *flowers;  // This will point to our shared memory
sem_t *semaphores[FLOWER_COUNT];
int shm_fd;

void cleanup_resources(int signum) {
    for (int i = 0; i < FLOWER_COUNT; ++i) {
        if (semaphores[i]) {
            sem_close(semaphores[i]);
            char sem_name[256];
            snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_NAME_PREFIX, i);
            sem_unlink(sem_name);
        }
    }
    if (flowers) {
        munmap(flowers, sizeof(Flower) * FLOWER_COUNT);
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
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

    // Initialize flower states
    for (int i = 0; i < FLOWER_COUNT; ++i) {
        flowers[i].state = 0; // All flowers start healthy
        // flowers[i].being_watered = 0;
    }

    // Create semaphores
    for (int i = 0; i < FLOWER_COUNT; ++i) {
        char sem_name[256];
        snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_NAME_PREFIX, i);
        // sem_unlink(sem_name);  // Unlink first to clear any previous semaphores
        semaphores[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 1);
        if (semaphores[i] == SEM_FAILED) {
            perror("sem_open");
            cleanup_resources(EXIT_FAILURE);
        }
    }
}


void *gardener_routine(void *arg) {
    int gardener_id = *(int *)arg;
    while (1) {
        for (int i = 0; i < FLOWER_COUNT; ++i) {
            sem_wait(semaphores[i]);
            if (flowers[i].state == 1) {  // Check if the flower is withering
                printf("Gardener %d is watering flower %d.\n", gardener_id, i);
                flowers[i].state = 0;  // Flower goes back to being healthy
                // flowers[i].being_watered = 0;
            }
            sem_post(semaphores[i]);
            usleep(100000);  // Sleep for a short time to simulate time delay in watering
        }
        usleep(500000);  // Gardener checks after a brief pause
    }
    return NULL;
}


void simulate_flower(int flower_index) {
    srand(time(NULL) + flower_index);  // Seed random number generator
    while (1) {
        int sleep_time = rand() % 5000000 + 1000000;  // Flower changes state randomly between 1 to 6 seconds
        usleep(sleep_time);
        sem_wait(semaphores[flower_index]);
        if (flowers[flower_index].state == 0) {
            flowers[flower_index].state = 1;  // Change state to withering
            printf("Flower %d has started withering.\n", flower_index);
        }
        sem_post(semaphores[flower_index]);
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
