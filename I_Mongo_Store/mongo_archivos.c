#include "mongo_archivos.h"

// Vars globales
t_log* logger_mongo;
t_config* config_mongo;
t_list* bitacoras;

void inicializar_archivos() { // TODO: Puede romper
	// Se obtiene el path al archivo oxigeno dentro de la carpeta files
	path_oxigeno = malloc((strlen(path_files)+1) + strlen("/Oxigeno.ims"));
	sprintf(path_oxigeno, "%s/Oxigeno.ims", path_files);

	// Se obtiene el path al archivo comida dentro de la carpeta files
	path_comida = malloc((strlen(path_files)+1) + strlen("/Comida.ims"));
	sprintf(path_comida, "%s/Comida.ims", path_files);

	// Se obtiene el path al archivo basura dentro de la carpeta files
	path_basura = malloc((strlen(path_files)+1) + strlen("/Basura.ims"));
	sprintf(path_basura, "%s/Basura.ims", path_files);

	// Se obtiene el path al archivo superbloque dentro de la carpeta files (deberia ser dentro del punto de montaje nomas)
	char* path_superbloque = malloc((strlen(path_directorio)+1) + strlen("/SuperBloque.ims"));
	sprintf(path_superbloque, "%s/SuperBloque.ims", path_directorio);

	// Se obtiene el path al archivo blocks dentro de la carpeta files (deberia ser dentro del punto de montaje nomas)
	char* path_blocks = malloc((strlen(path_directorio)+1) + strlen("/Blocks.ims"));
	sprintf(path_blocks, "%s/Blocks.ims", path_directorio);

    int filedescriptor_blocks = open(path_blocks, O_RDWR | O_APPEND | O_CREAT);

	// Trunco los archivos o los creo en modo escritura y lectura
	// Se guarda to.do en un struct para uso en distintas funciones
    recurso.oxigeno     = fopen(path_oxigeno, "w+");
	recurso.comida      = fopen(path_comida, "w+");
	recurso.basura      = fopen(path_basura, "w+");
	directorio.superbloque = fopen(path_superbloque, "w+");
	directorio.blocks      = fdopen(filedescriptor_blocks, "w+");

	escribir_archivo_recurso(recurso.oxigeno, 0, 0, NULL);
	escribir_archivo_recurso(recurso.comida, 0, 0, NULL);
	escribir_archivo_recurso(recurso.basura, 0, 0, NULL);

	iniciar_superbloque(directorio.superbloque);
	iniciar_blocks(filedescriptor_blocks); // Actualizar struct
	inicializar_mapa();
}

void inicializar_archivos_preexistentes() { // TODO: Puede romper, actualizar conforme arriba
	// Se obtiene el path al archivo oxigeno dentro de la carpeta files
	path_oxigeno = malloc((strlen(path_files)+1) + strlen("/Oxigeno.ims"));
	sprintf(path_oxigeno, "%s/Oxigeno.ims", path_files);

	// Se obtiene el path al archivo comida dentro de la carpeta files
	path_comida = malloc((strlen(path_files)+1) + strlen("/Comida.ims"));
	sprintf(path_comida, "%s/Comida.ims", path_files);

	// Se obtiene el path al archivo basura dentro de la carpeta files
	path_basura = malloc((strlen(path_files)+1) + strlen("/Basura.ims"));
	sprintf(path_basura, "%s/Basura.ims", path_files);

	// Se obtiene el path al archivo superbloque dentro de la carpeta files (deberia ser dentro del punto de montaje nomas)
	path_superbloque = malloc((strlen(path_files)+1) + strlen("/SuperBloque.ims"));
	sprintf(path_superbloque, "%s/SuperBloque.ims", path_files); 

	// Se obtiene el path al archivo blocks dentro de la carpeta files (deberia ser dentro del punto de montaje nomas)
	path_blocks = malloc((strlen(path_files)+1) + strlen("/Blocks.ims"));
	sprintf(path_blocks, "%s/Blocks.ims", path_files);

    int filedescriptor_blocks = open(path_blocks, O_RDWR | O_APPEND | O_CREAT);

	// Abro los archivos en modo escritura y lectura (deben existir archivos)
	// Se guarda to.do en un struct para uso en distintas funciones
	recurso.oxigeno     = fopen(path_oxigeno, "r+");
	recurso.comida      = fopen(path_comida, "r+");
	recurso.basura      = fopen(path_basura, "r+");
	directorio.superbloque = fopen(path_superbloque, "r+");
	directorio.blocks      = fdopen(filedescriptor_blocks, "r+");

	// TODO: Verificar si esta mappeado
	iniciar_blocks(filedescriptor_blocks); // Actualizar struct
	inicializar_mapa();
}

