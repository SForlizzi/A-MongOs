/*
 * mi_ram_hq.c
 *
 *  Created on: 9 may. 2021
 *      Author: utnso
 */

#include "mi_ram_hq.h"

int main(int argc, char** argv){

	logger = log_create("mi_ram_hq.log", "MI_RAM_HQ", 1, LOG_LEVEL_DEBUG);
	config = config_create("mi_ram_hq.config");
	config_discordiador = config_create("../Discordiador/discordiador.config");
	
    int mi_ram_fd = iniciar_servidor();

    int discordiador_fd = esperar_discordiador(mi_ram_fd);

    while(1){
		int cod_op = leer_operacion(discordiador_fd);
		t_tarea* tarea;
		
		switch(cod_op){
			case MENSAJE:
				log_info(logger, "MENSAJE RECIBIDO");
				break;
			case PEDIR_TAREA:
				log_info(logger, "PEDIDO DE TAREA RECIBIDO");
				break;
			case COD_TAREA:
				printf("recibo una tarea");				
				break;
			case -1:
				log_info(logger, "Murio el discordiador");
				return EXIT_FAILURE;
			default:
				log_warning(logger, "Operacion desconocida");
				break;
		}
	}

    close(discordiador_fd);
    log_destroy(logger);
    config_destroy(config);
    config_destroy(config_discordiador);
	return EXIT_SUCCESS;

    /*pthread_t hilo_escucha;
	pthread_create(&hilo_escucha, NULL, (void*) escuchar_alos_cliente, NULL);

	pthread_join(hilo_escucha, NULL);*/
}

/*
void escuchar_alos_cliente(){
    int socket_oyente = crear_socket_oyente("127.0.0.2", "4000");
    
    escuchar(socket_oyente, (void*) hola);

}

void hola(){
    printf("Hola");
}
*/
