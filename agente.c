#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define PIPE_NAME "log_pipe"
#define UPDATE_INTERVAL 5

typedef struct {
    char service_name[256];
    char priority[16];
    int update_interval;
    int *logs;
    pthread_mutex_t *mutex;
} ServiceConfig;

void *monitor_service(void *args) {
    ServiceConfig *config = (ServiceConfig *)args;
    char buffer[1024];
    int fd, pipe_fd[2];

    while (1) {
        if (pipe(pipe_fd) == -1) {
            perror("Error creating pipe");
            pthread_exit(NULL);
        }

        pid_t pid = fork();
        if (pid == 0) { // Proceso hijo
            close(pipe_fd[0]);
            dup2(pipe_fd[1], STDOUT_FILENO); // Redirigir a la salida estandar y cerrar el pipe porque se genera otro descriptor
            close(pipe_fd[1]);

            //journalctl
            execlp("journalctl", "journalctl",
                   "-u", config->service_name,
                   "-p", config->priority,
                   "--since=-5s", NULL);
            perror("Error ejecutando execlp");
            exit(EXIT_FAILURE);
        } else if (pid > 0) { // Proceso padre
            close(pipe_fd[1]);

            // Leer la salida del hijo en el pupe
            ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';

                // Contar logs
                int log_count = 0;
                char *line = strtok(buffer, "\n");
                while (line != NULL) {
                    log_count++;
                    line = strtok(NULL, "\n");
                }

                // Actualizar logs
                pthread_mutex_lock(config->mutex);
                *config->logs = log_count;
                pthread_mutex_unlock(config->mutex);

                // mensaje al server
                char formatted_message[512];
                snprintf(formatted_message, sizeof(formatted_message),
                         "Service: %s | Priority: %s | Logs: %d\n",
                         config->service_name, config->priority, *config->logs);

                // escribiendo en el fifo
                fd = open(PIPE_NAME, O_WRONLY);
                if (fd != -1) {
                    write(fd, formatted_message, strlen(formatted_message));
                    close(fd);
                } else {
                    perror("Error abriendo el FIFO");
                }
            } else {
                perror("Error leyendo desde el pipe");
            }

            close(pipe_fd[0]); 
            wait(NULL);       
            sleep(config->update_interval); // intervalo de espera
        } else {
            perror("Error haciendo fork");
            pthread_exit(NULL);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || (argc - 2) % 2 != 0) {
        fprintf(stderr, "Usage: %s <service1> <priority1> ... <update_interval>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_services = (argc - 2) / 2;
    pthread_t threads[num_services];
    ServiceConfig service_data[num_services];
    int logs[num_services];
    pthread_mutex_t mutex;

    // Init mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Error inicializando mutex");
        exit(EXIT_FAILURE);
    }

    // Crear FIFO
    if (mkfifo(PIPE_NAME, 0666) == -1 && errno != EEXIST) {
        perror("Error creando FIFO");
        exit(EXIT_FAILURE);
    }

    // Init threads
    for (int i = 0; i < num_services; i++) {
       logs[i] = 0; // Init logs
        snprintf(service_data[i].service_name, sizeof(service_data[i].service_name), "%s", argv[1 + i * 2]);
        snprintf(service_data[i].priority, sizeof(service_data[i].priority), "%s", argv[2 + i * 2]);
        service_data[i].update_interval = atoi(argv[argc - 1]);
        service_data[i].logs = &logs[i];
        service_data[i].mutex = &mutex;
        pthread_create(&threads[i], NULL, monitor_service, &service_data[i]);
    }

    // Esperar hilos
    for (int i = 0; i < num_services; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destruir el mutex
    pthread_mutex_destroy(&mutex);

    return 0;
}