// Verificar bitmap de superbloque
// Elegir el primer libre
// Ocuparlo en bitmap
// Se lo asigna a archivo
// Llenar metadata con nuevos datos (distinguir entre tripu y recursos)

void asignar_nuevo_bloque(FILE* archivo) {

	fseek(directorio.superbloque, strlen("BLOCK_SIZE="), SEEK_SET);
	uint32_t tamanio_bloque;
	fread(&tamanio_bloque, sizeof(uint32_t), 1, directorio.superbloque);

	fseek(directorio.superbloque, strlen("BLOCK_COUNT="), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, directorio.superbloque);

	t_bitarray* bitmap = malloc(sizeof(t_bitarray));
	bitmap = bitarray_create_with_mode(bitmap->bitarray, cant_bloques, LSB_FIRST); //Puede romper
	fread(bitmap->bitarray, 1/CHAR_BIT, cant_bloques, directorio.superbloque); //CHAR_BIT: represents the number of bits in a char
	int bit_libre = -1;

	//Recorro todas las pociciones del bitarray
	for (uint32_t i = 0; i < cant_bloques; i++){
		//Entra si el bit del bitmap está en 0 y no se encontro bit_libre (< 0). Se puede mejorar
		if(!bitarray_test_bit(bitmap, i) && bit_libre < 0){
			bit_libre = i;
			break;
		}
	}

	//Si había un bloque libre
	if (bit_libre >= 0) {
		//Marco el bit como ocupado
		bitarray_set_bit(bitmap, bit_libre);

		if(es_recurso(archivo)){
			//Asigno el bloque a un archivo
			asignar_bloque_recurso(archivo, bit_libre);

			//Actualizo la metadata del archivo
			uint32_t tamanio = tamanio_archivo(archivo);
			uint32_t cantidad_bloques = cantidad_bloques_recurso(archivo) + 1;
			uint32_t* lista_bloques = lista_bloques_recurso(archivo);
			lista_bloques[cantidad_bloques] = bit_libre; //Agrega el nuevo bloque al final de la lista de bloques preexistente

			escribir_archivo_recurso(archivo, tamanio, cantidad_bloques, lista_bloques);
		}
		else {
			asignar_bloque_tripulante(archivo, bit_libre);

			//Actualizo la metadata del archivo
			uint32_t tamanio = tamanio_archivo(archivo);
			uint32_t* lista_bloques = lista_bloques_tripulante(archivo);
			uint32_t cantidad_bloques = sizeof(lista_bloques)/sizeof(uint32_t);
			lista_bloques[cantidad_bloques] = bit_libre; //Agrega el nuevo bloque al final de la lista de bloques preexistente

			escribir_archivo_tripulante(archivo, tamanio, lista_bloques);
		}
	}
	//Si no había un bloque libre
	else
		log_info(logger_mongo, "No hay bloques disponibles en este momento");
}

