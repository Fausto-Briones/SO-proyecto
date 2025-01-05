#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct {
    char service_name[50];
    int log_count;
    int interval;
} stress_data_t;


void *generate_logs(void *arg) {
    stress_data_t *data = (stress_data_t *)arg;

    for (int i = 0; i < data->log_count; i++) {
        pid_t pid = fork();

        if (pid == 0) { //hijo
            // creando logs
            char log_message[128];
            snprintf(log_message, sizeof(log_message), "Prueba de estes %d para %s", i + 1, data->service_name);

            execlp("logger", "logger", "-t", data->service_name, log_message, NULL);
            perror("Error executing logger");
            exit(EXIT_FAILURE);
        } else if (pid > 0) { // padre
           
            wait(NULL);
            printf("Log creado para servicio %s (%d/%d)\n", data->service_name, i + 1, data->log_count);
        } else {
            perror("Error during fork");
        }

        //Esperar para generar siguiente log
        sleep(data->interval);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <service_name> <log_count> <interval>\n", argv[0]);
        return EXIT_FAILURE;
    }

    stress_data_t data;
    snprintf(data.service_name, sizeof(data.service_name), "%s", argv[1]);
    data.log_count = atoi(argv[2]);
    data.interval = atoi(argv[3]);

    if (data.log_count <= 0 || data.interval < 0) {
        fprintf(stderr, "Log count debe ser positivo\n");
        return EXIT_FAILURE;
    }

    printf("Generando %d logs para service: %s con intervalo de %d segundos.\n",
           data.log_count, data.service_name, data.interval);

    pthread_t stress_thread;

    //generate logs
    if (pthread_create(&stress_thread, NULL, generate_logs, &data) != 0) {
        perror("Error creating thread");
        return EXIT_FAILURE;
    }

    // Wait for thread 
    pthread_join(stress_thread, NULL);

    printf("Prueba de estres completada para el servicio: %s\n", data.service_name);
    return EXIT_SUCCESS;
}
