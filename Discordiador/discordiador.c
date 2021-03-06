/*
 ============================================================================
 Name        : Discordiador.c
 Author      : Rey de fuego
 Version     : 1
 Copyright   : Mala yuyu
 Description : El discordiador
 ============================================================================
 */

#define IP_MI_RAM_HQ config_get_string_value(config, "IP_MI_RAM_HQ")
#define PUERTO_MI_RAM_HQ config_get_string_value(config, "PUERTO_MI_RAM_HQ")
#define IP_I_MONGO_STORE config_get_string_value(config, "IP_I_MONGO_STORE")
#define PUERTO_I_MONGO_STORE config_get_string_value(config, "PUERTO_I_MONGO_STORE")
#define ALGORITMO config_get_string_value(config, "ALGORITMO")
#define GRADO_MULTITAREA config_get_int_value(config, "GRADO_MULTITAREA")
#define QUANTUM config_get_int_value(config, "QUANTUM")
#define RETARDO_CICLO_CPU config_get_int_value(config, "RETARDO_CICLO_CPU")
#define DURACION_SABOTAJE config_get_int_value(config, "DURACION_SABOTAJE")
#define DIR_TAREAS "tareas/"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#include "discordiador.h"
#include <semaphore.h>

// Variables globales
t_config* config;
t_log* logger;
int socket_a_mi_ram_hq;
int socket_a_mongo_store;

// Listas, colas, semaforos
t_list* lista_pids;
t_list* lista_patotas;
t_list* lista_tripulantes;
t_list* lista_tripulantes_new;
t_list* lista_tripulantes_exec;

t_queue* cola_tripulantes_ready;
t_queue* cola_tripulantes_block;
t_queue* cola_tripulantes_block_emergencia;

pthread_mutex_t sem_lista_tripulantes;
pthread_mutex_t sem_lista_new;
pthread_mutex_t sem_cola_ready;
pthread_mutex_t sem_lista_exec;
pthread_mutex_t sem_cola_block;
pthread_mutex_t sem_cola_block_emergencia;

// Variables de discordiador
char estado_tripulante[6] = {'N', 'R', 'E', 'B', 'F', 'V'};
int estamos_en_peligro = 0; // variable de sabotaje
int planificacion_activa = 0;
int testeo = DISCORDIADOR;

void notificar_fin_de_tarea(t_tripulante* un_tripulante, int socket_mongo){

	t_TCB tcb_aux = tripulante_a_tcb(*un_tripulante);
	t_buffer* buffer_t = serializar_tcb(tcb_aux);
	empaquetar_y_enviar(buffer_t, FIN_TAREA, socket_mongo);

	t_buffer* tarea_buffer = serializar_tarea(un_tripulante->tarea);
	empaquetar_y_enviar(tarea_buffer, TAREA, socket_mongo);

	log_debug(logger, "Notifico a Mongo que termino una tarea");
}

void notificar_inicio_de_tarea(t_tripulante* un_tripulante, int socket_mongo){

	t_TCB tcb_aux = tripulante_a_tcb(*un_tripulante);
	t_buffer* buffer_t = serializar_tcb(tcb_aux);
	empaquetar_y_enviar(buffer_t, INICIO_TAREA, socket_mongo);

	t_buffer* tarea_buffer = serializar_tarea(un_tripulante->tarea);
	empaquetar_y_enviar(tarea_buffer, TAREA, socket_mongo);

	log_debug(logger, "Notifico a Mongo que inicio una tarea");
}

void notificar_movimiento(t_tripulante* un_tripulante, int socket_mongo, int socket_ram){

	t_TCB tcb_aux = tripulante_a_tcb(*un_tripulante);
	t_buffer* buffer_t = serializar_tcb(tcb_aux);
	empaquetar_y_enviar(buffer_t, MOVIMIENTO, socket_mongo);

	log_debug(logger, "Notifico a Mongo que camino");
	actualizar_tripulante(un_tripulante, socket_ram);
}

void notificar_inicio_sabotaje(t_tripulante* un_tripulante, int socket_mongo){

	t_TCB tcb_aux = tripulante_a_tcb(*un_tripulante);
	t_buffer* buffer_t = serializar_tcb(tcb_aux);
	empaquetar_y_enviar(buffer_t, CORRE_SABOTAJE, socket_mongo);

	log_debug(logger, "Notifico a mongo que corro por mi vida");
}

void notificar_fin_sabotaje(t_tripulante* un_tripulante, int socket_mongo){

	t_TCB tcb_aux = tripulante_a_tcb(*un_tripulante);
	t_buffer* buffer_t = serializar_tcb(tcb_aux);
	empaquetar_y_enviar(buffer_t, RESUELVE_SABOTAJE, socket_mongo);

	log_debug(logger, "Notifico a Mongo que soy un heroe");
}

void ejecutar_tarea(char* path_archivo);

sem_t sistema_activo;
sem_t entrada_salida_libre;

int main() {
    if(testeo != DISCORDIADOR)
        correr_tests(testeo);
    else {

    FILE* reiniciar_logger = fopen("discordiador.log", "w");
    fclose(reiniciar_logger);

    logger = log_create("discordiador.log", "discordiador", true, 0);
    config = config_create("discordiador.config");

    iniciar_listas();
    iniciar_colas();
    iniciar_semaforos();

    sem_init(&sistema_activo, 0, 0);

    socket_a_mi_ram_hq = crear_socket_cliente(IP_MI_RAM_HQ, PUERTO_MI_RAM_HQ);
    socket_a_mongo_store = crear_socket_cliente(IP_I_MONGO_STORE, PUERTO_I_MONGO_STORE);

    //iniciar_patota("INICIAR_PATOTA 1 ES3_Patota1.txt");
    //iniciar_planificacion();
    /*for(int i = 0; i<1; i++){
        iniciar_patota("INICIAR_PATOTA 3 ES3_Patota1.txt 9|9 0|0 5|5");
        sleep(1);
        iniciar_patota("INICIAR_PATOTA 3 ES3_Patota2.txt 4|0 2|6 8|2");
        sleep(1);
        iniciar_patota("INICIAR_PATOTA 3 ES3_Patota3.txt 2|3 5|8 5|3");
        sleep(1);
        iniciar_patota("INICIAR_PATOTA 3 ES3_Patota4.txt 0|9 4|4 9|0");
        sleep(1);
        iniciar_patota("INICIAR_PATOTA 3 ES3_Patota5.txt 0|2 9|6 3|5");
        sleep(1);
    }*/
    /*for(int i = 0; i<2; i++){
        ejecutar_tarea("estabilidad_general.txt");
    }*/

    pthread_t hiloConsola;
	pthread_create(&hiloConsola, NULL, (void*)leer_consola, NULL);
	pthread_detach(hiloConsola);

    pthread_t sabotaje;
    pthread_create(&sabotaje, NULL, (void*) guardian_mongo, NULL);
    pthread_detach(sabotaje);

    sem_wait(&sistema_activo);

    sem_destroy(&sistema_activo);
	planificacion_activa = 0;
    enviar_codigo(DESCONEXION, socket_a_mongo_store);

    liberar_tripulantes();
    log_warning(logger, "Apagando sistema, espere por favor.");
    sleep(5);
    liberar_listas();
    liberar_colas();
    liberar_semaforos();

    close(socket_a_mi_ram_hq);
    close(socket_a_mongo_store);

    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;

    }
}

