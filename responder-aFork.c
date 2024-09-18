#include <stdio.h>
#include <stdlib.h> // Para exit() y getenv()
#include <string.h> // Para strcmp()
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> // Para read(), write(), close()

#define MAX 256
#define EQ(str1, str2) (strcmp((str1), (str2)) == 0)  // Macro para comparar dos cadenas

int fifo_12, fifo_21;
char nombre_fifo_12[MAX], nombre_fifo_21[MAX];
char mensaje[MAX];

void fin_de_transmision(int sig);

int main(int argc, char *argv[]) {
    char *logname;

    // Análisis de los argumentos de la línea de órdenes
    if (argc != 2) {
        fprintf(stderr, "Forma de uso: %s usuario\n", argv[0]);
        exit(-1);
    }

    // Lectura del nombre del usuario
    logname = getenv("LOGNAME");

    // Comprobación para que un usuario no se responda a sí mismo
    if (EQ(logname, argv[1])) {
        fprintf(stderr, "Comunicación con uno mismo no permitida\n");
        exit(0);
    }

    // Formación del nombre de las tuberías de comunicación
    sprintf(nombre_fifo_12, "/tmp/%s_%s", argv[1], logname);
    sprintf(nombre_fifo_21, "/tmp/%s_%s", logname, argv[1]);

    // Apertura de las tuberías
    if ((fifo_12 = open(nombre_fifo_12, O_RDONLY)) == -1 || (fifo_21 = open(nombre_fifo_21, O_WRONLY)) == -1) {
        perror(nombre_fifo_21);
        exit(-1);
    }

    // Armamos el manejador de la señal SIGINT
    signal(SIGINT, fin_de_transmision);

    pid_t pid;
    if ((pid = fork()) == 0) {  // proceso hijo
        // Bucle de recepción de mensajes
        do {
            printf("==> ");
            fflush(stdout);
            read(fifo_12, mensaje, MAX);
            printf("%s", mensaje);
        } while (!EQ(mensaje, "corto\n"));
    } else if (pid > 0) {  // proceso padre
        // Bucle de envío de mensajes
        do {
            printf("<== ");
            fgets(mensaje, sizeof(mensaje), stdin);
            write(fifo_21, mensaje, strlen(mensaje) + 1);
        } while (!EQ(mensaje, "cambio\n") && !EQ(mensaje, "corto\n"));
    } else {
        perror("Error al crear el proceso hijo");
        exit(-1);
    }

    printf("FIN DE TRANSMISIÓN.\n");
    close(fifo_12);
    close(fifo_21);
    unlink(nombre_fifo_12);  // Elimina la tubería
    unlink(nombre_fifo_21);  // Elimina la tubería
    exit(0);
}

void fin_de_transmision(int sig) {
    sprintf(mensaje, "corto\n");
    write(fifo_21, mensaje, strlen(mensaje) + 1);
    printf("FIN DE TRANSMISIÓN.\n");
    close(fifo_12);
    close(fifo_21);
    unlink(nombre_fifo_12);  // Elimina la tubería
    unlink(nombre_fifo_21);  // Elimina la tubería
    exit(0);
}