int asignar_primer_bloque_libre(uint32_t* lista_bloques, uint32_t cant_bloques, int cantidad_deseada, char tipo) { // ESPANTOSO, fijarse si funca, puede explotar por ser un void* (desplazamiento numerico tiene que ser bytes para que funque)
	int cantidad_alcanzada = 0;

	for(int j = 0; j < cant_bloques; j++) {
		for (int i = 0; tipo != *(directorio.mapa_blocks + lista_bloques[j] * TAMANIO_BLOQUE + i + 1) && *(directorio.mapa_blocks + lista_bloques[j] * TAMANIO_BLOQUE + i + 1) != ' '; i++) { // Cambiar Macro por revision al Superbloque
			
			if (*(directorio.mapa_blocks + lista_bloques[j] * TAMANIO_BLOQUE + i) == ' ') {
				*(directorio.mapa_blocks + lista_bloques[j] * TAMANIO_BLOQUE + i) = tipo;
				cantidad_alcanzada++;
			}

			if (cantidad_alcanzada == cantidad_deseada) {
				return j * 100 + i;
			}
		}
	}
	
	return cantidad_alcanzada - cantidad_deseada;
}

int quitar_ultimo_bloque_libre(uint32_t* lista_bloques, uint32_t cant_bloques, int cantidad_deseada, char tipo) {
	int cantidad_alcanzada = 0;

	for(int j = cant_bloques; j < 0; j--) {
		for (int i = TAMANIO_BLOQUE; tipo != *(directorio.mapa_blocks + (lista_bloques[j] + 1) * TAMANIO_BLOQUE - i - 1) && *(directorio.mapa_blocks + (lista_bloques[j] + 1) * TAMANIO_BLOQUE - i - 1) != ' '; i--) { // Cambiar Macro por revision al Superbloque
			
			if (*(directorio.mapa_blocks + (lista_bloques[j] + 1) * TAMANIO_BLOQUE - i) == tipo) {
				*(directorio.mapa_blocks + (lista_bloques[j] + 1) * TAMANIO_BLOQUE - i) = ' ';
				cantidad_alcanzada++;
			}

			if (cantidad_alcanzada == cantidad_deseada) {
				return j * 100 + i;
			}
		}
	}
	
	return cantidad_alcanzada - cantidad_deseada;
}

void actualizar_MD5(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR); 
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	fseek(archivo, strlen("CARACTER_LLENADO="), SEEK_CUR);
	char tipo;
	fread(&tipo, sizeof(char), 1, archivo);

	char* path_archivo = conseguir_path_recurso_archivo(archivo);
	archivo = fopen(path_archivo , "w+"); //Se actualiza struct?

	char* size = "SIZE=";
	char* block_count = "BLOCK_COUNT=";
	char* blocks = "BLOCKS=[";
	char* corchete_cierre = "]";
	char* caracter = "CARACTER_LLENADO=";
	char* md5 = "MD5_ARCHIVO=";
	char* md5_dato = crear_md5();

	fwrite(&size, strlen(size), 1, archivo);
	fwrite(&tamanio_archivo, sizeof(uint32_t), 1, archivo);
	fwrite(&block_count, strlen(block_count), 1, archivo);
	fwrite(&cant_bloques, sizeof(uint32_t), 1, archivo);
	fwrite(&blocks, strlen(blocks), 1, archivo);
	fwrite(lista_bloques, sizeof(uint32_t), cant_bloques, archivo);
	fwrite(&corchete_cierre, strlen(corchete_cierre), 1, archivo);
	fwrite(&caracter, strlen(caracter), 1, archivo);
	fwrite(&tipo, sizeof(char), 1, archivo);
	fwrite(&md5, strlen(md5), 1, archivo);
	fwrite(&md5_dato, strlen(md5_dato), 1, archivo);

	free(size);
	free(block_count);
	free(blocks);
	free(corchete_cierre);
	free(caracter);
	free(md5);
	free(md5_dato);
}