void ejecutar_tarea(char* path_archivo){
	char* path_real = malloc(strlen(path_archivo) + strlen(DIR_TAREAS) + 1);
	strcpy(path_real, DIR_TAREAS);
	strcat(path_real, path_archivo);

	char* texto = leer_archivo_entero(path_real);
	char** texto_split = string_split(texto, "\n");

	for(int i = 0; i < contar_palabras(texto_split) ; i++){
		char** texto_nuevo = string_split(texto_split[i], " ");
		if(!strcmp(texto_nuevo[0], "INICIAR_PATOTA")){
			iniciar_patota(texto_split[i]);
		}
	}
}


int correr_tests(int enumerado) {
    switch(enumerado) {
    case TEST_SERIALIZACION:
        return CUmain_serializacion();
        break;

    case TEST_ENVIO_Y_RECEPCION:
        return CUmain_envio_y_recepcion();
            break;

    case TEST_DISCORDIADOR:
        break;
    }
    return 1;
}

int iniciar_patota(char* leido){

    char** palabras = string_split(leido, " ");
    int cantidad_tripulantes = atoi(palabras[1]);
    char* path = malloc(strlen(DIR_TAREAS) + strlen(palabras[2]) + 1);
    strcpy(path, DIR_TAREAS);
    strcat(path, palabras[2]);

    log_info(logger, "PATOTA: cantidad de tripulantes %d, url: %s", cantidad_tripulantes, path);

    char* archivo_tareas = leer_archivo_entero(path);
    free(path);

    t_patota* patota = malloc(sizeof(t_patota));
    patota->PID = nuevo_pid();
    list_add(lista_patotas, patota);

    if (archivo_tareas != NULL){
        enviar_archivo_tareas(archivo_tareas, patota->PID, socket_a_mi_ram_hq);
        free(archivo_tareas);
    } else {
        eliminar_patota_de_lista(lista_patotas, patota->PID);
        free(archivo_tareas);
        liberar_puntero_doble(palabras);
    	return 0;
    }

    if(!verificacion_archivo_tareas(socket_a_mi_ram_hq)){
        eliminar_patota_de_lista(lista_patotas, patota->PID);
        liberar_puntero_doble(palabras);
    	return 0;
    }

    t_list* l_aux = list_create();
    t_tripulante* t_aux;

    for(int i = 0; i < cantidad_tripulantes; i++){

    	if(i < contar_palabras(palabras) - 3){
    		t_aux = crear_puntero_tripulante((patota->PID)*10000 + i + 1, palabras[i+3]);
    	} else {
    		t_aux = crear_puntero_tripulante((patota->PID)*10000 + i + 1, "0|0");
    	}

    	list_add(l_aux, t_aux);
        enviar_tripulante(*t_aux, socket_a_mi_ram_hq);

        if(!verificacion_tcb(socket_a_mi_ram_hq)){
            eliminar_patota_de_lista(lista_patotas, patota->PID);
            liberar_lista(l_aux);
            liberar_puntero_doble(palabras);
        	return 0;
        }
        log_trace(logger, "Tripulante creado:\n tid: %i, estado: %c, pos: %i %i.", t_aux->TID, t_aux->estado_tripulante, t_aux->coord_x, t_aux->coord_y);
    }

    for(int i = 0; i < list_size(l_aux); i++){
    	t_aux = list_get(l_aux, i);
    	monitor_lista(sem_lista_tripulantes, (void*) list_add, lista_tripulantes, t_aux);
    	monitor_lista(sem_lista_new, (void*) list_add, lista_tripulantes_new, t_aux);
    	crear_hilo_tripulante(t_aux);
    }

    list_destroy(l_aux);
    liberar_puntero_doble(palabras);
    return 1;
}

void iniciar_planificacion() {

	if(planificacion_activa){
		log_info(logger, "La planificacion ya esta activa");
	} else{

	    planificacion_activa = 1;
	    log_debug(logger, "\nTripulantes en NEW: %i\n", list_size(lista_tripulantes_new));
	    log_debug(logger, "\nTripulantes en READY: %i\n", queue_size(cola_tripulantes_ready));
	    log_debug(logger, "\nTripulantes en EXEC: %i\n", list_size(lista_tripulantes_exec));
	    log_debug(logger, "\nTripulantes en BLOQ I/O: %i\n", queue_size(cola_tripulantes_block));
	    log_debug(logger, "\nTripulantes en BLOQ EMERGENCIA: %i\n", queue_size(cola_tripulantes_block_emergencia));
	    log_debug(logger, "\nTripulantes VIVOS: %i\n", list_size(lista_tripulantes));

	    pthread_t t_planificador;
	    pthread_create(&t_planificador, NULL, (void*) planificador, NULL);
	    pthread_detach(t_planificador);
	}

}

void planificador(){
    log_info(logger, "Planificando");
    log_info(logger, "Algoritmo %s", ALGORITMO);

    while(planificacion_activa){
        if (list_size(lista_tripulantes_exec) < GRADO_MULTITAREA && !queue_is_empty(cola_tripulantes_ready)){
            if(comparar_strings(ALGORITMO, "FIFO")){
                t_tripulante* aux_tripulante = monitor_cola_pop(sem_cola_ready, cola_tripulantes_ready);
            	quitar_tripulante_de_listas(aux_tripulante);
            	aux_tripulante->estado_tripulante = estado_tripulante[EXEC];
                monitor_lista(sem_lista_exec, (void*)list_add, lista_tripulantes_exec, aux_tripulante);
                // log_trace(logger, "Muevo %i a EXEC", aux_tripulante->TID);
            } else if(comparar_strings(ALGORITMO, "RR")){
                t_tripulante* aux_tripulante = monitor_cola_pop(sem_cola_ready, cola_tripulantes_ready);
            	quitar_tripulante_de_listas(aux_tripulante);
                aux_tripulante->quantum_restante = QUANTUM;
            	aux_tripulante->estado_tripulante = estado_tripulante[EXEC];
                monitor_lista(sem_lista_exec, (void*)list_add, lista_tripulantes_exec, aux_tripulante);
                // log_trace(logger, "Muevo %i a EXEC", aux_tripulante->TID);
            }
        } else {
        	usleep(1000);
        }
    }
}

