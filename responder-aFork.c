#include <stdio.h>
#include <stdlib.h> // Para exit() y getenv()
#include <string.h> // Para strcmp()
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> // Para read(), write(), close()
#include <sys/select.h> // Para select()
 
#define MAX 256
#define EQ(str1, str2) (strcmp((str1), (str2)) == 0) // Macro para comparar dos cadenas
 
int fifo_12[MAX], fifo_21[MAX];
char nombre_fifo_12[MAX][MAX], nombre_fifo_21[MAX][MAX];
char mensaje[MAX];
int num_usuarios;
 
void fin_de_transmision(int sig);
 
int main(int argc, char *argv[]) {
char *logname;
 
// Análisis de los argumentos de la línea de órdenes
if (argc < 2) {
fprintf(stderr, "Forma de uso: %s usuario1 usuario2 ... usuarioN\n", argv[0]);
exit(-1);
}
 
num_usuarios = argc - 1;
 
// Lectura del nombre del usuario
logname = getenv("LOGNAME");
 
// Comprobación para que un usuario no se responda a sí mismo
for (int i = 1; i < argc; i++) {
if (EQ(logname, argv[i])) {
fprintf(stderr, "Comunicación con uno mismo no permitida\n");
exit(0);
}
}
 
// Apertura de las tuberías
for (int i = 1; i < argc; i++) {
sprintf(nombre_fifo_12[i-1], "/tmp/%s_%s", argv[i], logname);
sprintf(nombre_fifo_21[i-1], "/tmp/%s_%s", logname, argv[i]);
 
if ((fifo_12[i-1] = open(nombre_fifo_12[i-1], O_RDONLY)) == -1 ||
(fifo_21[i-1] = open(nombre_fifo_21[i-1], O_WRONLY)) == -1) {
perror("Error al abrir tuberías");
exit(-1);
}
}
 
// Armamos el manejador de la señal SIGINT
signal(SIGINT, fin_de_transmision);
 
pid_t pid;
if ((pid = fork()) == 0) { // Proceso hijo: Recepción de mensajes
fd_set readfds;
int max_fd = 0;
 
// Configurar el conjunto de descriptores
for (int i = 0; i < num_usuarios; i++) {
if (fifo_12[i] > max_fd) {
max_fd = fifo_12[i];
}
}
 
do {
FD_ZERO(&readfds);
for (int i = 0; i < num_usuarios; i++) {
FD_SET(fifo_12[i], &readfds);
}
 
// Esperar mensajes
select(max_fd + 1, &readfds, NULL, NULL, NULL);
 
// Leer mensajes de los usuarios disponibles
for (int i = 0; i < num_usuarios; i++) {
if (FD_ISSET(fifo_12[i], &readfds)) {
read(fifo_12[i], mensaje, MAX);
printf("Mensaje de %s: %s", argv[i+1], mensaje);
}
}
} while (!EQ(mensaje, "corto\n") && !EQ(mensaje, "cambio\n"));
} else { // Proceso padre: Envío de mensajes
do {
printf("<== ");
fgets(mensaje, sizeof(mensaje), stdin);
for (int i = 0; i < num_usuarios; i++) {
write(fifo_21[i], mensaje, strlen(mensaje) + 1);
}
} while (!EQ(mensaje, "corto\n"));
}
 
fin_de_transmision(0);
return 0;
}
 
void fin_de_transmision(int sig) {
sprintf(mensaje, "corto\n");
for (int i = 0; i < num_usuarios; i++) {
write(fifo_21[i], mensaje, strlen(mensaje) + 1);
close(fifo_12[i]);
close(fifo_21[i]);
}
printf("FIN DE TRANSMISIÓN.\n");
exit(0);
} 
