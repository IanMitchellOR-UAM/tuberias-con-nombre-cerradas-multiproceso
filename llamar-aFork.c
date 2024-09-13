#include <stdio.h>
#include <stdlib.h>     // Para exit() y getenv()
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>     // Para read(), write(), close()
#include "utmp.h"

#define MAX 256
#define EQ(str1, str2) (strcmp((str1), (str2)) == 0)  // Macro para comparar dos cadenas

int fifo_12, fifo_21;
char nombre_fifo_12[MAX], nombre_fifo_21[MAX];
char mensaje[MAX];

void fin_de_transmision(int sig);

int main(int argc, char *argv[]) {
    int tty;
    char terminal[MAX], *logname;
    struct utmp *utmp;

    // Análisis de los argumentos de la línea de órdenes
    if (argc != 2) {
        fprintf(stderr, "Forma de uso: %s usuario\n", argv[0]);
        exit(-1);
    }

    // Lectura de nuestro nombre de usuario
    logname = getenv("LOGNAME");

    // Comprobación para que un usuario no se llame a sí mismo
    if (EQ(logname, argv[1])) {
        fprintf(stderr, "Comunicación con uno mismo no permitida\n");
        exit(0);
    }

    // Consultamos si el usuario ha iniciado una sesión
    while ((utmp = getutent()) != NULL && strncmp(utmp->ut_user, argv[1], sizeof(utmp->ut_user)) != 0);

    if (utmp == NULL) {
        printf("EL USUARIO %s NO HA INICIADO SESIÓN.\n", argv[1]);
        exit(0);
    }

    // Formación de los nombres de las tuberías
    sprintf(nombre_fifo_12, "/tmp/%s_%s", logname, argv[1]);
    sprintf(nombre_fifo_21, "/tmp/%s_%s", argv[1], logname);

    // Creación y apertura de las tuberías
    unlink(nombre_fifo_12);
    unlink(nombre_fifo_21);
    umask(~0666);

    if (mkfifo(nombre_fifo_12, 0666) == -1) {
        perror(nombre_fifo_12);
        exit(-1);
    }

    if (mkfifo(nombre_fifo_21, 0666) == -1) {
        perror(nombre_fifo_21);
        exit(-1);
    }

    // Apertura del terminal del usuario
    sprintf(terminal, "/dev/%s", utmp->ut_line);

    if ((tty = open(terminal, O_WRONLY)) == -1) {
        perror(terminal);
        exit(-1);
    }

    // Aviso al usuario con el que queremos comunicarnos
    sprintf(mensaje,
        "\n\t\tLLAMADA PROCEDENTE DEL USUARIO %s\07\07\07\n"
        "\t\tRESPONDER ESCRIBIENDO: responder-a %s\n\n",
        logname, logname);
    write(tty, mensaje, strlen(mensaje) + 1);
    close(tty);

    printf("Esperando respuesta...\n");

    // Apertura de las tuberías
    if ((fifo_12 = open(nombre_fifo_12, O_WRONLY)) == -1 || (fifo_21 = open(nombre_fifo_21, O_RDONLY)) == -1) {
        if (fifo_12 == -1)
            perror(nombre_fifo_12);
        else
            perror(nombre_fifo_21);
        exit(-1);
    }

    // Llamada atendida
    printf("LLAMADA ATENDIDA. \07\07\07\n");

    // Armamos el manejador de la señal SIGINT
    signal(SIGINT, fin_de_transmision);

    pid_t pid;
    if ((pid = fork()) == 0) {  // proceso hijo
        // Bucle de envío de mensajes
        do {
            printf("<== ");
            fgets(mensaje, sizeof(mensaje), stdin);
            write(fifo_12, mensaje, strlen(mensaje) + 1);
        } while (!EQ(mensaje, "corto\n"));
    } else {  // proceso padre
        // Bucle de recepción de mensajes
        do {
            printf("==> ");
            fflush(stdout);
            read(fifo_21, mensaje, MAX);
            printf("%s", mensaje);
        } while (!EQ(mensaje, "cambio\n") && !EQ(mensaje, "corto\n"));
    }

    // Fin de la transmisión
    printf("PUES SE ACABÓ LA TRANSMISIÓN.\n");
    close(fifo_12);
    close(fifo_21);
    unlink(nombre_fifo_12);  // Elimina la tubería
    unlink(nombre_fifo_21);  // Elimina la tubería
    return 0;
}

void fin_de_transmision(int sig) {
    sprintf(mensaje, "corto\n");
    write(fifo_12, mensaje, strlen(mensaje) + 1);
    printf("FIN DE TRANSMISIÓN.\n");
    close(fifo_12);
    close(fifo_21);
    unlink(nombre_fifo_12);  // Elimina la tubería
    unlink(nombre_fifo_21);  // Elimina la tubería
    exit(0);
}