void tripulante(t_tripulante* un_tripulante){

    int st_ram = crear_socket_cliente(IP_MI_RAM_HQ, PUERTO_MI_RAM_HQ);
    int st_mongo = crear_socket_cliente(IP_I_MONGO_STORE, PUERTO_I_MONGO_STORE);

    usleep(1000);

    enviar_tripulante(*un_tripulante, st_mongo);

    log_trace(logger, "Iniciando tripulante: %i", un_tripulante->TID);
    char estado_guardado = un_tripulante->estado_tripulante; // NEW
    iniciar_tripulante(un_tripulante, st_ram);

    if(llegue(un_tripulante)){
    	notificar_inicio_de_tarea(un_tripulante, st_mongo);
    }

    estado_guardado = un_tripulante->estado_tripulante; // READY

    if(comparar_strings(ALGORITMO, "FIFO")){
        ciclo_de_vida_fifo(un_tripulante, st_ram, st_mongo, &estado_guardado);
    } else if (comparar_strings(ALGORITMO, "RR")){
        ciclo_de_vida_rr(un_tripulante, st_ram, st_mongo, &estado_guardado);
    }

    close(st_ram);
    close(st_mongo);

    morir(un_tripulante);

}

int conseguir_siguiente_tarea(t_tripulante* un_tripulante, int socket_ram, int socket_mongo){
    pedir_tarea_a_mi_ram_hq(un_tripulante->TID, socket_ram);
    t_estructura* tarea = recepcion_y_deserializacion(socket_ram);

    if(tarea->codigo_operacion == TAREA){
    	free(un_tripulante->tarea.nombre);
        un_tripulante->tarea = *(tarea->tarea);
        if(llegue(un_tripulante)){
        	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
        }
        free(tarea->tarea);
        free(tarea);
        return 1;
    }
    else if(tarea->codigo_operacion == FALLO){
    	log_debug(logger, "%i paso a exit", un_tripulante->TID);
    	quitar_tripulante_de_listas(un_tripulante);
    	un_tripulante->estado_tripulante = estado_tripulante[EXIT];
    	t_TCB tcb_aux = tripulante_a_tcb(*un_tripulante);
    	t_buffer* b_tripulante = serializar_tcb(tcb_aux);
		empaquetar_y_enviar(b_tripulante, ACTUALIZAR, socket_ram);
		free(tarea);
    }
    return 0;
}

void morir(t_tripulante* un_tripulante){

    quitar_tripulante_de_listas(un_tripulante);

    if(esta_tripulante_en_lista(lista_tripulantes, un_tripulante->TID)){
        monitor_lista(sem_lista_tripulantes, (void*) eliminar_tripulante_de_lista, lista_tripulantes, (void*) un_tripulante->TID);
    }

    if(soy_el_ultimo_de_mi_especie(un_tripulante->TID)){
        t_patota* patota_aux = eliminar_patota_de_lista(lista_patotas, un_tripulante->TID/10000);
        free(patota_aux);
        log_trace(logger, "Muere el ultimo de la patota %i", un_tripulante->TID/10000);
    }

    free(un_tripulante->tarea.nombre);
    free(un_tripulante);
    pthread_exit(NULL);
}

void iniciar_tripulante(t_tripulante* un_tripulante, int socket){
    // Se verifica que se creo bien y se enlista

    if (un_tripulante->estado_tripulante == estado_tripulante[NEW]){
        while(planificacion_activa == 0){
        	usleep(1000);
            // por si lo matan antes de iniciar la planificacion
            if(un_tripulante->estado_tripulante == estado_tripulante[EXIT]){
            	un_tripulante->tarea.nombre = malloc(sizeof(char));
                morir(un_tripulante);
            }
        }
        enlistarse(un_tripulante, socket);
    }
    else {
        log_error(logger, "Por un motivo desconocido, el tripulante se ha creado en un estado distinto a NEW.");
    }
}

void enlistarse(t_tripulante* un_tripulante, int socket){
    // Se le asigna la tarea y se lo pasa a READY
    pedir_tarea_a_mi_ram_hq(un_tripulante->TID, socket);

    t_estructura* respuesta = recepcion_y_deserializacion(socket);

    if(respuesta->codigo_operacion == TAREA){
        un_tripulante->tarea = *(respuesta->tarea);
        free(respuesta->tarea);
    }
    else if (respuesta->codigo_operacion == FALLO){
        log_error(logger, "No se recibio ninguna tarea.\n Codigo de error: FALLO");
    }
    else{
        log_error(logger, "No se recibio ninguna tarea.\n Error desconocido.");
    }

    free(respuesta);

	cambiar_estado(un_tripulante, estado_tripulante[READY], socket);
    monitor_cola_push(sem_cola_ready, cola_tripulantes_ready, un_tripulante);
    log_debug(logger, "Estado cambiado a READY");

}

