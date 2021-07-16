#include "mongo_sabotaje.h"

extern char** posiciones_sabotajes;
int pos_actual_sabotaje = 0;

void enviar_posicion_sabotaje(int socket_discordiador) {
	log_warning(logger_mongo, "INICIO enviar_posicion_sabotaje");

	if (pos_actual_sabotaje != contar_palabras(posiciones_sabotajes)) {
		log_warning(logger_mongo, "dentro del if");
		t_posicion posicion;

		posicion.coord_x = (uint32_t) posiciones_sabotajes[pos_actual_sabotaje][0] - 48; // EQUIVALENCIA ASCII NUMERO
		posicion.coord_y = (uint32_t) posiciones_sabotajes[pos_actual_sabotaje][2] - 48; // EQUIVALENCIA ASCII NUMERO
		log_warning(logger_mongo, "antes de enviar");
		empaquetar_y_enviar(serializar_posicion(posicion), SABOTAJE, socket_discordiador);
		log_warning(logger_mongo, "enviada");

		pos_actual_sabotaje++;
	}
	else{
		log_warning(logger_mongo, "No hay posiciones de sabotaje");
		pos_actual_sabotaje = 0; //reset para que vuelva a ser posible sabotear
	}

}

char* reparar() {
	// TODO: mejorable
	char* roto = string_new();
    int reparado = 0;
    
    reparado = verificar_cant_bloques();

    if (reparado == 1)
    	string_append(&roto, "\n\t-la cantidad de bloques del superbloque");

    reparado = verificar_bitmap();

    if (reparado == 2)
    	string_append(&roto, "\n\t-el bitmap del superbloque");

    reparado = verificar_sizes();

    if (reparado == 3)
    	string_append(&roto, "\n\t-los tamanios de los archivos");

    reparado = verificar_block_counts();

    if (reparado == 4)
    	string_append(&roto, "\n\t-la cantidad de bloques de los recursos");

    reparado = verificar_blocks();

    if (reparado == 5)
    	string_append(&roto, "\n\t-la lista de bloques de los recursos");

    if (reparado == 0)
    	string_append(&roto, "\n\t-nada");

    return roto;
}

int verificar_cant_bloques() {

	uint32_t cant_bloques = obtener_cantidad_bloques_superbloque();

	fseek(directorio.blocks, 0, SEEK_END);
	int tamanio_en_bytes = ftell(directorio.blocks);
	fseek(directorio.blocks, 0, SEEK_SET);
    int cantidad_real = tamanio_en_bytes / TAMANIO_BLOQUE;

    if (cant_bloques != cantidad_real) {
        t_bitarray* bitmap = obtener_bitmap();
        reescribir_superbloque(TAMANIO_BLOQUE, cantidad_real, bitmap);
        return 1;
    }
    else{
    	return 0;
    }
}

int verificar_bitmap() {
	//Creo la lista // TODO de donde saco la lista?
	t_list* lista_bloques_ocupados = list_create();

	//Agrego los bloques usados en la lista, con en el indice = n° bloque
    recorrer_recursos(lista_bloques_ocupados);
    recorrer_bitacoras(lista_bloques_ocupados);

    sortear(lista_bloques_ocupados);

    if (bloques_ocupados_difieren(lista_bloques_ocupados)) {
        actualizar_bitmap(lista_bloques_ocupados);
        return 2;
    }
    else
        return 0;

    list_destroy(lista_bloques_ocupados);
}

int verificar_sizes() {
    // Compara tamanio archivo vs lo que ocupa en sus blocks, uno por uno, si alguna vez rompio, devuelve 3, sino 0

	uint32_t tamanio_real_B = bloques_contar('B');
	uint32_t tamanio_real_C = bloques_contar('C');
	uint32_t tamanio_real_O = bloques_contar('O');

	int corrompido = 0;

	if(tamanio_real_B != tamanio_archivo(path_basura)) {
		set_tam(path_basura, tamanio_real_B);
		corrompido = 3;
	}
	if(tamanio_real_C != tamanio_archivo(path_comida)) {
		set_tam(path_comida, tamanio_real_C);
		corrompido = 3;
	}
	if(tamanio_real_O != tamanio_archivo(path_oxigeno)) {
		set_tam(path_oxigeno, tamanio_real_O);
		corrompido = 3;
	}

	return corrompido;
}

