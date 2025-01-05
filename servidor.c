#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <curl/curl.h>

#define SERVER_PORT 5000 // port
#define BUFFER_SIZE 1024
#define CPU_THRESHOLD 80  // Uso de CPU
#define MEM_THRESHOLD 75  // Uso de memoria

pthread_mutex_t mutex;

const char *TWILIO_ACCOUNT_SID = "AC982024f97b245116807119c4c4493f48";
const char *TWILIO_AUTH_TOKEN = "405c0472bbb16ea3f6df5cea7e1d24e5";
const char *TWILIO_FROM = "+17129381952"; // Twilio sandbox
const char *TWILIO_TO = "+593958687031"; //mi numero


void send_whatsapp_alert(const char *message) {
    CURL *curl;
    CURLcode res;
    char url[256];
    char post_fields[1024];

    //url
    snprintf(url, sizeof(url), "https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json", TWILIO_ACCOUNT_SID);

    // POST
    snprintf(post_fields, sizeof(post_fields), "To=%s&From=%s&Body=%s", TWILIO_TO, TWILIO_FROM, message);

    //cURL
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);

        
        curl_easy_setopt(curl, CURLOPT_USERNAME, TWILIO_ACCOUNT_SID);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, TWILIO_AUTH_TOKEN);

        //request
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "cURL error: %s\n", curl_easy_strerror(res));
        }

        // Clean up
        curl_easy_cleanup(curl);
    }
}

void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    free(client_socket);
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            // cliente desconectado
            close(sock);
            pthread_exit(NULL);
        }

        buffer[bytes_received] = '\0';

        // mutex lock
        pthread_mutex_lock(&mutex);

        printf("----- New Metrics -----\n");
        printf("%s\n", buffer);

        // uso de memoria y cpu
        char *cpu_ptr = strstr(buffer, "CPU: ");
        char *mem_ptr = strstr(buffer, "Mem: ");

        int cpu_usage = 0, mem_usage = 0;
        if (cpu_ptr != NULL) {
            cpu_usage = atoi(cpu_ptr + 5);
        }
        if (mem_ptr != NULL) {
            mem_usage = atoi(mem_ptr + 5);
        }

        // Envia alertas
        if (cpu_usage > CPU_THRESHOLD) {
            char alert_message[256];
            snprintf(alert_message, sizeof(alert_message), "ALERTA: Uso de CPU es %d%%, excede el limite de  %d%%.", cpu_usage, CPU_THRESHOLD);
            send_whatsapp_alert(alert_message);
            printf("Alerta de CPU enviada: %s\n", alert_message);
        }

        if (mem_usage > MEM_THRESHOLD) {
            char alert_message[256];
            snprintf(alert_message, sizeof(alert_message), "ALERTA: Uso de memoria de:  %d%%, excede el limite de  %d%%.", mem_usage, MEM_THRESHOLD);
            send_whatsapp_alert(alert_message);
            printf("Alerta de memoria enviada: %s\n", alert_message);
        }

        pthread_mutex_unlock(&mutex);
    }

    close(sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t threads[1024];
    int client_count = 0;

    // inicializar mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Error initializing mutex");
        exit(EXIT_FAILURE);
    }

    // Crear el socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Direccion del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_sock, 1024) == -1) {
        perror("Error listening on socket");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("Error accepting client connection");
            continue;
        }

        printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Manejo de la cantidad de conexiones
        int *client_sock_ptr = malloc(sizeof(int));
        if (client_sock_ptr == NULL) {
            perror("Error allocating memory");
            close(client_sock);
            continue;
        }

        *client_sock_ptr = client_sock;

        // thread de cliente
        if (pthread_create(&threads[client_count++], NULL, handle_client, client_sock_ptr) != 0) {
            perror("Error creating thread");
            free(client_sock_ptr);
            close(client_sock);
            continue;
        }

        
        pthread_detach(threads[client_count - 1]);
    }

    // Cleanup
    close(server_sock);
    pthread_mutex_destroy(&mutex);

    return 0;
}