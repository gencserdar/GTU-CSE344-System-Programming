#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#define MAX_AUTO_AMOUNT 8
#define MAX_PICKUP_AMOUNT 4
#define CAR_ARRIVAL_TIME 2
#define VALET_PARK_TIME 5

#define BLUE "\x1b[34m"
#define RED "\x1b[31m"
#define RESET "\x1b[0m"

typedef struct {
    int mFree_automobile_temp;
    int mFree_pickup_temp;
    int mFree_automobile;
    int mFree_pickup;
    int simulationEnd; 
} ParkingLot;

ParkingLot *parkingLot;

sem_t newAutomobile, inChargeforAutomobile, newPickup, inChargeforPickup;
pthread_mutex_t parkingLotMutex;

void signal_handler(int signum) {
    // Termination signal
    parkingLot->simulationEnd=1;
    // Post semaphores to stop waiting
    sem_post(&newAutomobile);
    sem_post(&newPickup);
    sem_post(&inChargeforAutomobile);
    sem_post(&inChargeforPickup);
    printf("\nCTRL+C or CTRL+Z signal received. Waiting for current threads in sleep()...\n");
}

void init() {
    int fd = shm_open("/parkingLot", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(ParkingLot));
    parkingLot = mmap(0, sizeof(ParkingLot), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    parkingLot->mFree_automobile_temp = MAX_AUTO_AMOUNT;
    parkingLot->mFree_pickup_temp = MAX_PICKUP_AMOUNT;
    parkingLot->mFree_automobile = MAX_AUTO_AMOUNT;
    parkingLot->mFree_pickup = MAX_PICKUP_AMOUNT;
    parkingLot->simulationEnd = 0;

    sem_init(&newAutomobile, 1, 0);
    sem_init(&inChargeforAutomobile, 1, 1);
    sem_init(&newPickup, 1, 0);
    sem_init(&inChargeforPickup, 1, 1);

    pthread_mutex_init(&parkingLotMutex, NULL);
}

void* carOwner(void* arg) {
    while (parkingLot->simulationEnd==0) { // This checks for unexpected termination signal
        sleep(CAR_ARRIVAL_TIME);  // Car arrival time

        int isAuto = rand() % 2;  // Randomly decide if the car is an automobile or a pickup

        pthread_mutex_lock(&parkingLotMutex);
        if (isAuto) {
            // Automobile arrival
            if (parkingLot->mFree_automobile_temp > 0 && parkingLot->mFree_automobile > 0 && (parkingLot->mFree_automobile + parkingLot->mFree_automobile_temp) > MAX_AUTO_AMOUNT) {
                parkingLot->mFree_automobile_temp--; // Place the car in the temporary automobile parking place.
                printf(BLUE "Automobile arrived. Temp spaces: %d, Real spaces: %d\n" RESET, parkingLot->mFree_automobile_temp, parkingLot->mFree_automobile);
                sem_post(&newAutomobile); // Signal the arrival of a new automobile
            } else printf(BLUE "Automobile arrived. No space. Exiting.\n" RESET);
        } else {
            // Pickup arrival
            if (parkingLot->mFree_pickup_temp > 0 && parkingLot->mFree_pickup > 0 && (parkingLot->mFree_pickup + parkingLot->mFree_pickup_temp) > MAX_PICKUP_AMOUNT) {
                parkingLot->mFree_pickup_temp--; // Place the car in the temporary pickup parking place.
                printf(RED "Pickup arrived. Temp spaces: %d, Real spaces: %d\n" RESET, parkingLot->mFree_pickup_temp, parkingLot->mFree_pickup);
                sem_post(&newPickup); // Signal the arrival of a new pickup
            } else printf(RED "Pickup arrived. No space. Exiting.\n" RESET);
        }
        pthread_mutex_unlock(&parkingLotMutex); 

        // Check if the parking lot is full
        if(parkingLot->mFree_automobile == 0 && parkingLot->mFree_pickup == 0){
            pthread_mutex_lock(&parkingLotMutex);
            parkingLot->simulationEnd = 1; // Inform other threads that parking lot is full and all cars are parked so simulation should be terminated
            pthread_mutex_unlock(&parkingLotMutex);
            // Increment semaphore values so valets wont wait for new cars
            sem_post(&newAutomobile);
            sem_post(&newPickup);
            break;
        }
    }
    return NULL;
}

void* carAttendantAuto(void* arg) {
    while (1) {
        sem_wait(&newAutomobile); // Wait for a new automobile to arrive
        sem_wait(&inChargeforAutomobile); // Ensure only one automobile is handled at a time
        pthread_mutex_lock(&parkingLotMutex);
        if (parkingLot->simulationEnd) { // Check if the simulation is ending
            pthread_mutex_unlock(&parkingLotMutex);
            sem_post(&inChargeforAutomobile);
            break;
        }
        pthread_mutex_unlock(&parkingLotMutex);
        printf(BLUE "Automobile valet took automobile.\n" RESET);
        sleep(VALET_PARK_TIME); // Simulate parking time
        sem_post(&inChargeforAutomobile); // Parking is done so post the semaphore

        pthread_mutex_lock(&parkingLotMutex);
        parkingLot->mFree_automobile_temp++; // Free 1 car space in temp auto parking lot
        parkingLot->mFree_automobile--; // Place the car in real auto parking lot
        printf(BLUE "Valet parked automobile. Temp spaces: %d, Real spaces: %d\n" RESET, parkingLot->mFree_automobile_temp, parkingLot->mFree_automobile);
        pthread_mutex_unlock(&parkingLotMutex);
    }
    return NULL;
}

void* carAttendantPickup(void* arg) {
    while (1) {
        sem_wait(&newPickup); // Wait for a new pickup to arrive
        sem_wait(&inChargeforPickup); // Ensure only one pickup is handled at a time
        pthread_mutex_lock(&parkingLotMutex);
        if (parkingLot->simulationEnd) {  //Check if the simulation is ending
            pthread_mutex_unlock(&parkingLotMutex);
            sem_post(&inChargeforPickup);
            break;
        }
        pthread_mutex_unlock(&parkingLotMutex);
        printf(RED "Pickup valet took pickup.\n" RESET);
        sleep(VALET_PARK_TIME); // Simulate parking time
        sem_post(&inChargeforPickup); // Parking is done so post the semaphore

        pthread_mutex_lock(&parkingLotMutex);
        parkingLot->mFree_pickup_temp++; // Free 1 car space in temp pickup parking lot
        parkingLot->mFree_pickup--; // Place the car in real pickup parking lot
        printf(RED "Valet parked pickup. Temp spaces: %d, Real spaces: %d\n" RESET, parkingLot->mFree_pickup_temp, parkingLot->mFree_pickup);
        pthread_mutex_unlock(&parkingLotMutex);
    }
    return NULL;
}

int main() {
    init(); // Initialize parkingLot struct, semaphores and parkingLotMutex

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting up SIGINT handler");
        return(1);
    }
    sa.sa_handler = signal_handler;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error setting up SIGTSTP handler");
        return(1);
    }

    pthread_t ownerThread, attendantAutoThread, attendantPickupThread;

    srand(time(NULL));

    // Create threads
    pthread_create(&attendantAutoThread, NULL, carAttendantAuto, NULL);
    pthread_create(&attendantPickupThread, NULL, carAttendantPickup, NULL);
    pthread_create(&ownerThread, NULL, carOwner, NULL);

    // Wait for threads to complete 
    pthread_join(ownerThread, NULL);
    pthread_join(attendantAutoThread, NULL);
    pthread_join(attendantPickupThread, NULL);

    // Cleanup to prevent memory leaks
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);
    pthread_mutex_destroy(&parkingLotMutex);
    shm_unlink("/parkingLot");
    printf("Simulation is ended. Cleanup successfull!\nTerminating the program...\n");
    return 0;
}