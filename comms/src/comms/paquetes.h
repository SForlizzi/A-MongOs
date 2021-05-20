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

    codigo_operacion codigo_operacion;
    t_buffer* buffer;

} t_paquete;

t_buffer* serializar_tripulante(t_tripulante tripulante);
t_buffer* serializar_tarea(t_tarea tarea);
t_buffer* serializar_vacio();
t_paquete* crear_paquete_de_codigo(codigo_operacion codigo);
void empaquetar_y_enviar(t_buffer* buffer, int codigo_operacion, int socket_receptor);
void enviar_codigo(t_paquete* paquete, int socket_receptor);
t_estructura* recepcion_y_deserializacion(int socket_receptor);
t_tripulante* desserializar_tripulante(t_buffer* buffer);
t_tarea* desserializar_tarea(t_buffer* buffer);
t_paquete* crear_paquete(op_code codigo);
void eliminar_paquete(t_paquete* paquete);


#endif
