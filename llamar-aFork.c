#include <stdio.h>
#include <stdlib.h>     
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utmp.h"
 
#define MAX 256
#define EQ(str1, str2) (strcmp((str1), (str2)) == 0)  // Macro para comparar dos cadenas de caracteres
 
// Descriptores de fichero de las tuberías con nombre mediante las cuales vamos a comunicarnos
int fifo_12, fifo_21;
char nombre_fifo_12[MAX], nombre_fifo_21[MAX];
char mensaje[MAX];  // Array para leer los mensajes
 
void fin_de_transmision(int sig);
void ter_proc();
 
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
 
    // Formación de los nombres de las tuberías de comunicación
    sprintf(nombre_fifo_12, "/tmp/%s_%s", logname, argv[1]);
    sprintf(nombre_fifo_21, "/tmp/%s_%s", argv[1], logname);
 
    // Creación y apertura de las tuberías
    unlink(nombre_fifo_12);
    unlink(nombre_fifo_21);
    umask(0);  // Aseguramos que las tuberías tengan permisos completos (0666)
 
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
    pid_t padre = getpid();
    if ((pid = fork()) == 0) { // Proceso hijo: envío de mensajes
	signal(SIGTERM, ter_proc);
	do {
            printf("<== ");
	    fflush(stdin);
            fgets(mensaje, sizeof(mensaje), stdin);
            write(fifo_12, mensaje, strlen(mensaje) + 1);
	    if(EQ(mensaje, "corto\n")) kill(padre, SIGTERM);
        } while (!EQ(mensaje, "corto\n"));
    } else {  // Proceso padre: recepción de mensajes
        signal(SIGTERM, ter_proc);
	do {
            printf("==> ");
            fflush(stdout);
            if (read(fifo_21, mensaje, MAX) <= 0) {
                perror("Error en la lectura");
                exit(-1);
            }
	    if(EQ(mensaje, "corto\n")){
	    	kill(pid, SIGTERM);
	  	break;
	    }
            printf("%s", mensaje);
        } while (!EQ(mensaje, "corto\n"));
    }
 
    // Fin de la transmisión
    printf("PUES SE ACABO LA TRANSMISIÓN.\n");
    close(fifo_12);
    close(fifo_21);
    unlink(nombre_fifo_12);
    unlink(nombre_fifo_21);
 
    return 0;
}
 
/***
fin_de_transmision: rutina de tratamiento de la señal SIGINT. Al pulsar Ctrl+C
entramos en esta rutina, que se encarga de enviar el mensaje "corto\n" al usuario
con el que estamos hablando.
***/
 
void fin_de_transmision(int sig) {
    sprintf(mensaje, "corto\n");
    write(fifo_12, mensaje, strlen(mensaje) + 1);
    printf("FIN DE TRANSMISIÓN.\n");
    close(fifo_12);
    close(fifo_21);
    exit(0);
}
 
void ter_proc(){
    sprintf(mensaje, "corto\n");
    write(fifo_12, mensaje, strlen(mensaje) + 1);
    close(fifo_12);
    close(fifo_21);
    exit(0);
}