void realizar_tarea(t_tripulante* un_tripulante, int socket_ram, int socket_mongo){

    int codigo_tarea = identificar_tarea(un_tripulante->tarea.nombre);
    log_trace(logger, "%i mi tarea: %s", un_tripulante->TID, un_tripulante->tarea.nombre);
	// log_info(logger, "Codigo tarea: %i", codigo_tarea);

    switch(codigo_tarea){
        case GENERAR_OXIGENO:
            if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
            }else{
            	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
            	sleep(RETARDO_CICLO_CPU);
            	t_buffer* b_oxigeno = serializar_cantidad(un_tripulante->tarea.parametro);
            	empaquetar_y_enviar(b_oxigeno, OXIGENO, socket_mongo);
            	cambiar_estado(un_tripulante, estado_tripulante[BLOCK], socket_ram);
            	monitor_cola_push(sem_cola_block, cola_tripulantes_block, un_tripulante);
            }
            break;

        case CONSUMIR_OXIGENO:
        	if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
			}else{
            	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
				sleep(RETARDO_CICLO_CPU);
				int cantidad = (int) un_tripulante->tarea.parametro;
				t_buffer* b_oxigeno = serializar_cantidad(-cantidad);
				empaquetar_y_enviar(b_oxigeno, OXIGENO, socket_mongo);
            	cambiar_estado(un_tripulante, estado_tripulante[BLOCK], socket_ram);
            	monitor_cola_push(sem_cola_block, cola_tripulantes_block, un_tripulante);
            }
            break;

        case GENERAR_COMIDA:
            if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
            }else{
            	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
            	sleep(RETARDO_CICLO_CPU);
            	t_buffer* b_comida = serializar_cantidad(un_tripulante->tarea.parametro);
            	empaquetar_y_enviar(b_comida, COMIDA ,socket_mongo);
            	cambiar_estado(un_tripulante, estado_tripulante[BLOCK], socket_ram);
            	monitor_cola_push(sem_cola_block, cola_tripulantes_block, un_tripulante);
            }
            break;

        case CONSUMIR_COMIDA:
            if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
            }else{
            	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
            	sleep(RETARDO_CICLO_CPU);
            	int cantidad = (int) un_tripulante->tarea.parametro;
            	t_buffer* b_comida = serializar_cantidad(-cantidad);
            	empaquetar_y_enviar(b_comida, COMIDA ,socket_mongo);
            	cambiar_estado(un_tripulante, estado_tripulante[BLOCK], socket_ram);
            	monitor_cola_push(sem_cola_block, cola_tripulantes_block, un_tripulante);
            }
            break;

        case GENERAR_BASURA:
            if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
            }else{
            	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
            	sleep(RETARDO_CICLO_CPU);
            	t_buffer* b_basura = serializar_cantidad(un_tripulante->tarea.parametro);
            	empaquetar_y_enviar(b_basura, BASURA ,socket_mongo);
            	cambiar_estado(un_tripulante, estado_tripulante[BLOCK], socket_ram);
            	monitor_cola_push(sem_cola_block, cola_tripulantes_block, un_tripulante);
            }
            break;

        case DESCARTAR_BASURA:
            if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
            }else{
            	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
            	sleep(RETARDO_CICLO_CPU);
            	int cantidad = 0;
            	t_buffer* b_oxigeno = serializar_cantidad(cantidad);
            	empaquetar_y_enviar(b_oxigeno, BASURA ,socket_mongo);
            	cambiar_estado(un_tripulante, estado_tripulante[BLOCK], socket_ram);
            	monitor_cola_push(sem_cola_block, cola_tripulantes_block, un_tripulante);
            }
            break;

        default:
            if(!llegue(un_tripulante)){
                atomic_llegar_a_destino(un_tripulante, socket_ram);
                notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
                if(llegue(un_tripulante)){
                	notificar_inicio_de_tarea(un_tripulante, socket_mongo);
                }
            }else{
                atomic_no_me_despierten_estoy_trabajando(un_tripulante, socket_ram, socket_mongo);
            }
            break;
    }
}

int llegue(t_tripulante* un_tripulante){
    return un_tripulante->coord_x == un_tripulante->tarea.coord_x && un_tripulante->coord_y == un_tripulante->tarea.coord_y;
}

void atomic_llegar_a_destino(t_tripulante* un_tripulante, int socket){

    log_trace(logger, "%i Me encuentro en %i|%i.", un_tripulante->TID, un_tripulante->coord_x, un_tripulante->coord_y);

    uint32_t origen_x = un_tripulante->coord_x;
    uint32_t origen_y = un_tripulante->coord_y;
    uint32_t destino_x = un_tripulante->tarea.coord_x;
    uint32_t destino_y = un_tripulante->tarea.coord_y;

    uint32_t distancia_x = abs(destino_x - origen_x);
    uint32_t distancia_y = abs(destino_y - origen_y);

	sleep(RETARDO_CICLO_CPU);

	if(distancia_x != 0){

		destino_x > origen_x ? un_tripulante->coord_x++ : un_tripulante->coord_x--;
		distancia_x--;

	} else if (distancia_y != 0){

		destino_y > origen_y ? un_tripulante->coord_y++ : un_tripulante->coord_y--;
		distancia_y--;

	}

	log_trace(logger, "%i Distancia restante: %i|%i", un_tripulante->TID, distancia_x, distancia_y);
}


void atomic_no_me_despierten_estoy_trabajando(t_tripulante* un_tripulante, int socket_ram, int socket_mongo){

    if (un_tripulante->tarea.duracion > 0){
        sleep(RETARDO_CICLO_CPU);
        un_tripulante->tarea.duracion--;

        if (un_tripulante->tarea.duracion == 0){

            log_info(logger, "Tarea finalizada: %s\n", un_tripulante->tarea.nombre);
            notificar_fin_de_tarea(un_tripulante, socket_mongo);

            // 	Pequenio parche, implica DESBLOQUEAR
        	if(un_tripulante->estado_tripulante == estado_tripulante[BLOCK]){
        		cambiar_estado(un_tripulante, estado_tripulante[READY], socket_ram);
        		monitor_cola_push(sem_cola_ready, cola_tripulantes_ready, un_tripulante);
        	}

            if(conseguir_siguiente_tarea(un_tripulante, socket_ram, socket_mongo)){
            	log_trace(logger, "%i nueva tarea!: %s", un_tripulante->TID, un_tripulante->tarea.nombre);
            }
            else{
            	log_trace(logger, "%i Termine mis tareas!", un_tripulante->TID);
            }


        }else{
            log_debug(logger, "%i trabajando! tarea restante %i", un_tripulante->TID, un_tripulante->tarea.duracion);
        }
    }

    else {
        log_error(logger, "ERROR, duracion de tarea negativa.%i", un_tripulante->TID);
        log_error(logger, "o tripulante no pidio tarea.");
    }
}

void leer_consola() {

    char* leido;
    int comando;

    do {

        leido = readline(">>> ");

        if (strlen(leido) > 0) {
            comando = reconocer_comando(leido);

            switch (comando) {

                case INICIAR_PATOTA:
                    iniciar_patota(leido);
                    break;

                case INICIAR_PLANIFICACION:
                    iniciar_planificacion();
                    break;

                case LISTAR_TRIPULANTES:
                    listar_tripulantes();
                    break;

                case PAUSAR_PLANIFICACION:
                    pausar_planificacion();
                    break;

                case OBTENER_BITACORA:
                    obtener_bitacora(leido);
                    break;

                case EXPULSAR_TRIPULANTE:
                    expulsar_tripulante(leido);
                    break;

                case HELP:
                    help_comandos();
                    break;

                case APAGAR_SISTEMA:
                    sem_post(&sistema_activo);
                    free(leido);
                    pthread_exit(NULL);
                    break;

                case NO_CONOCIDO:
                    break;
            }
        }

        free(leido);

    } while (comando != APAGAR_SISTEMA);

}