int verificar_block_counts(t_TCB* tripulante) { 
    // Compara block count vs el largo de la lista de cada archivo recurso. Devuelve 4 si algún recurso fue corrompido

	uint32_t cantidad_real_basura = list_size(obtener_lista_bloques(path_basura));
	uint32_t cantidad_real_comida = list_size(obtener_lista_bloques(path_comida));
	uint32_t cantidad_real_oxigeno = list_size(obtener_lista_bloques(path_oxigeno));;

	int corrompido = 0;

	if(cantidad_real_oxigeno != cantidad_bloques_recurso(path_oxigeno)) {
		set_tam(path_oxigeno, cantidad_real_oxigeno);
		corrompido = 4;
	}
	if(cantidad_real_comida  != cantidad_bloques_recurso(path_comida)) {
		set_tam(path_comida, cantidad_real_comida);
		corrompido = 4;
	}
	if(cantidad_real_basura  != cantidad_bloques_recurso(path_basura)) {
		set_tam(path_basura, cantidad_real_basura);
		corrompido = 4;
	}
	return corrompido;
}
int verificar_blocks() {

	if (md5_no_concuerda() || !tamanio_correcto() || bloques_sin_sentido() || bitmap_no_concuerda()) {
		restaurar_blocks();
		return 5;
	}

	return 0;
}

int md5_no_concuerda() { // Puede explotar
	t_config* config_basura = config_create(path_basura);
	t_config* config_oxigeno = config_create(path_oxigeno);
	t_config* config_comida = config_create(path_comida);

	char* bloques_basura = config_get_string_value(config_basura, "BLOCKS");
	char* bloques_oxigeno = config_get_string_value(config_oxigeno, "BLOCKS");
	char* bloques_comida = config_get_string_value(config_comida, "BLOCKS");

	char* nuevo_md5_basura = md5_archivo(bloques_basura);
	char* nuevo_md5_oxigeno = md5_archivo(bloques_oxigeno);
	char* nuevo_md5_comida = md5_archivo(bloques_comida);

	char* md5_basura = config_get_string_value(config_basura, "MD5");
	char* md5_oxigeno = config_get_string_value(config_oxigeno, "MD5");
	char* md5_comida = config_get_string_value(config_comida, "MD5");

	config_destroy(config_basura);
	config_destroy(config_oxigeno);
	config_destroy(config_comida);

	if (!strcmp(nuevo_md5_basura, md5_basura) || !strcmp(nuevo_md5_oxigeno, md5_oxigeno) || !strcmp(nuevo_md5_comida, md5_comida)) {
		return 1;
	}

	return 0;
}

int tamanio_correcto() {
	uint32_t cant_bloques = obtener_cantidad_bloques_superbloque();
	uint32_t tamanio_bloque = obtener_tamanio_bloque_superbloque();

	return (cant_bloques * tamanio_bloque) == strlen(directorio.mapa_blocks); // Revisar logica
}

int bloques_sin_sentido() {
	t_list* bloques_basura = obtener_lista_bloques(path_basura);
	t_list* bloques_oxigeno = obtener_lista_bloques(path_oxigeno);
	t_list* bloques_comida = obtener_lista_bloques(path_comida);
	uint32_t nro_bloque;

	for(int i = 0; i < list_size(bloques_basura) ; i++) {
		nro_bloque = list_get(bloques_basura, i);
		if (nro_bloque > obtener_cantidad_bloques_superbloque() || nro_bloque < 0)
			return 1;
	}

	for(int i = 0; i < list_size(bloques_oxigeno) ; i++) {
		nro_bloque = list_get(bloques_oxigeno, i);
		if (nro_bloque > obtener_cantidad_bloques_superbloque() || nro_bloque < 0)
			return 1;
	}

	for(int i = 0; i < list_size(bloques_comida) ; i++) {
		nro_bloque = list_get(bloques_comida, i);
		if (nro_bloque > obtener_cantidad_bloques_superbloque() || nro_bloque < 0)
			return 1;
	}

	return 0;
}

int bitmap_no_concuerda() {
	t_bitarray* bitmap = obtener_bitmap();
	t_list* bloques_basura = obtener_lista_bloques(path_basura);
	t_list* bloques_oxigeno = obtener_lista_bloques(path_oxigeno);
	t_list* bloques_comida = obtener_lista_bloques(path_comida);
	uint32_t nro_bloque;

	list_add_all(bloques_basura, bloques_oxigeno);
	list_add_all(bloques_basura, bloques_oxigeno);

	for(int i = 0; i < list_size(bloques_basura) ; i++){
		nro_bloque = list_get(bloques_basura, i);
		if (!bitarray_test_bit(bitmap, nro_bloque)) // Funciona con uint?
			return 1;
	}

	return 0;
}

int lista_blocks_saboteada(FILE* archivo) {
/*
	//Concatenar los bloques de la lista de bloques
	char* nuevo_hash = string_new();
	int* lista_bloques = (int*) obtener_lista_bloques(recurso.basura);
	for(int i = 0; i < sizeof(lista_bloques) / sizeof(int); i++)
		string_append(&nuevo_hash, string_itoa(lista_bloques[i]));

	//Hashear lo concatenado (Osea si la lista es [1,4,2], debo hashear a MD5 la cadena 142)
    // TODO arreglar o cambiar
	//unsigned char digest[16];
    //compute_md5(nuevo_hash, digest);

    //Comparar lo hasheado con el hash del archivo. Si son iguales no hay sabotaje. Si difieren está saboteado el archivo
    if(strcmp(nuevo_hash, md5_archivo(recurso.basura)))
    	return 0;
    else
    	return 1;
*/
	return 0;
}

