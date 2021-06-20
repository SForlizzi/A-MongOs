/*
 * mi_ram_hq.c
 *
 *  Created on: 9 may. 2021
 *      Author: utnso
 */

#include "mi_ram_hq.h"
#include <comms/generales.h>


#define	IP config_get_string_value(config, "IP")
#define PUERTO config_get_string_value(config, "PUERTO")
#define TAMANIO_MEMORIA config_get_int_value(config, "TAMANIO_MEMORIA")
#define TAMANIO_PAGINA config_get_int_value(config, "TAMANIO_PAGINA")
#define ESQUEMA_MEMORIA config_get_string_value(config, "ESQUEMA_MEMORIA")
#define LIMIT_CONNECTIONS 10

t_log* logger;
t_config* config;

t_list* lista_tcb;
t_list* lista_pcb;	

char estado_tripulante[4] = {'N', 'R', 'E', 'B'};

void gestionar_tareas(t_archivo_tareas* archivo){
	//char** string_tareas = string_split(archivo_tareas->texto, "\n");
	//int cantidad_tareas = contar_palabras(string_tareas);
	int pid_patota = archivo->pid;
	size_t tamanio_tareas = archivo->largo_texto * sizeof(char);

	log_debug(logger, "Comienza la creación de PCB y persistencia de datos de la patota con PID: %d", pid_patota);
	if(strcmp(ESQUEMA_MEMORIA, "SEGMENTACION") == 0){
		tabla_segmentos* tabla = (tabla_segmentos*) buscar_tabla(pid_patota);
		if(tabla == NULL){ 
			tabla = crear_tabla_segmentos(pid_patota);
		}
		
		//Creamos segmento para tareas y lo guardamos en la tabla de la patota
		segmento* segmento_tareas = asignar_segmento(tamanio_tareas);
		void* puntero_a_tareas = memcpy(memoria_principal + segmento_tareas->base, archivo->texto, tamanio_tareas);
		tabla->segmento_tareas = segmento_tareas;
		
		//Creamos el PCB
		t_PCB* pcb = malloc(sizeof(t_PCB));
		pcb->PID = pid_patota;
		pcb->direccion_tareas = puntero_a_tareas;

		//Creamos el segmento para el PCB y lo guardamos en la tabla de la patota
		segmento* segmento_pcb = asignar_segmento(sizeof(t_PCB));
		memcpy(memoria_principal + segmento_pcb->base, pcb, sizeof(t_PCB));
		tabla->segmento_pcb = segmento_pcb;

	}else if(strcmp(ESQUEMA_MEMORIA, "PAGINACION") == 0){

	}else{
		log_error(logger, "Esquema de memoria desconocido");
		exit(EXIT_FAILURE);
	}
	log_debug(logger,"Se termino la creación de PCB y persistencia de datos de la patota con PID: %d", pid_patota);
}

void gestionar_tcb(t_TCB* tcb){
	int pid = tcb->TID / 10000;
	size_t tamanio_tcb = sizeof(t_TCB);
	log_debug(logger, "Comienza la creación del TCB y su persistencia del tripulante con TID: %d", tcb->TID);
	if(strcmp(ESQUEMA_MEMORIA, "SEGMENTACION") == 0){
		tabla_segmentos* tabla = (tabla_segmentos*) buscar_tabla(pid);
		if(tabla == NULL){ 
			tabla = crear_tabla_segmentos(pid);
		}
		
		//Creamos segmento para el tcb y lo guardamos en la tabla de la patota
		segmento* segmento_tcb = asignar_segmento(tamanio_tcb);
			// direccion donde está guardado el pcb
		void* puntero_a_pcb = memoria_principal + tabla->segmento_pcb->base; 
		tcb->puntero_a_pcb = puntero_a_pcb;
			// direccion donde está guardada la string de tareas, como estoy creando el tcb, la siguiente tarea va a ser la primera
		void* puntero_a_tareas = memoria_principal + tabla->segmento_tareas->base; 
		tcb->siguiente_instruccion = puntero_a_tareas;

		memcpy(memoria_principal + segmento_tcb->base, tcb, sizeof(t_TCB));

		bool criterio_orden(segmento* seg1, segmento* seg2){
			t_TCB* tcb1 = memoria_principal + seg1->base;
			t_TCB* tcb2 = memoria_principal + seg2->base;
			return tcb1->TID < tcb2->TID;
		}

		list_add_sorted(tabla->segmentos_tcb, segmento_tcb, criterio_orden);
	}else if(strcmp(ESQUEMA_MEMORIA, "PAGINACION") == 0){

	}else{
		log_error(logger, "Esquema de memoria desconocido");
		exit(EXIT_FAILURE);
	}
	log_debug(logger,"Se termino la creación de TCB y su persistencia del tripulante con TID: %d", tcb->TID);

}