void alterar(int codigo_archivo, int cantidad) {  
	if (cantidad >= 0){
		agregar(codigo_archivo, cantidad);
		log_info(logger_mongo, "Se agregaron %s unidades a %s.\n", string_itoa(cantidad), conseguir_tipo(conseguir_char(codigo_archivo)));
	}
	else{
		quitar(codigo_archivo, cantidad);
		log_info(logger_mongo, "Se quitaron %s unidades a %s.\n", string_itoa(cantidad), conseguir_tipo(conseguir_char(codigo_archivo)));
	}
}

void agregar(int codigo_archivo, int cantidad) { // Puede que haya que hacer mallocs previos
	FILE* archivo = conseguir_archivo_recurso(codigo_archivo);
	pthread_mutex_lock(&mutex_blocks); // Declarar mutex

	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR); 
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	fseek(archivo, strlen("CARACTER_LLENADO="), SEEK_CUR);
	char tipo;
	fread(&tipo, sizeof(char), 1, archivo);

	int offset = asignar_primer_bloque_libre(lista_bloques, cant_bloques, cantidad, tipo);

	if (offset < 0) { // Falto agregar cantidad, dada por offset
		asignar_nuevo_bloque(archivo);
		agregar(tipo, offset * -1); // Recursividad con la cantidad que falto
	}
	else if (offset < 100) { // No paso bloques. ¿No sería TAMANIO_BLOQUES?
		msync(directorio.mapa_blocks, offset + 1, MS_SYNC);
		sleep(config_get_int_value(config_mongo, "TIEMPO_SINCRONIZACION"));
	}
	else if (offset > 100) { // Se paso bloques
		int cant_bloques_local = offset / 100;
		offset = offset % 100;
		
		msync(directorio.mapa_blocks, cant_bloques_local * TAMANIO_BLOQUE + offset + 1, MS_SYNC); // Cambiar macro por lo de Superbloque. Falta el flag, puse MS_ASYNC, ni idea cual va. Link: https://man7.org/linux/man-pages/man2/msync.2.html
		sleep(config_get_int_value(config_mongo, "TIEMPO_SINCRONIZACION"));
	}

	actualizar_MD5(archivo);

    pthread_mutex_unlock(&mutex_blocks);
}

void quitar(int codigo_archivo, int cantidad) { // Puede explotar en manejo de fopens, revisar
	FILE* archivo = conseguir_archivo_recurso(codigo_archivo);
	pthread_mutex_lock(&mutex_blocks);

	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR); 
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	fseek(archivo, strlen("CARACTER_LLENADO="), SEEK_CUR);
	char tipo;
	fread(&tipo, sizeof(char), 1, archivo);

	int offset = quitar_ultimo_bloque_libre(lista_bloques, cant_bloques, cantidad * -1, tipo);

	if (offset < 0) { // Se quiso quitar mas de lo existente, no hace nada (queda para comprension)
	}
	else if (offset < 100) { // No paso bloques
		msync(directorio.mapa_blocks, lista_bloques[cant_bloques - 1] * TAMANIO_BLOQUE + 1, MS_SYNC);
		sleep(config_get_int_value(config_mongo, "TIEMPO_SINCRONIZACION"));
	}
	else if (offset > 100) { // Se paso bloques
		int cant_bloques_local = offset / 100;
		offset = offset % 100;
		
		msync(directorio.mapa_blocks, lista_bloques[(cant_bloques_local - 1)] * TAMANIO_BLOQUE + offset + 1, MS_SYNC);
		sleep(config_get_int_value(config_mongo, "TIEMPO_SINCRONIZACION"));
	}

	actualizar_MD5(archivo);

    pthread_mutex_unlock(&mutex_blocks);
}

char conseguir_char(int codigo_operacion) {
	switch(codigo_operacion) {
	case OXIGENO:
		return 'O';
		break;
	case COMIDA:
		return 'C';
		break;
	case BASURA:
		return 'B';
		break;
	}
	return '\0';
}

char* conseguir_tipo(char tipo) {
	if (tipo == 'O')
        return "Oxigeno";
    if (tipo == 'C')
        return "Comida";
    if (tipo == 'B')
        return "Basura";
    return NULL;
}

