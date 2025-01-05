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
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>
#include <netdb.h>
#include <sys/utsname.h>

#define UPDATE_INTERVAL 5
#define SERVER_IP "192.168.100.4" // IP actual
#define SERVER_PORT 5000 // Puerto del server

typedef struct {
    char client_id[256];
    int update_interval;
    int *metrics;
    pthread_mutex_t *mutex;
    char server_ip[16];
    int server_port;
} ServiceConfig;

void collect_metrics(int *metrics) {
    struct sysinfo info;
    struct statvfs disk_info;

    // cpu load
    sysinfo(&info);
    metrics[0] = (int)(info.loads[0] / 65536.0 * 100); // Load average

    // uso de memoria
    metrics[1] = (int)(((info.totalram - info.freeram) / (double)info.totalram) * 100);

    // uso de disco
    if (statvfs("/", &disk_info) == 0) {
        metrics[2] = (int)((disk_info.f_blocks - disk_info.f_bfree) / (double)disk_info.f_blocks * 100);
    } else {
        metrics[2] = -1; //Error recuperando datos
    }

    // Procesos activos
    metrics[3] = (int)info.procs;

    // Uptime del sistema (minutos)
    metrics[4] = (int)(info.uptime / 60);

    // Porcentaje de uso de memoria swap
    metrics[5] = (int)(((info.totalswap - info.freeswap) / (double)info.totalswap) * 100);
}

void monitor_service(ServiceConfig *config) {
    char buffer[1024];
    int sock;
    struct sockaddr_in server_addr;

    while (1) {
        // obtener metricas
        collect_metrics(config->metrics);

        //mutex lock
        pthread_mutex_lock(config->mutex);

        // Mensaje
        snprintf(buffer, sizeof(buffer),
                 "Client: %s | CPU: %d%% | Mem: %d%% | Disk: %d%% | Procs: %d | Uptime: %d min | Swap: %d%%\n",
                 config->client_id, config->metrics[0], config->metrics[1], config->metrics[2],
                 config->metrics[3], config->metrics[4], config->metrics[5]);

        // Enviando al servidor
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            perror("Error creating socket");
            pthread_mutex_unlock(config->mutex);
            sleep(config->update_interval);
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config->server_port);
        inet_pton(AF_INET, config->server_ip, &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error connecting to server");
            close(sock);
            pthread_mutex_unlock(config->mutex);
            sleep(config->update_interval);
            continue;
        }

        send(sock, buffer, strlen(buffer), 0);
        close(sock);

        // Desbloquear mutex despues de enviar al servidor
        pthread_mutex_unlock(config->mutex);

        sleep(config->update_interval); // wait
    }
}

int main() {
    ServiceConfig service_data;
    int metrics[6];
    pthread_mutex_t mutex;

    // inicializar mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Error initializing mutex");
        exit(EXIT_FAILURE);
    }

    // CLiente
    struct utsname uts;
    if (uname(&uts) == 0) {
        snprintf(service_data.client_id, sizeof(service_data.client_id), "%s", uts.nodename);
    } else {
        perror("Error retrieving hostname");
        snprintf(service_data.client_id, sizeof(service_data.client_id), "Unknown");
    }

    service_data.update_interval = UPDATE_INTERVAL;
    service_data.metrics = metrics;
    service_data.mutex = &mutex;
    snprintf(service_data.server_ip, sizeof(service_data.server_ip), "%s", SERVER_IP);
    service_data.server_port = SERVER_PORT;

    pid_t pid = fork();
    if (pid == 0) { 
        monitor_service(&service_data);
        exit(0);
    } else if (pid < 0) {
        perror("Error creating process");
        exit(EXIT_FAILURE);
    }

    // Wait for child process
    wait(NULL);

    // Destroy mutex
    pthread_mutex_destroy(&mutex);

    return 0;
}