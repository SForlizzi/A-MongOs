#include "mongo_sabotaje.h"

char** posiciones_sabotajes;//TODO = config_get_array_value(config_mongo, "POSICIONES_SABOTAJE");

void enviar_posicion_sabotaje(int socket_discordiador) {

	if (posiciones_sabotajes != NULL) { //El último parámetro es NULL
		t_posicion posicion;

		//Consigo la primera posicion del char** posiciones_sabotajes
		posicion.coord_x = (uint32_t) posiciones_sabotajes[0][0];
		posicion.coord_y = (uint32_t) posiciones_sabotajes[0][2];

		empaquetar_y_enviar(serializar_posicion(posicion), POSICION, socket_discordiador);

		//Remuevo la primera posición del char** posiciones_sabotajes
		posiciones_sabotajes = posiciones_sabotajes + 1; //Puede romper
	}
	else
		log_warning(logger_mongo, "No hay posiciones de sabotaje");
}

char* reparar() {
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
    int cant_bloques = obtener_cantidad_bloques();
    int cantidad_real = sizeof(directorio.mapa_blocks)/sizeof(char); // Verificar que asi obtenga tamanio

    if (cant_bloques != cantidad_real) {
        int tamanio = obtener_tamanio_bloque();
        obtener_cantidad_bloques(); // Para desplazamiento only

        t_bitarray* bitmap = obtener_bitmap();
        reescribir_superbloque(tamanio, cantidad_real, bitmap);

        return 1;
    }
    else
        return 0;
}

int verificar_bitmap() {
	//Creo la lista
	int* lista_bloques_ocupados = malloc(sizeof(int) * obtener_cantidad_bloques());

	//Agrego los bloques usados en la lista, con en el indice = n° bloque
    recorrer_recursos(lista_bloques_ocupados);
    recorrer_bitacoras(lista_bloques_ocupados);

    sortear(lista_bloques_ocupados);

    if (bloques_ocupados_difieren(lista_bloques_ocupados)) {
        t_bitarray* bitmap = actualizar_bitmap(lista_bloques_ocupados);
        reescribir_superbloque(obtener_tamanio_bloque(), obtener_cantidad_bloques(), bitmap);

        return 2;
    }
    else
        return 0;

}

int verificar_sizes() {
    // Compara tamanio archivo vs lo que ocupa en sus blocks, uno por uno, si alguna vez rompio, devuelve 3, sino 0

	int tamanio_real_B = bloques_contar('B');
	int tamanio_real_C = bloques_contar('C');
	int tamanio_real_O = bloques_contar('O');

	int corrompido = 0;

	if(tamanio_real_B != (int) tamanio_archivo(recurso.basura)) {
		escribir_tamanio(recurso.basura, tamanio_real_B);
		corrompido = 3;
	}
	if(tamanio_real_C != (int) tamanio_archivo(recurso.comida)) {
		escribir_tamanio(recurso.comida, tamanio_real_C);
		corrompido = 3;
	}
	if(tamanio_real_O != (int) tamanio_archivo(recurso.oxigeno)) {
		escribir_tamanio(recurso.oxigeno, tamanio_real_O);
		corrompido = 3;
	}

	return corrompido;
}

int verificar_block_counts(t_TCB* tripulante) { 
    // Compara block count vs el largo de la lista de cada archivo recurso. Devuelve 4 si algún recurso fue corrompido
	uint32_t cantidad_real_basura  = sizeof(lista_bloques_recurso(recurso.basura)) / sizeof(uint32_t);
	uint32_t cantidad_real_comida  = sizeof(lista_bloques_recurso(recurso.comida)) / sizeof(uint32_t);
	uint32_t cantidad_real_oxigeno = sizeof(lista_bloques_recurso(recurso.oxigeno)) / sizeof(uint32_t);

	int corrompido = 0;

	if(cantidad_real_oxigeno != cantidad_bloques_recurso(recurso.oxigeno)) {
		escribir_tamanio(recurso.oxigeno, cantidad_real_oxigeno);
		corrompido = 4;
	}
	if(cantidad_real_comida  != cantidad_bloques_recurso(recurso.comida)) {
		escribir_tamanio(recurso.comida, cantidad_real_comida);
		corrompido = 4;
	}
	if(cantidad_real_basura  != cantidad_bloques_recurso(recurso.basura)) {
		escribir_tamanio(recurso.basura, cantidad_real_basura);
		corrompido = 4;
	}
	return corrompido;
}

int verificar_blocks() {
    // TODO
	//Por cada archivo de recurso:
	//Concatenar los bloques de la lista de bloques
	//Hashear lo concatenado (Osea si la lista es [1,4,2], debo hashear a MD5 la cadena 142)
	//Comparar lo hasheado con el hash del archivo. Si son iguales no hay sabotaje. Si difieren está saboteado el archivo
	//¿Cómo reparar el archivo?
	//Reescribir tantos caracteres de llenado como hagan falta hasta llenar el size del archivo
	//Supongo que el último bloque debería completarlo de basura.

	int lbs_basura  = lista_blocks_saboteada(recurso.basura);
	int lbs_comida  = lista_blocks_saboteada(recurso.comida);
	int lbs_oxigeno = lista_blocks_saboteada(recurso.oxigeno);
	int flag = 0;

	if (lbs_basura) {
		reparar(recurso.basura);
		flag = 5;
	}
	if (lbs_comida) {
		reparar(recurso.comida);
		flag = 5;
	}
	if (lbs_oxigeno) {
		reparar(recurso.oxigeno);
		flag = 5;
	}
	return flag;
}