char* conseguir_path_recurso(int codigo_archivo) {
	switch(codigo_archivo) {
	case BASURA:
		return path_basura;
		break;
	case COMIDA:
		return path_comida;
		break;
	case OXIGENO:
		return path_oxigeno;
		break;
	}
	log_error(logger_mongo, "Archivo de recurso no encontrado");
	return "No encontrado";
}

FILE* conseguir_archivo_char(char tipo) {
	if (tipo == 'O')
        return recurso.oxigeno;
    if (tipo == 'C')
        return recurso.comida;
    if (tipo == 'B')
        return recurso.basura;
    return NULL;
}

char* conseguir_path_recurso_codigo(int codigo) {
	switch(codigo) {
		case OXIGENO:
			return path_oxigeno;
			break;
		case COMIDA:
			return path_comida;
			break;
		case BASURA:
			return path_basura;
			break;
	}
	return NULL;
}

char* conseguir_path_recurso_archivo(FILE* archivo) {
	int codigo_archivo;
	char caracter = caracter_llenado_archivo(archivo);

	switch(caracter) {
	case 'O':
		codigo_archivo = OXIGENO;
		return conseguir_path_recurso(codigo_archivo);
		break;

	case 'B':
		codigo_archivo = BASURA;
		return conseguir_path_recurso(codigo_archivo);
		break;
	case 'C':
		codigo_archivo = COMIDA;
		return conseguir_path_recurso(codigo_archivo);
		break;
	}

	log_info(logger_mongo, "No se pudo conseguir el path del recurso");
	return "El archivo no era un recurso";
}

int max (int a, int b) {
	if (a >= b) {
		return a;
	}
	else {
		return b;
	}
}

char* crear_md5() { // String de 32
	char* md5 = malloc(sizeof(char) * 32);

	for (int i = 0; i < 32; i++){
		md5[i] = char_random();
	}

	return md5;
}

char char_random() {

	srand(time(NULL));
	int seleccion = rand() % 2;

	switch (seleccion) {
		case 0:
			return (char) (rand() % 9 + 48); // Devuelve un numero por ASCII
			break;
		case 1:
			return (char) (rand() % 26 + 65); // Devuelve un alfa por ASCII
			break;
	}

	return '\0';
}

uint32_t tamanio_archivo(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	return tamanio_archivo;
}

uint32_t cantidad_bloques_recurso(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	return cant_bloques;
}

uint32_t* lista_bloques_recurso(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	return lista_bloques;
}

char caracter_llenado_archivo(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	char caracter_llenado;
	fseek(archivo, strlen("CARACTER_LLENADO="), SEEK_CUR);
	fread(&caracter_llenado, sizeof(char), 1, archivo);

	return caracter_llenado;
}

char* md5_archivo(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	fseek(archivo, strlen("CARACTER_LLENADO="), SEEK_CUR);
	char tipo;
	fread(&tipo, sizeof(char), 1, archivo);

	fseek(archivo, strlen("MD5_ARCHIVO="), SEEK_CUR);
	char* md5;
	fread(&md5, sizeof(char), 32, archivo);

	return md5;
}

uint32_t cantidad_bloques_tripulante(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	return cant_bloques;
}

uint32_t* lista_bloques_tripulante(FILE* archivo) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	return lista_bloques;
}

