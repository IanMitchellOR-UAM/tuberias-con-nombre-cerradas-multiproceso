#include <stdio.h>
#include <stdlib.h>     // Para exit() y getenv()
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>     // Para read(), write(), close()
#include <sys/select.h> // Para select()
#include "utmp.h"

#define MAX 256
#define EQ(str1, str2) (strcmp((str1), (str2)) == 0)  // Macro para comparar dos cadenas

int fifo_12[MAX], fifo_21[MAX];
char nombre_fifo_12[MAX][MAX], nombre_fifo_21[MAX][MAX];
char mensaje[MAX];
int num_usuarios;

void fin_de_transmision(int sig);

int main(int argc, char *argv[]) {
    int tty;
    char terminal[MAX], *logname;
    struct utmp *utmp;

    if (argc < 2) {
        fprintf(stderr, "Forma de uso: %s usuario1 usuario2 ... usuarioN\n", argv[0]);
        exit(-1);
    }

    num_usuarios = argc - 1;
    logname = getenv("LOGNAME");

    for (int i = 1; i < argc; i++) {
        if (EQ(logname, argv[i])) {
            fprintf(stderr, "Comunicación con uno mismo no permitida\n");
            exit(0);
        }
    }

    for (int i = 1; i < argc; i++) {
        while ((utmp = getutent()) != NULL && strncmp(utmp->ut_user, argv[i], sizeof(utmp->ut_user)) != 0);

        if (utmp == NULL) {
            printf("EL USUARIO %s NO HA INICIADO SESIÓN.\n", argv[i]);
            exit(0);
        }

        sprintf(nombre_fifo_12[i-1], "/tmp/%s_%s", logname, argv[i]);
        sprintf(nombre_fifo_21[i-1], "/tmp/%s_%s", argv[i], logname);

        unlink(nombre_fifo_12[i-1]);
        unlink(nombre_fifo_21[i-1]);
        umask(~0666);

        if (mkfifo(nombre_fifo_12[i-1], 0666) == -1) {
            perror(nombre_fifo_12[i-1]);
            exit(-1);
        }

        if (mkfifo(nombre_fifo_21[i-1], 0666) == -1) {
            perror(nombre_fifo_21[i-1]);
            exit(-1);
        }

        sprintf(terminal, "/dev/%s", utmp->ut_line);
        if ((tty = open(terminal, O_WRONLY)) == -1) {
            perror(terminal);
            exit(-1);
        }

        sprintf(mensaje,
            "\n\t\tLLAMADA PROCEDENTE DEL USUARIO %s\07\07\07\n"
            "\t\tRESPONDER ESCRIBIENDO: responder-aFork %s\n\n",
            logname, logname);
        write(tty, mensaje, strlen(mensaje) + 1);
        close(tty);
    }

    printf("Esperando respuesta...\n");

    for (int i = 0; i < num_usuarios; i++) {
        if ((fifo_12[i] = open(nombre_fifo_12[i], O_WRONLY)) == -1 || (fifo_21[i] = open(nombre_fifo_21[i], O_RDONLY)) == -1) {
            perror("Error al abrir tuberías");
            exit(-1);
        }
    }

    printf("LLAMADA ATENDIDA. \07\07\07\n");

    signal(SIGINT, fin_de_transmision);

    pid_t pid;
    if ((pid = fork()) == 0) {  // Proceso hijo: Envío de mensajes
        do {
            printf("<== ");
            fgets(mensaje, sizeof(mensaje), stdin);

            if (!EQ(mensaje, "corto\n")) {
                for (int i = 0; i < num_usuarios; i++) {
                    write(fifo_12[i], mensaje, strlen(mensaje) + 1);
                }
            }
        } while (!EQ(mensaje, "corto\n"));
        
        // Al escribir "corto", también notificar al proceso padre
        kill(getppid(), SIGINT);
        exit(0);  // El proceso hijo termina aquí
    } else {  // Proceso padre: Recepción de mensajes
        fd_set readfds;
        int max_fd = 0;

        for (int i = 0; i < num_usuarios; i++) {
            if (fifo_21[i] > max_fd) {
                max_fd = fifo_21[i];
            }
        }

        do {
            FD_ZERO(&readfds);
            for (int i = 0; i < num_usuarios; i++) {
                FD_SET(fifo_21[i], &readfds);
            }

            select(max_fd + 1, &readfds, NULL, NULL, NULL);

            for (int i = 0; i < num_usuarios; i++) {
                if (FD_ISSET(fifo_21[i], &readfds)) {
                    read(fifo_21[i], mensaje, MAX);
                    if (!EQ(mensaje, "corto\n")) {
                        printf("Mensaje de %s: %s", argv[i+1], mensaje);
                    }
                }
            }
        } while (!EQ(mensaje, "corto\n"));
    }

    printf("PUES SE ACABÓ LA TRANSMISIÓN.\n");
    fin_de_transmision(0);
    return 0;
}

void fin_de_transmision(int sig) {
    sprintf(mensaje, "corto\n");
    for (int i = 0; i < num_usuarios; i++) {
        write(fifo_12[i], mensaje, strlen(mensaje) + 1);
        close(fifo_12[i]);
        close(fifo_21[i]);
        unlink(nombre_fifo_12[i]);
        unlink(nombre_fifo_21[i]);
    }
    printf("FIN DE TRANSMISIÓN.\n");
    exit(0);
}
