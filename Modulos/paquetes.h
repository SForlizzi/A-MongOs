#ifndef PAQUETES_H_
#define PAQUETES_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "estructuras.h"
#include "socketes.h"

typedef struct {
    uint32_t tamanio_estructura; 
    void* estructura;
} t_buffer;

typedef struct {
    uint8_t codigo_operacion;
    t_buffer* buffer;
} t_paquete;

t_buffer serializar_tripulante(t_tripulante tripulante);
t_buffer serializar_tarea(t_tarea tarea);
t_buffer serializar_sabotaje();
void empaquetar(t_buffer buffer, int codigo_operacion, int socket_receptor);
estructura_t* recepcion_y_deserializacion(int socket_receptor);
t_tripulante* desserializar_tripulante(t_buffer* buffer);
t_tarea* desserializar_tarea(t_buffer* buffer);

#endif