void pausar_planificacion() {

	log_info(logger, "Pausar Planificacion");
    planificacion_activa = 0;

}

void obtener_bitacora(char* leido) {

	log_info(logger, "Obtener Bitacora");
    char** palabras = string_split(leido, " ");
    int tid_tripulante_a_buscar = atoi(palabras[1]);

    t_buffer* b_tid = serializar_entero(tid_tripulante_a_buscar);
	empaquetar_y_enviar(b_tid, PEDIR_BITACORA, socket_a_mongo_store);

    liberar_puntero_doble(palabras);
}

void expulsar_tripulante(char* leido) {

	log_info(logger, "Expulsar Tripulante");
    char** palabras = string_split(leido, " ");
    int tid_tripulante_a_expulsar = atoi(palabras[1]);

    t_buffer* b_tid = serializar_entero(tid_tripulante_a_expulsar);
    empaquetar_y_enviar(b_tid, T_SIGKILL, socket_a_mi_ram_hq);

    t_estructura* respuesta = recepcion_y_deserializacion(socket_a_mi_ram_hq);

    if(respuesta->codigo_operacion == EXITO){
        if(esta_tripulante_en_lista(lista_tripulantes, tid_tripulante_a_expulsar)){

            bool obtener_tripulante(void* elemento){
                return (((t_tripulante*) elemento)->TID == tid_tripulante_a_expulsar);
            }

            t_tripulante* t_aux = list_find(lista_tripulantes, obtener_tripulante);

            log_info(logger, "Tripulante expulsado, TID: %d", tid_tripulante_a_expulsar);
            log_info(logger, "Lugar del deceso: %i|%i", t_aux->coord_x, t_aux->coord_y);
        	quitar_tripulante_de_listas(t_aux);
        	t_aux->estado_tripulante = estado_tripulante[EXIT];
        }
        else{
            log_info(logger, "Dicho tripulante no existe en Discordiador.");
        }
    }
    else if (respuesta->codigo_operacion == FALLO){
        log_info(logger, "No existe el tripulante. TID: %d", tid_tripulante_a_expulsar);
    }
    else{
        log_info(logger, "Error desconocido.");
    }

    free(respuesta);

    liberar_puntero_doble(palabras);

}

void crear_hilo_tripulante(t_tripulante* un_tripulante){

    pthread_t hilo_tripulante;
    pthread_create(&hilo_tripulante, NULL, (void*)tripulante, (void*)un_tripulante);
    pthread_detach(hilo_tripulante);
}

int identificar_tarea(char* nombre_recibido){

    if(comparar_strings(nombre_recibido, "GENERAR_OXIGENO")){
        return GENERAR_OXIGENO;
    }
    else if(comparar_strings(nombre_recibido, "CONSUMIR_OXIGENO")){
        return CONSUMIR_OXIGENO;
    }
    else if(comparar_strings(nombre_recibido, "GENERAR_COMIDA")){
        return GENERAR_COMIDA;
    }
    else if(comparar_strings(nombre_recibido, "CONSUMIR_COMIDA")){
        return CONSUMIR_COMIDA;
    }
    else if(comparar_strings(nombre_recibido, "GENERAR_BASURA")){
        return GENERAR_BASURA;
    }
    else if(comparar_strings(nombre_recibido, "DESCARTAR_BASURA")){
        return DESCARTAR_BASURA;
    }

    return OTRA_TAREA;

}

void listar_tripulantes() {

    log_debug(logger, "\nTripulantes en NEW: %i\n", list_size(lista_tripulantes_new));
    log_debug(logger, "\nTripulantes en READY: %i\n", queue_size(cola_tripulantes_ready));
    log_debug(logger, "\nTripulantes en EXEC: %i\n", list_size(lista_tripulantes_exec));
    log_debug(logger, "\nTripulantes en BLOQ I/O: %i\n", queue_size(cola_tripulantes_block));
    log_debug(logger, "\nTripulantes en BLOQ EMERGENCIA: %i\n", queue_size(cola_tripulantes_block_emergencia));
    log_debug(logger, "\nTripulantes VIVOS: %i\n", list_size(lista_tripulantes));

    char* fechaHora = fecha_y_hora();

    log_info(logger, "Estado de la nave: %s\n", fechaHora);

    t_patota* aux_p;
    t_tripulante* aux_t;
    t_list* lista_tripulantes_de_una_patota;

    for(int i = 0; i < list_size(lista_patotas); i++){
        aux_p = list_get(lista_patotas, i);

        lista_tripulantes_de_una_patota = lista_tripulantes_patota(aux_p->PID);

        for(int j = 0; j < list_size(lista_tripulantes_de_una_patota); j++){
            aux_t = list_get(lista_tripulantes_de_una_patota, j);
            // printf("    Tripulante: %d \t   Patota: %d \t Status: %c\n", aux_t->TID, aux_t->TID/10000, aux_t->estado_tripulante);
            log_info(logger, "TID: %d  PID: %d Status: %c", aux_t->TID, aux_t->TID/10000, aux_t->estado_tripulante);
        }

        liberar_lista(lista_tripulantes_de_una_patota);
    }

}

t_list* lista_tripulantes_patota(uint32_t pid){

    t_list* lista_tripulantes_patota = list_create();

    enviar_pid_a_ram(pid, socket_a_mi_ram_hq);

    t_estructura* respuesta = recepcion_y_deserializacion(socket_a_mi_ram_hq);

    while(respuesta->codigo_operacion != EXITO){
        list_add(lista_tripulantes_patota, respuesta->tcb);
        free(respuesta);
        respuesta = recepcion_y_deserializacion(socket_a_mi_ram_hq);
    }
    if(respuesta->codigo_operacion == FALLO){
        log_info(logger, "Error al pedir los tripulantes para listar.");
        log_info(logger, "Codigo de error: FALLO\n");
    }

    free(respuesta);

    bool ordenar_por_tid(void* un_elemento, void* otro_elemento){
         return ((((t_tripulante*) un_elemento)->TID) < (((t_tripulante*) otro_elemento)->TID));
    }

    list_sort(lista_tripulantes_patota, ordenar_por_tid);

    return lista_tripulantes_patota;

}

