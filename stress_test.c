#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#define NUM_CPU_THREADS 4  // numero de threads para estresar cpu
#define MEMORY_SIZE_MB 500 // 500 MB  para estresar memoria

volatile int keep_running = 1; // Control flag

// estresar cpu
void *cpu_stress(void *arg) {
    while (keep_running) {
        // calculo
        double x = 0.0;
        for (int i = 0; i < 1000000; i++) {
            x += i * 0.123456789;
        }
    }
    return NULL;
}

// estresar memoria
void *memory_stress(void *arg) {
    size_t memory_size = MEMORY_SIZE_MB * 1024 * 1024; //MB to B
    char *memory_block = (char *)malloc(memory_size); //escribir dinamicamente en la memoria

    if (memory_block == NULL) {
        perror("Failed to allocate memory");
        pthread_exit(NULL);
    }

    // LLenar memoria actual para simular uso de memoria
    memset(memory_block, 0xFF, memory_size);

    // Hasta que termine la prueba
    while (keep_running) {
        usleep(100000); // evitar uso excesivo de CPU
    }

    free(memory_block);
    return NULL;
}

//manejo de la senial para detener la prueba
void handle_signal(int sig) {
    keep_running = 0;
}

int main() {
    pthread_t cpu_threads[NUM_CPU_THREADS];
    pthread_t memory_thread;

    // captura de la senial
    signal(SIGINT, handle_signal);

    printf("Starting stress test...\n");
    printf("Press Ctrl+C to stop the stress test.\n");

    // threads de cpu
    for (int i = 0; i < NUM_CPU_THREADS; i++) {
        if (pthread_create(&cpu_threads[i], NULL, cpu_stress, NULL) != 0) {
            perror("Error creating CPU stress thread");
            exit(EXIT_FAILURE);
        }
    }

    // thread de memoria
    if (pthread_create(&memory_thread, NULL, memory_stress, NULL) != 0) {
        perror("Error creating memory stress thread");
        exit(EXIT_FAILURE);
    }

    // Esperar a que los thread terminen
    for (int i = 0; i < NUM_CPU_THREADS; i++) {
        pthread_join(cpu_threads[i], NULL);
    }
    pthread_join(memory_thread, NULL);

    printf("Stress test stopped.\n");
    return 0;
}