void gestionar_pedido_tarea(int tid, int socket){
	// si necesitas el pid, es este
	int pid = tid / 10000;
	// para enviarme una tarea:
	// t_buffer* buffer_tarea = serializar_tarea(t_tarea una_tarea);
	// empaquetar_y_enviar(buffer_tarea, TAREA, socket);
	//
	// si sale mal por alguna razon o no hay tarea;
	// enviar_codigo(FALLO, socket);
}


int main(int argc, char** argv) {
	
	// Reinicio el log
	FILE* f = fopen("mi_ram_hq.log", "w");
    fclose(f);

	// Inicializar
	logger = log_create("mi_ram_hq.log", "MI_RAM_HQ", 1, LOG_LEVEL_DEBUG);
	config = config_create("mi_ram_hq.config");

	lista_tcb = list_create();
	lista_pcb = list_create();

	iniciar_memoria();
	test_gestionar_tcb();

	//iniciar_mapa(); TODO dibujar mapa inicial vacio
/*
	int socket_oyente = crear_socket_oyente(IP, PUERTO);
    args_escuchar args_miram;
	args_miram.socket_oyente = socket_oyente;

	pthread_t hilo_escucha;
	pthread_create(&hilo_escucha, NULL, (void*) proceso_handler, (void*) &args_miram);

	//pthread_detach(hilo_escucha);
	pthread_join(hilo_escucha, NULL);
	close(socket_oyente);
*/

	log_destroy(logger);
	config_destroy(config);

	return EXIT_SUCCESS;
}

void proceso_handler(void* args) {
	log_info(logger,"Se inicia el servidor multi-hilo");
	args_escuchar* p = malloc(sizeof(args_escuchar));
	p = args;
	int socket_escucha = p->socket_oyente;
	//int socket_escucha = (int) args; //Tema de testeos, no borrar

    int addrlen, socket_especifico;
    struct sockaddr_in address;

    addrlen = sizeof(address);

	// struct sockaddr_storage direccion_a_escuchar;
	// socklen_t tamanio_direccion;

	if (listen(socket_escucha, LIMIT_CONNECTIONS) == -1)
		log_error(logger,"Error al configurar recepcion de mensajes");

	while (1) {
		if ((socket_especifico = accept(socket_escucha, (struct sockaddr*) &address, (socklen_t *) &addrlen)) > 0) {
			// Maté la verificación
			log_info(logger, "Se conecta un nuevo proceso");

			hilo_tripulante* parametros = malloc(sizeof(hilo_tripulante));

			parametros->socket = socket_especifico;
			//parametros->ip_cliente = inet_ntoa(address.sin_addr);
			//parametros->puerto_cliente = ntohs(address.sin_port);

			pthread_t un_hilo_tripulante;

			pthread_create(&un_hilo_tripulante, NULL, (void*) atender_clientes, (void *) parametros);

			pthread_detach(un_hilo_tripulante);
		}
	}
}