void test_config_discordiador(){

    log_warning(logger, "%s", IP_MI_RAM_HQ);
    log_warning(logger, "%s", PUERTO_MI_RAM_HQ);
    log_warning(logger, "%s", IP_I_MONGO_STORE);
    log_warning(logger, "%s", PUERTO_I_MONGO_STORE);
    log_warning(logger, "%s", ALGORITMO);
    log_warning(logger, "%i", GRADO_MULTITAREA);
    log_warning(logger, "%i", QUANTUM);
    log_warning(logger, "%i", RETARDO_CICLO_CPU);
    log_warning(logger, "%i", DURACION_SABOTAJE);

}

int soy_el_ultimo_de_mi_especie(int tid){
    bool t_misma_patota(void* elemento1){
        return tid/10000 == (((t_tripulante*) elemento1)->TID)/10000;
    }

    int respuesta = list_count_satisfying(lista_tripulantes, t_misma_patota);
    // 0 porque elimino el tripulante de la lista antes de verificar esto
    // si lo eliminara despues de verificar, seria 1
    return respuesta == 0;
}


void* eliminar_patota_de_lista(t_list* lista, int elemento){

    bool contains(void* elemento1){
        return (elemento == ((t_patota*) elemento1)->PID);
    }

    t_patota* aux = list_remove_by_condition(lista, contains);
    return aux;
}

void test_soy_el_ultimo_de_mi_especie(){

    t_tripulante* trip_1 = malloc(sizeof(t_tripulante));
    trip_1->TID = 10001;
    t_tripulante* trip_2 = malloc(sizeof(t_tripulante));
    trip_2->TID = 10002;
    t_tripulante* trip_3 = malloc(sizeof(t_tripulante));
    trip_3->TID = 20001;
    t_tripulante* trip_4 = malloc(sizeof(t_tripulante));
    trip_4->TID = 20002;
    t_tripulante* trip_5 = malloc(sizeof(t_tripulante));
    trip_5->TID = 30001;

    list_add(lista_tripulantes, trip_1);
    list_add(lista_tripulantes, trip_2);
    list_add(lista_tripulantes, trip_3);
    list_add(lista_tripulantes, trip_4);
    list_add(lista_tripulantes, trip_5);

    printf("trip 1: %i", soy_el_ultimo_de_mi_especie(10001));
    printf("trip 2: %i", soy_el_ultimo_de_mi_especie(10002));
    printf("trip 3: %i", soy_el_ultimo_de_mi_especie(20001));
    printf("trip 4: %i", soy_el_ultimo_de_mi_especie(20002));
    printf("trip 5: %i", soy_el_ultimo_de_mi_especie(30001));
    eliminar_tripulante_de_lista(lista_tripulantes, 20002);
    printf("trip 3: %i", soy_el_ultimo_de_mi_especie(20001));

}

void test_eliminar_patota_de_lista(){

    t_patota* patota = malloc(sizeof(t_patota));
    patota->PID = 1;
    list_add(lista_patotas, patota);

    printf("una patota en la lista %i", list_size(lista_patotas));
    printf("la elimino");
    t_patota* otra_patota = eliminar_patota_de_lista(lista_patotas, 1);
    printf("cero patotas en la lista %i", list_size(lista_patotas));
    printf("pid de la eliminada: %i", otra_patota->PID);

}

void quitar_tripulante_de_listas(t_tripulante* un_tripulante){

    if(esta_tripulante_en_lista(lista_tripulantes_new, un_tripulante->TID)){
        monitor_lista(sem_lista_new, (void*) eliminar_tripulante_de_lista, lista_tripulantes_new, (void*) un_tripulante->TID);
    }

    if(esta_tripulante_en_lista(cola_tripulantes_ready->elements, un_tripulante->TID)){
        monitor_lista(sem_cola_ready, (void*) eliminar_tripulante_de_lista, cola_tripulantes_ready->elements, (void*) un_tripulante->TID);
    }

    if(esta_tripulante_en_lista(lista_tripulantes_exec, un_tripulante->TID)){
        monitor_lista(sem_lista_exec, (void*) eliminar_tripulante_de_lista, lista_tripulantes_exec, (void*) un_tripulante->TID);
    }

    if(esta_tripulante_en_lista(cola_tripulantes_block->elements, un_tripulante->TID)){
        monitor_lista(sem_cola_block, (void*) eliminar_tripulante_de_lista, cola_tripulantes_block->elements, (void*) un_tripulante->TID);
    }

    if(esta_tripulante_en_lista(cola_tripulantes_block_emergencia->elements, un_tripulante->TID)){
        monitor_lista(sem_cola_block_emergencia, (void*) eliminar_tripulante_de_lista, cola_tripulantes_block_emergencia->elements, (void*) un_tripulante->TID);
    }

}

void verificar_cambio_estado(char* estado_guardado, t_tripulante* un_tripulante, int socket){
    if(*estado_guardado != un_tripulante->estado_tripulante){
        log_debug(logger, "%i estoy en %c y cambio a %c", un_tripulante->TID, *estado_guardado, un_tripulante->estado_tripulante);
        actualizar_tripulante(un_tripulante, socket);
        *estado_guardado = un_tripulante->estado_tripulante;
    }
}

void actualizar_tripulante(t_tripulante* un_tripulante, int socket){
	if(un_tripulante->estado_tripulante != estado_tripulante[EXIT]){

		t_TCB tcb_aux;
		tcb_aux.TID = un_tripulante->TID;
		tcb_aux.coord_x = un_tripulante->coord_x;
		tcb_aux.coord_y = un_tripulante->coord_y;
		tcb_aux.estado_tripulante = un_tripulante->estado_tripulante;
		tcb_aux.puntero_a_pcb = 0;
		tcb_aux.siguiente_instruccion = 0;
		t_buffer* buffer_t = serializar_tcb(tcb_aux);
		empaquetar_y_enviar(buffer_t, ACTUALIZAR, socket);

	}
}

int verificacion_tcb(int socket){

    t_estructura* respuesta = recepcion_y_deserializacion(socket);

    if(respuesta->codigo_operacion == EXITO){
        log_info(logger, "Cargado el TCB en memoria ");
        free(respuesta);
        return 1;
    } else{
        log_warning(logger, "No hay memoria para el TCB");
        free(respuesta);
        return 0;
    }
}