void escribir_archivo_recurso(FILE* archivo, uint32_t tamanio, uint32_t cantidad_bloques, uint32_t* list_bloques) {
	fseek(archivo, strlen("SIZE="), SEEK_SET);
	uint32_t tamanio_archivo;
	fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCK_COUNT="), SEEK_CUR);
	uint32_t cant_bloques;
	fread(&cant_bloques, sizeof(uint32_t), 1, archivo);

	fseek(archivo, strlen("BLOCKS=["), SEEK_CUR);
	uint32_t* lista_bloques = malloc(sizeof(uint32_t) * cant_bloques);
	for (int i = 0; i < cant_bloques; i++) {
		fread(&lista_bloques[i], sizeof(uint32_t), 1, archivo);
	}
	fseek(archivo, strlen("]"), SEEK_CUR);

	fseek(archivo, strlen("CARACTER_LLENADO="), SEEK_CUR);
	char tipo;
	fread(&tipo, sizeof(char), 1, archivo);

	fseek(archivo, strlen("MD5_ARCHIVO="), SEEK_CUR);
	char* md5 = crear_md5();

	char* path_archivo = conseguir_path_recurso_archivo(archivo);
	freopen(path_archivo, "w+", archivo);

	char* size = "SIZE=";
	char* block_count = "BLOCK_COUNT=";
	char* blocks = "BLOCKS=[";
	char* corchete_cierre = "]";
	char* caracter = "CARACTER_LLENADO=";
	char* md_5 = "MD5_ARCHIVO=";

	fwrite(&size, strlen(size), 1, archivo);
	fwrite(&tamanio, sizeof(uint32_t), 1, archivo);

	fwrite(&block_count, strlen(block_count), 1, archivo);
	fwrite(&cantidad_bloques, sizeof(uint32_t), 1, archivo);
	fwrite(&blocks, strlen(blocks), 1, archivo);
	fwrite(list_bloques, sizeof(uint32_t), cant_bloques, archivo);
	fwrite(&corchete_cierre, strlen(corchete_cierre), 1, archivo);
	fwrite(&caracter, strlen(caracter), 1, archivo);
	fwrite(&tipo, sizeof(char), 1, archivo);
	fwrite(&md_5, strlen(md_5), 1, archivo);
	fwrite(&md5, strlen(md5), 1, archivo);

	free(size);
	free(block_count);
	free(blocks);
	free(corchete_cierre);
	free(caracter);
	free(md_5);
}

void escribir_archivo_tripulante(FILE* archivo, uint32_t tamanio, uint32_t* list_bloques) {
	char* size = "SIZE=";
	char* blocks = "BLOCKS=[";
	char* corchete_cierre = "]";

	fwrite(&size, strlen(size), 1, archivo);
	fwrite(&tamanio, sizeof(uint32_t), 1, archivo);
	fwrite(&blocks, strlen(blocks), 1, archivo);
	uint32_t cant_bloques = sizeof(list_bloques); //Puede ser que sea sizeof(list_bloques)/2; por un tema del sizeof()
	fwrite(&corchete_cierre, strlen(corchete_cierre), 1, archivo);
	fwrite(list_bloques, sizeof(uint32_t), cant_bloques, archivo);

	free(size);
	free(blocks);
	free(corchete_cierre);
}

int es_recurso(FILE* archivo) { //Solo sirve si en las bitácoras escribimos to.do en minúscula
	char* nombre = conseguir_path_recurso_archivo(archivo);
	int boolean = strcmp(nombre, "El archivo no era un recurso") ? 0 : 1;
	return boolean;
}

void asignar_bloque_recurso(FILE* archivo, int bit_libre) {
	uint32_t tamanio = tamanio_archivo(archivo);
	uint32_t cantidad_bloques = cantidad_bloques_recurso(archivo);
	uint32_t* lista_bloques = lista_bloques_recurso(archivo);
	lista_bloques[cantidad_bloques] = bit_libre;

	escribir_archivo_recurso(archivo, tamanio, cantidad_bloques + 1, lista_bloques);
}

void asignar_bloque_tripulante(FILE* archivo, int bit_libre) {
	uint32_t tamanio = tamanio_archivo(archivo);
	uint32_t* lista_bloques = lista_bloques_tripulante(archivo);
	uint32_t cantidad_bloques = cantidad_bloques_tripulante(archivo);
	lista_bloques[cantidad_bloques] = bit_libre;

	escribir_archivo_tripulante(archivo, tamanio, lista_bloques);
}