void atender_clientes(void* param) {
	hilo_tripulante* parametros = param;

	int flag = 1;
	log_info(logger, "Atendiendo. %i\n", parametros->socket);

	while(flag) {
		t_estructura* mensaje_recibido = recepcion_y_deserializacion(parametros->socket);

		//sleep(1); //para que no se rompa en casos de bug o tiempos de espera

		switch(mensaje_recibido->codigo_operacion) {

			case ARCHIVO_TAREAS:
				log_info(logger, "Recibido contenido del archivo\n");
				printf("\tpid:%i. \n\tlongitud; %i. \n%s\n", mensaje_recibido->archivo_tareas->pid, mensaje_recibido->archivo_tareas->largo_texto, mensaje_recibido->archivo_tareas->texto);
				gestionar_tareas(mensaje_recibido->archivo_tareas);
				sleep(1);
				break;

			case MENSAJE:
				log_info(logger, "Mensaje recibido\n");
				break;

			case PEDIR_TAREA:
				log_info(logger, "Pedido de tarea recibido\n");
				log_info(logger, "Tripulante: %i\n", mensaje_recibido->tid_condenado->tid);
				// TODO: GABITO Y JULIA
				// gestionar_pedido_tarea(mensaje_recibido->tid_condenado->tid, parametros->socket);
				break;

			case RECIBIR_PCB:
				//log_info(logger, "Recibo una pcb\n");
				//almacenar_pcb(mensaje_recibido); //TODO en un futuro, capaz no podamos recibir el PCB por quedarnos sin memoria
				free(mensaje_recibido->pcb);
				break;

			case RECIBIR_TCB:
				log_info(logger, "Recibo una tcb\n");
				log_info(logger, "Tripulante %i, estado: %c, pos: %i %i, puntero_pcb: %i, sig_ins %i\n", (int) mensaje_recibido->tcb->TID, (char) mensaje_recibido->tcb->estado_tripulante, (int) mensaje_recibido->tcb->coord_x, (int) mensaje_recibido->tcb->coord_y, (int) mensaje_recibido->tcb->puntero_a_pcb, (int) mensaje_recibido->tcb->siguiente_instruccion);
				gestionar_tcb(mensaje_recibido->tcb);
				free(mensaje_recibido->tcb);
				//printf("Tripulante 1 pos: %c %c\n", (int) mensaje_recibido->tcb->coord_x, (int) mensaje_recibido->tcb->coord_y);
				break;

			case T_SIGKILL:
				log_info(logger, "Expulsar Tripulante.\n");
				// TODO: GABITO Y JULIA
				// verifica si existe
				// si existe mandame un enviar_codigo(EXITO, parametros->socket);
				// si no existe, mandame un enviar_codigo(FALLO, parametros->socket);
				log_info(logger, "%i -KILLED", mensaje_recibido->tid_condenado->tid);
				enviar_codigo(EXITO, parametros->socket);
				break;

			case LISTAR_POR_PID:
				log_info(logger, "Recibido pedido de tripulantes.\n");
				// TODO: GABITO Y JULIA
				// consultarme que hacer aca
				enviar_codigo(EXITO, parametros->socket);
				break;

			case DESCONEXION:
				log_info(logger, "Se desconecto un cliente.\n");
				flag = 0;
				// close(parametros->socket);
				break;

			default:
				log_info(logger, "Se recibio un codigo invalido.\n");
				printf("El codigo es %d\n", mensaje_recibido->codigo_operacion);
				break;
		}
		free(mensaje_recibido);
	}

}


t_PCB* crear_pcb(char* path){
	t_PCB* pcb = malloc(sizeof(t_PCB));
	pcb -> PID = 1;
	pcb -> direccion_tareas = (uint32_t) path;
	return pcb;
}

t_TCB crear_tcb(t_PCB* pcb, int tid, char* posicion){
	t_TCB tcb;
	tcb.TID = tid;
	tcb.estado_tripulante = estado_tripulante[NEW];
	tcb.coord_x = posicion[0];
	tcb.coord_y = posicion[2];
	tcb.siguiente_instruccion = 5; //TODO
	tcb.puntero_a_pcb = 7;
	return tcb;
}