int verificacion_archivo_tareas(int socket){

    t_estructura* respuesta = recepcion_y_deserializacion(socket);

    if(respuesta->codigo_operacion == EXITO){
        log_info(logger, "Cargado el archivo de tareas en memoria.");
        free(respuesta);
        return 1;
    } else{
        log_warning(logger, "No hay memoria para el archivo de tareas.");
        free(respuesta);
        return 0;
    }
}

void cambiar_estado(t_tripulante* un_tripulante, char estado, int socket){
	quitar_tripulante_de_listas(un_tripulante);
	un_tripulante->estado_tripulante = estado;
	actualizar_tripulante(un_tripulante, socket);
}

void peligro(t_posicion* posicion_sabotaje, int socket_ram){
	// PRIMERO los de EXEC, los de mayor TID primero
	// despues lo de READY, los de mayor TID primero
	log_warning(logger, "Estamos en peligro!");
	pausar_planificacion();

	int i, j;
	t_tripulante* t_aux;
	t_list* lista_auxiliar = list_create();

    bool ordenar_por_tid(void* un_elemento, void* otro_elemento){
         return ((((t_tripulante*) un_elemento)->TID) > (((t_tripulante*) otro_elemento)->TID));
    }

    j = list_size(lista_tripulantes_exec);
	for(i = 0; i < j ; i++){
		t_aux = monitor_lista(sem_lista_exec, (void*) list_remove, lista_tripulantes_exec, 0);
        log_trace(logger, "Tripulante removido:\n tid: %i, estado: %c, pos: %i %i\n", (int)t_aux->TID, (char) t_aux->estado_tripulante, (int) t_aux->coord_x, (int) t_aux->coord_y);
		list_add(lista_auxiliar, t_aux);
	}
	list_sort(lista_auxiliar, ordenar_por_tid);

	j = list_size(lista_auxiliar);
	for(i = 0; i < j; i++){
		t_aux = list_remove(lista_auxiliar, 0);
		cambiar_estado(t_aux, estado_tripulante[PANIK], socket_ram);
		monitor_cola_push(sem_cola_block_emergencia, cola_tripulantes_block_emergencia, t_aux);
	}

    j = queue_size(cola_tripulantes_ready);
	for(i = 0; i < j; i++){
		t_aux = monitor_cola_pop(sem_lista_exec, cola_tripulantes_ready);
		log_trace(logger, "Tripulante removido:\n tid: %i, estado: %c, pos: %i %i\n", (int)t_aux->TID, (char) t_aux->estado_tripulante, (int) t_aux->coord_x, (int) t_aux->coord_y);
		list_add(lista_auxiliar, t_aux);
	}
	list_sort(lista_auxiliar, ordenar_por_tid);

	j = list_size(lista_auxiliar);

	for(i = 0; i < j; i++){
		t_aux = list_remove(lista_auxiliar, 0);
		cambiar_estado(t_aux, estado_tripulante[PANIK], socket_ram);
		monitor_cola_push(sem_cola_block_emergencia, cola_tripulantes_block_emergencia, t_aux);
	}

	// si necesitamos printear
	// j = queue_size(cola_tripulantes_block_emergencia);

	// for(i = 0; i < j; i++){
		// t_aux = queue_pop(cola_tripulantes_block_emergencia);
		// log_trace(logger, "Tripulante removido:\n tid: %i, estado: %c, pos: %i %i\n", (int)t_aux->TID, (char) t_aux->estado_tripulante, (int) t_aux->coord_x, (int) t_aux->coord_y);
	// }

	list_destroy(lista_auxiliar);

	estamos_en_peligro = 1;

	t_aux = tripulante_mas_cercano_a(posicion_sabotaje);
	log_warning(logger, "%i ES EL TRIPULANTE MAS CERCANO", t_aux->TID);

	t_tarea contexto = t_aux->tarea;
	t_tarea resolver_sabotaje;
	resolver_sabotaje.coord_x = posicion_sabotaje->coord_x;
	resolver_sabotaje.coord_y = posicion_sabotaje->coord_y;
	resolver_sabotaje.duracion = DURACION_SABOTAJE;
	t_aux->tarea = resolver_sabotaje;

    log_error(logger, "Cordenada en X: %i\n", resolver_sabotaje.coord_x);
    log_error(logger, "Cordenada en Y: %i\n", resolver_sabotaje.coord_y);
    log_error(logger, "Duracion: %i\n", resolver_sabotaje.duracion);

	cambiar_estado(t_aux, estado_tripulante[EXEC], socket_ram);

    log_error(logger, "PRE PELIGRO");

	while(estamos_en_peligro){
		usleep(1000); // todos se esperan a que termine el sabotaje
	}

	cambiar_estado(t_aux, estado_tripulante[PANIK], socket_ram);
	monitor_cola_push(sem_cola_block_emergencia, cola_tripulantes_block_emergencia, t_aux);

	t_aux->tarea = contexto;

	// al terminar el sabotaje paso todos a ready (issue #2163)
	j = queue_size(cola_tripulantes_block_emergencia);

	for(i = 0; i < j; i++){
		t_aux = monitor_cola_pop(sem_cola_block_emergencia, cola_tripulantes_block_emergencia);
		cambiar_estado(t_aux, estado_tripulante[READY], socket_ram);
		monitor_cola_push(sem_cola_ready, cola_tripulantes_ready, t_aux);
	}

	enviar_codigo(REPARADO, socket_a_mongo_store);
	// iniciar_planificacion();

}

t_tripulante* tripulante_mas_cercano_a(t_posicion* posicion){
	int x_sabotaje = posicion->coord_x;
	int y_sabotaje = posicion->coord_y;
	t_tripulante* un_tripulante;

	void* mas_cercano (void* un_trip, void* otro_trip){
		int distancia_x_t1 = abs(x_sabotaje - ((t_tripulante*) un_trip)->coord_x);
		int distancia_y_t1 = abs(y_sabotaje - ((t_tripulante*) un_trip)->coord_y);
		int distancia_x_t2 = abs(x_sabotaje - ((t_tripulante*) otro_trip)->coord_x);
		int distancia_y_t2 = abs(y_sabotaje - ((t_tripulante*) otro_trip)->coord_y);

		int distancia_t1 = distancia_x_t1 + distancia_y_t1;
		int distancia_t2 = distancia_x_t2 + distancia_y_t2;

		if(distancia_t1 < distancia_t2 ){
			return un_trip;
		}
		else{
			return otro_trip;
		}
	}

	un_tripulante = list_get_minimum(cola_tripulantes_block_emergencia->elements, mas_cercano);

	return un_tripulante;
}