void restaurar_blocks() {
	uint32_t tamanio_archivo_basura = tamanio_archivo(path_basura);
	uint32_t tamanio_archivo_oxigeno = tamanio_archivo(path_oxigeno);
	uint32_t tamanio_archivo_comida = tamanio_archivo(path_comida);

	liberar_bloques(path_basura);
	liberar_bloques(path_oxigeno);
	liberar_bloques(path_comida);

	limpiar_metadata(path_basura);
	limpiar_metadata(path_oxigeno);
	limpiar_metadata(path_comida);

	agregar("BASURA", (int) tamanio_archivo_basura);
	agregar("OXIGENO", (int) tamanio_archivo_oxigeno);
	agregar("COMIDA", (int) tamanio_archivo_comida);
}

void recorrer_recursos(t_list* lista_bloques_ocupados) {
    // Recorre las listas de las metadatas de los recursos y va anotando en la lista que bloques estan ocupados
	int i = 0;

	//BASURA
	uint32_t cantidad_bloques_basura = cantidad_bloques_recurso(path_basura);
	t_list* lista_bloques_basura = obtener_lista_bloques(path_basura);

	for(uint32_t j = 0; j < cantidad_bloques_basura; j++) {
		reemplazar(lista_bloques_ocupados, i, list_get(lista_bloques_basura, j)); // lista_bloques_ocupados[i] =lista_bloques_basura[j]
		i++;
	}

	//COMIDA
	uint32_t cantidad_bloques_comida = cantidad_bloques_recurso(path_comida);
	t_list* lista_bloques_comida = obtener_lista_bloques(path_comida);

	for(uint32_t j = 0; j < cantidad_bloques_comida; j++) {
		reemplazar(lista_bloques_ocupados, i, list_get(lista_bloques_comida, j));
		i++;

	}

	//OXIGENO
	uint32_t cantidad_bloques_oxigeno = cantidad_bloques_recurso(path_oxigeno);
	t_list* lista_bloques_oxigeno = obtener_lista_bloques(path_oxigeno);

	for(uint32_t j = 0; j < cantidad_bloques_oxigeno; j++) {
		reemplazar(lista_bloques_ocupados, i, list_get(lista_bloques_oxigeno, j));
		i++;
	}

	free(lista_bloques_basura);
	free(lista_bloques_comida);
	free(lista_bloques_oxigeno);
}

void recorrer_bitacoras(t_list* lista_bloques_ocupados) {
	// TODO: revisar
	// Recorre las listas de las metadatas de las bitacoras y va anotando en la lista que bloques estan ocupados

	//Obtengo la cantidad de bloques de lista_bloques_ocupados y la cantidad de bitacoras
	int i = list_size(lista_bloques_ocupados);
	int cant_bitacoras = list_size(bitacoras);

	//Itero por todas las bitacoras
	for(int j = 0; j < cant_bitacoras; j++) {
		t_bitacora* bitacora = list_get(bitacoras, i);
		int cant_bloques_bitacora = list_size(bitacora->bloques);
		//Le asigno a lista_bloques_ocupados el bloque n°k de la bitacora n°j
		for(int k = 0; k < cant_bloques_bitacora; k++){
			reemplazar(lista_bloques_ocupados, i, list_get(bitacora->bloques, k));
		}
	}
}

void sortear(t_list* lista_bloques_ocupados) {
	//Obtengo la cantidad de bloques de lista_bloques_ocupados
	int n = list_size(lista_bloques_ocupados);

    int i, j;
    for (i = 0; i < n-1; i++) {
        for (j = 0; j < n-i-1; j++) {
            if (list_get(lista_bloques_ocupados, j) > list_get(lista_bloques_ocupados, j+1)) {
                int temp = (int) list_get(lista_bloques_ocupados, j);
    			reemplazar(lista_bloques_ocupados, j, list_get(lista_bloques_ocupados, j+1));
    			reemplazar(lista_bloques_ocupados, j+1, (void*) temp);
            }
        }
    }
}

int bloques_ocupados_difieren(t_list* lista_bloques_ocupados) {
	// TODO: revisar BIEN el for
    // Compara lista contra el bitmap, apenas difieren devuelve 1 (como true), sino 0
	int no_difieren;

	t_bitarray* bitmap = obtener_bitmap();

	for(int i = 0; i < CANTIDAD_BLOQUES; i++) {

		//Si el bit es 1, la lista debe contener el bloque n° i
		if(bitarray_test_bit(bitmap, i))
			no_difieren = esta_en_lista(lista_bloques_ocupados, i);

		//Si el bit es 0, la lista no debe contener el bloque n° i
		else
			no_difieren = ! esta_en_lista(lista_bloques_ocupados, i);

		//Si el flag es 0, los bloques difieren
		if(! no_difieren)
			return 1;
	}
	return 0;
}
