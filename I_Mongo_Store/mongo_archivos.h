#ifndef I_MONGO_ARCHIVOS_H_
#define I_MONGO_ARCHIVOS_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/bitarray.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <string.h>
#include <comms/estructuras.h>
#include <comms/paquetes.h>
#include <comms/socketes.h>
#include <pthread.h>
#include <readline/readline.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define TAMANIO_BLOQUE 64
#define CANTIDAD_BLOQUES 64 

/* ESTRUCTURAS PROPIAS */

typedef struct{

    char* punto_montaje;
    int puerto;
    int tiempo_sincronizacion;
    char** posiciones_sabotaje;

} config_mongo_t;

typedef struct {

    int socket_oyente;

} args_escuchar_mongo;

typedef struct {

    t_TCB* tripulante;
    FILE* bitacora_asociada;

} t_bitacora;

typedef struct {

    FILE* oxigeno;
    char* path_oxigeno;
    FILE* comida;
    char* path_comida;
    FILE* basura;
    char* path_basura;
    FILE* superbloque;
    FILE* blocks;

} t_archivos;

/* SEMAFOROS PROPIOS */

pthread_mutex_t mutex_oxigeno;
pthread_mutex_t mutex_comida;
pthread_mutex_t mutex_basura;

void inicializar_archivos(char* path_files);
void inicializar_archivos_preexistentes(char* path_files);
void alterar(int codigo_archivo, int cantidad);
int asignar_primer_bloque_libre(uint32_t* lista_bloques, uint32_t cant_bloques, int cantidad_deseada; char tipo);
void agregar(FILE* archivo, int cantidad, char tipo);
void quitar(FILE* archivo, char* path, int cantidad, char tipo);
char* conseguir_tipo(char tipo);
FILE* conseguir_archivo_char(char tipo);
FILE* conseguir_archivo(int codigo);
char conseguir_char(int codigo);
int max(int a, int b);

extern t_log* logger_mongo;
extern t_config* config_mongo;
extern t_archivos archivos;
extern t_list* bitacoras;
extern int* posiciciones_bitacora;

#endif