void resolver_sabotaje(t_tripulante* un_tripulante, int socket_ram, int socket_mongo){
	notificar_inicio_sabotaje(un_tripulante, socket_mongo);

	while(!llegue(un_tripulante)){
		atomic_llegar_a_destino(un_tripulante, socket_ram);
		notificar_movimiento(un_tripulante, socket_mongo, socket_ram);
		log_warning(logger, "AAAAAAAAAAAAA");
	}

	while(un_tripulante->tarea.duracion > 0){
		sleep(RETARDO_CICLO_CPU);
		un_tripulante->tarea.duracion--;
		log_warning(logger, "DESABOTEATEEEEE");
	}

	estamos_en_peligro = 0;
	log_warning(logger, "Sabotaje resuelto, viviremos un dia mas.");
	notificar_fin_sabotaje(un_tripulante, socket_mongo);
}

void esperar_entrada_salida(t_tripulante* un_tripulante, int st_ram, int st_mongo){
	atomic_no_me_despierten_estoy_trabajando(un_tripulante, st_ram, st_mongo);
}

int es_mi_turno(t_tripulante* un_tripulante){
	t_tripulante* titular = monitor_cola_pop_or_peek(sem_cola_block, (void*) queue_peek, cola_tripulantes_block);
	return (titular->TID == un_tripulante->TID);
}

void ciclo_de_vida_fifo(t_tripulante* un_tripulante, int st_ram, int st_mongo, char* estado_guardado){
    while(un_tripulante->estado_tripulante != estado_tripulante[EXIT]){
        verificar_cambio_estado(estado_guardado, un_tripulante, st_ram);

    	if(planificacion_activa){
    		switch(un_tripulante->estado_tripulante){
    		case 'E':
				realizar_tarea(un_tripulante, st_ram, st_mongo);
    			break;
    		case 'B':
				if(es_mi_turno(un_tripulante)){
					esperar_entrada_salida(un_tripulante, st_ram, st_mongo);
				} else {
					log_trace(logger, "%i espero mi turno!", un_tripulante->TID);
					sleep(RETARDO_CICLO_CPU); // Espero hasta que la entrada deje de estar ocupada
				}
    			break;
    		case 'R':
    			sleep(RETARDO_CICLO_CPU);
				break;
    		}
    	} else {
			if(estamos_en_peligro && un_tripulante->estado_tripulante == 'E'){
				resolver_sabotaje(un_tripulante, st_ram, st_mongo);
			} else{
				usleep(1000);
			}
    	}

		verificar_cambio_estado(estado_guardado, un_tripulante, st_ram);
    }
}

void ciclo_de_vida_rr(t_tripulante* un_tripulante, int st_ram, int st_mongo, char* estado_guardado){

	while(un_tripulante->estado_tripulante != estado_tripulante[EXIT]){
        verificar_cambio_estado(estado_guardado, un_tripulante, st_ram);

    	if(planificacion_activa){
            switch(un_tripulante->estado_tripulante){
			case 'E':
				if(un_tripulante->quantum_restante > 0){
					realizar_tarea(un_tripulante, st_ram, st_mongo);
					un_tripulante->quantum_restante--;
					log_debug(logger, "%i Trabaaajo muy duuro, como un esclaaaaavo: quantum restante %i", un_tripulante->TID, un_tripulante->quantum_restante);
				} else {
					cambiar_estado(un_tripulante, estado_tripulante[READY], st_ram);
					monitor_cola_push(sem_cola_ready, cola_tripulantes_ready, un_tripulante);
					log_debug(logger, "%i Ayudaaa me desalojan", un_tripulante->TID);
				}
				break;
			case 'B':
				if(es_mi_turno(un_tripulante)){
					esperar_entrada_salida(un_tripulante, st_ram, st_mongo);
				} else {
					log_trace(logger, "%i espero mi turno!", un_tripulante->TID);
					sleep(RETARDO_CICLO_CPU); // Espero hasta que la entrada deje de estar ocupada
				}
				break;
			case 'R':
				sleep(RETARDO_CICLO_CPU);
				break;
            }
		} else{

			if(estamos_en_peligro && un_tripulante->estado_tripulante == 'E'){
				resolver_sabotaje(un_tripulante, st_ram, st_mongo);
			} else{
				usleep(1000);
			}

		}
    verificar_cambio_estado(estado_guardado, un_tripulante, st_ram);
	}
}


void liberar_lista(t_list* lista){
	if(lista == NULL){
		// No hacer nada, aunque no se deberia cometer este error
	} else if (list_is_empty(lista)){
		list_destroy(lista);
	} else {
		list_destroy_and_destroy_elements(lista, free);
	}
}


void liberar_cola(t_queue* cola){
	if(cola == NULL){
		// No hacer nada, aunque no se deberia cometer este error
	} else if (queue_is_empty(cola)){
		queue_destroy_and_destroy_elements(cola, free);
	} else {
		queue_destroy(cola);
	}
}


void guardian_mongo(){
	log_info(logger, "Vigilando que no explote el Mongo");
	int flag = 1;
	t_estructura* mensaje;

	while(flag){

		mensaje = recepcion_y_deserializacion(socket_a_mongo_store);
		log_debug(logger, "Llega mensaje de Mongo");

		switch(mensaje->codigo_operacion){
			case BITACORA:
				log_info(logger, "Bitacora del tripulante:");
				char** bitacora_ordenada = string_split(mensaje->archivo_tareas->texto, ".");

				for(int i = 0; i < contar_palabras(bitacora_ordenada); i++){
					log_info(logger, "%s", bitacora_ordenada[i]);
				}

				liberar_puntero_doble(bitacora_ordenada);
				break;
			case FALLO:
				log_info(logger, "La bitacora del tripulante estaba vacia.");
				break;
			case SABOTAJE:
				log_info(logger, "Sabotaje a la vista");
				if((queue_is_empty(cola_tripulantes_ready) && list_is_empty(lista_tripulantes_exec))){
					log_warning(logger, "No estamos listos para defendernos del sabotaje...");
					enviar_codigo(FALLO, socket_a_mongo_store);
					log_info(logger, "Asegurese de tener un tripulante en R o E.");
				} else {
					peligro(mensaje->posicion, socket_a_mi_ram_hq);
					free(mensaje->posicion);
				}
				break;
			case DESCONEXION:
				log_error(logger, "Se desconecta el Mongo");
				flag = 0;
				break;
			default:
				log_info(logger, "Error desconocido");
				log_error(logger, "Error desconocido, codigo de error: %i", mensaje->codigo_operacion);
		}
		free(mensaje);
	}
}
