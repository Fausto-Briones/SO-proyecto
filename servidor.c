#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define PIPE_NAME "log_pipe"
#define THRESHOLD 10

// Twilio credentials
#define ACCOUNT_SID "AC982024f97b245116807119c4c4493f48"
#define AUTH_TOKEN "405c0472bbb16ea3f6df5cea7e1d24e5"
#define TWILIO_NUMBER "whatsapp:+14155238886" // Twilio's WhatsApp sandbox number
#define RECIPIENT_NUMBER "whatsapp:+593958687031"

pthread_mutex_t mutex;

void send_whatsapp_alert(const char *message) {
    char command[1024];

    snprintf(command, sizeof(command),
             "curl -X POST 'https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json' "
             "--data-urlencode 'To=%s' "
             "--data-urlencode 'From=%s' "
             "--data-urlencode 'Body=%s' "
             "-u '%s:%s'",
             ACCOUNT_SID, RECIPIENT_NUMBER, TWILIO_NUMBER, message, ACCOUNT_SID, AUTH_TOKEN);

    system(command);
}

void *process_logs(void *arg) {
    int fd;
    char buffer[512];

    fd = open(PIPE_NAME, O_RDONLY);
    if (fd == -1) {
        perror("Error opening FIFO for reading");
        pthread_exit(NULL);
    }

    while (1) {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';

            // dashboard
            pthread_mutex_lock(&mutex);
            printf("\033[H\033[J"); // Clear screen
            printf("----- Log Dashboard -----\n");
            printf("%s\n", buffer);
            pthread_mutex_unlock(&mutex);

            //alertas
            char *logs_ptr = strstr(buffer, "Logs: ");
            if (logs_ptr != NULL) {
                int log_count = atoi(logs_ptr + 6);
                if (log_count > THRESHOLD) {
                    printf("ALERTA!!\n");
                    char alert_message[256];
                    snprintf(alert_message, sizeof(alert_message),
                             "Alerta: Logs excedidos. Detalles: %s", buffer);
                    send_whatsapp_alert(alert_message);
                }
            }
        } else if (bytes_read == 0) {
      
            sleep(1);
        } else {
            perror("Error reading from FIFO");
        }
    }

    close(fd);
    return NULL;
}

int main() {
    pthread_t log_thread;

    // Init mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    // Create FIFO
    if (mkfifo(PIPE_NAME, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO");
        exit(EXIT_FAILURE);
    }

    // process logs
    if (pthread_create(&log_thread, NULL, process_logs, NULL) != 0) {
        perror("Error creating thread");
        exit(EXIT_FAILURE);
    }

    // Wait thread
    pthread_join(log_thread, NULL);

    // Destroy mutex
    pthread_mutex_destroy(&mutex);

    // Remove FIFO on exit
    unlink(PIPE_NAME);

    return 0;
}