int lista_blocks_saboteada(FILE* archivo) {
/*
	//Concatenar los bloques de la lista de bloques
	char* nuevo_hash = string_new();
	int* lista_bloques = (int*) lista_bloques_recurso(recurso.basura);
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

void reparar_blocks() {
	//TODO
	//Reescribir tantos caracteres de llenado como hagan falta hasta llenar el size del archivo
	//Supongo que el último bloque debería completarlo de basura. Basura en nuestro caso es \t
}

void recorrer_recursos(int* lista_bloques_ocupados) {
    // Recorre las listas de las metadatas de los recursos y va anotando en la lista que bloques estan ocupados
	int i = 0;

	//BASURA
	uint32_t cantidad_bloques_basura = cantidad_bloques_recurso(recurso.basura);
	uint32_t* lista_bloques_basura = lista_bloques_recurso(recurso.basura);

	for(uint32_t j = 0; j < cantidad_bloques_basura; j++) {
		lista_bloques_ocupados[i] = (int) lista_bloques_basura[j];
		i++;
	}

	//COMIDA
	uint32_t cantidad_bloques_comida = cantidad_bloques_recurso(recurso.comida);
	uint32_t* lista_bloques_comida = lista_bloques_recurso(recurso.comida);

	for(uint32_t j = 0; j < cantidad_bloques_comida; j++) {
		lista_bloques_ocupados[i] = (int) lista_bloques_comida[j];
		i++;
	}

	//OXIGENO
	uint32_t cantidad_bloques_oxigeno = cantidad_bloques_recurso(recurso.oxigeno);
	uint32_t* lista_bloques_oxigeno = lista_bloques_recurso(recurso.oxigeno);

	for(uint32_t j = 0; j < cantidad_bloques_oxigeno; j++) {
		lista_bloques_ocupados[i] =(int) lista_bloques_oxigeno[j];
		i++;
	}

	free(lista_bloques_basura);
	free(lista_bloques_comida);
	free(lista_bloques_oxigeno);
}

void recorrer_bitacoras(int* lista_bloques_ocupados) {
	// Recorre las listas de las metadatas de las bitacoras y va anotando en la lista que bloques estan ocupados

	//Obtengo la cantidad de bloques de lista_bloques_ocupados y la cantidad de bitacoras
	int i = sizeof(lista_bloques_ocupados) / sizeof(int);
	int cant_bitacoras = list_size(bitacoras);

	//Itero por todas las bitacoras
	for(int j = 0; j < cant_bitacoras; j++) {
		t_bitacora* bitacora = list_get(bitacoras, i);
		int cant_bloques_bitacora = (int) sizeof(bitacora->bloques) / sizeof(int);
		//Le asigno a lista_bloques_ocupados el bloque n°k de la bitacora n°j
		for(int k = 0; k < cant_bloques_bitacora; k++)
			lista_bloques_ocupados[i] = bitacora->bloques[k];
		i++;
	}
}

void sortear(int* lista_bloques_ocupados) {
	//Obtengo la cantidad de bloques de lista_bloques_ocupados
	int n = sizeof(lista_bloques_ocupados) / sizeof(int);

    int i, j;
    for (i = 0; i < n-1; i++) {
        for (j = 0; j < n-i-1; j++) {
            if (lista_bloques_ocupados[j] > lista_bloques_ocupados[j+1]) {
                int temp = lista_bloques_ocupados[j];
                lista_bloques_ocupados[j] = lista_bloques_ocupados[j+1];
                lista_bloques_ocupados[j+1] = temp;
            }
        }
    }
}

int bloques_ocupados_difieren(int* lista_bloques_ocupados) {
    // Compara lista contra el bitmap, apenas difieren devuelve 1 (como true), sino 0
	int no_difieren;

	for(int i = 0; i < CANTIDAD_BLOQUES; i++) {
		t_bitarray* bitmap = obtener_bitmap();

		//Si el bit es 1, la lista debe contener el bloque n° i
		if(bitarray_test_bit(bitmap, i))
			no_difieren = contiene(lista_bloques_ocupados, i);

		//Si el bit es 0, la lista no debe contener el bloque n° i
		else
			no_difieren = ! contiene(lista_bloques_ocupados, i);

		//Si el flag es 0, los bloques difieren
		if(! no_difieren)
			return 1;
	}
	return 0;
}

int contiene(int* lista, int valor) {
	int tamanio_lista = (int) sizeof(lista) / sizeof(int);

	for(int i = 0; i < tamanio_lista; i++) {
		if(lista[i] == valor)
			return 1;
	}
	return 0;
}