indice_tabla* crear_indice(int pid, void* tabla){
	log_info(logger,"Se crea indice para la tabla de la patota %d",pid);
	indice_tabla* nuevo_indice = malloc(sizeof(indice_tabla));
	nuevo_indice->pid = pid;
	nuevo_indice->tabla = tabla;
	return nuevo_indice;
}

// void liberar_pagina(int base){
//     for(int i = 0; i<list_size(paginas);i++){
//         pagina* x = list_get(paginas, i);
//         if(x->base == base) {
//             x->libre = true;
//             log_info(logger, "Se elimina la página con base %d", x->base);
//         }
//     }
//     ordenar_segmentos();
// }


pagina* crear_pagina(int base, bool libre){
    pagina* nueva_pagina = malloc(sizeof(pagina));
    nueva_pagina->base = base;
    nueva_pagina->libre = libre;

    return nueva_pagina;
}


tabla_paginas* crear_tabla_paginas(uint32_t pid){
	tabla_paginas* nueva_tabla = malloc(sizeof(tabla_paginas));
	nueva_tabla->paginas = list_create();
	list_add(indices,crear_indice(pid, (void*) nueva_tabla));
	return nueva_tabla;
}

void iniciar_memoria(){
	memoria_principal = malloc(TAMANIO_MEMORIA);
	indices = list_create();
	if(strcmp(ESQUEMA_MEMORIA,"SEGMENTACION")==0){
		log_info(logger,"Se inicia memoria con esquema se SEGMENTACION");
		segmentos = list_create();
		segmento* segmento_principal = crear_segmento(0,TAMANIO_MEMORIA,true);
		list_add(segmentos,segmento_principal);
	}else if(strcmp(ESQUEMA_MEMORIA,"PAGINACION")==0){
		log_info(logger,"Se inicia memoria con esquema de PAGINACION");
		paginas = list_create();

		int cantidad_paginas = TAMANIO_MEMORIA/TAMANIO_PAGINA;
		
		for(int i=0; i < cantidad_paginas ; i++) {
			pagina* pagina = crear_pagina(TAMANIO_PAGINA * i, true);

			list_add(paginas,pagina);
		}

	}else{
		log_error(logger,"Esquema de memoria desconocido");
		exit(EXIT_FAILURE);
	}
}

void* buscar_tabla(int pid){
	bool criterio(void* un_indice){
		indice_tabla* indice = (indice_tabla*) un_indice;
		return indice->pid == pid;
	}
	
	log_debug(logger,"Se comienza la busqueda de tabla de pid: %d",pid);

	indice_tabla* indice = (indice_tabla*) list_find(indices, criterio);
	if(indice != NULL){
		log_debug(logger,"Tabla encontrada, pid: %d",indice->pid);
		return indice->tabla;
	}
	log_debug(logger,"Tabla no encontrada");
	return NULL;
}

t_TCB* buscar_tcb_por_tid(int tid){
	int pid = tid / 10000;
	tabla_segmentos* tabla = (tabla_segmentos*) buscar_tabla(pid);
	// busco en la lista de tcbs en la posicion [tid - 10000 - 1] porque al guardar los tcbs los guardo ordenados por tids
    segmento* segmento_tcb = (segmento*) list_get(tabla->segmentos_tcb, tid - 10000 - 1); // resto 1 porque el primer TCB siempre tiene tid #0001
    t_TCB* tcb_recuperado = memoria_principal + segmento_tcb->base;						  // si no resto 1 me agarra el tcb[1] enves del tcb[0]
	return tcb_recuperado;
}

t_list* buscar_tcbs_por_pid(int pid){
	tabla_segmentos* tabla = (tabla_segmentos*) buscar_tabla(pid);
	void* transformer(void* un_segmento){
		segmento* segmento_tcb = (segmento*) un_segmento;
		return memoria_principal + segmento_tcb->base;
	}
	return list_map(tabla->segmentos_tcb, transformer);
}

