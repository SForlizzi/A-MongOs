#include "mongo_blocks.h"
#include "comms/generales.h"

#include <fcntl.h>

t_log* logger_mongo;
t_config* config_mongo;
t_config* config_superbloque;
t_directorio directorio;
t_recurso recurso;
t_list* bitacoras;

char* path_directorio;
char* path_files;
char* path_bitacoras;
char* path_oxigeno;
char* path_comida;
char* path_basura;
char* path_superbloque;
char* path_blocks;

char* mapa;

void iniciar_superbloque(FILE* archivo) {
	// TODO podria ser un mmap
	log_trace(logger_mongo, "INICIO iniciar_superbloque");
    uint32_t block_size = TAMANIO_BLOQUE; // Bytes
    uint32_t size = CANTIDAD_BLOQUES;

    void* puntero_a_bits = malloc(size/8);
    t_bitarray* bitmap = bitarray_create_with_mode(puntero_a_bits, size/8, LSB_FIRST); // SE DIVIDE POR OCHO PORQUE EL SIZE ES EN BYTES, PONER 1 SIGNIFICA CREAR UN BITARRAY DE 8 BITS

    log_info(logger_mongo,"Cantidad de bloques: %i", CANTIDAD_BLOQUES);
    log_info(logger_mongo,"size: %i", size);
    log_info(logger_mongo,"size bitmap: %i", bitmap->size);

    for(int i = 0; i < size; i++) {
 	   bitarray_clean_bit(bitmap, i);
    }

    fwrite(&block_size, sizeof(uint32_t), 1, archivo);
    fwrite(&size, sizeof(uint32_t), 1, archivo);
    fflush(archivo);

    fwrite(bitmap->bitarray, bitmap->size, 1, archivo);
    fflush(archivo);
    bitarray_destroy(bitmap);
	log_trace(logger_mongo, "fin iniciar_superbloque");
}

void iniciar_blocks(int filedescriptor_blocks) {
    log_trace(logger_mongo, "0 iniciar_blocks");

    uint32_t block_size = TAMANIO_BLOQUE;
    uint32_t size = CANTIDAD_BLOQUES;

    mapa = (void*) mmap(NULL, block_size * size, PROT_READ | PROT_WRITE, MAP_SHARED, filedescriptor_blocks, 0); // Revisar flags

    if(mapa == MAP_FAILED){
        log_trace(logger_mongo, "Fallo el mapa, posiblemente por tamanio");
    }

    directorio.mapa_blocks = malloc(block_size * size);

    log_trace(logger_mongo, "block size * size: %i * %i = %i", block_size, size, block_size * size);

    posix_fallocate(filedescriptor_blocks, 0, block_size * size);

    memcpy(directorio.mapa_blocks, mapa, block_size * size);

    log_trace(logger_mongo, "fin iniciar_blocks");

}

void inicializar_mapa() {
	log_trace(logger_mongo, "0 inicializar_mapa");

	memcpy(directorio.mapa_blocks, mapa, CANTIDAD_BLOQUES * TAMANIO_BLOQUE);

	for (int i = 0; i < CANTIDAD_BLOQUES * TAMANIO_BLOQUE ; i++) {
        *(directorio.mapa_blocks + i) = ',';
	}

	memcpy(mapa, directorio.mapa_blocks, CANTIDAD_BLOQUES * TAMANIO_BLOQUE);
    msync(mapa, CANTIDAD_BLOQUES * TAMANIO_BLOQUE, MS_ASYNC);

	log_trace(logger_mongo, "fin inicializar_mapa");
}

uint32_t obtener_tamanio_bloque_superbloque() {
	uint32_t block_size;
    fseek(directorio.superbloque, 0, SEEK_SET);
    fread(&block_size, sizeof(uint32_t), 1, directorio.superbloque);
    return block_size;
}

uint32_t obtener_cantidad_bloques_superbloque() {
    obtener_tamanio_bloque_superbloque();

    uint32_t size;
    fread(&size, sizeof(uint32_t), 1, directorio.superbloque);

    return size;
}

t_bitarray* obtener_bitmap() {

	char* puntero_a_bitmap = crear_puntero_a_bitmap();
	t_bitarray* bitmap = bitarray_create_with_mode(puntero_a_bitmap, CANTIDAD_BLOQUES/8, MSB_FIRST);

	return bitmap;
}

void reescribir_superbloque(uint32_t tamanio, uint32_t cantidad, t_bitarray* bitmap) {

	log_trace(logger_mongo, "0 reescribir_superbloque");

	fseek(directorio.superbloque, 0, SEEK_SET);
    fwrite(&tamanio, sizeof(uint32_t), 1, directorio.superbloque);
    fwrite(&cantidad, sizeof(uint32_t), 1, directorio.superbloque);
    fwrite(bitmap->bitarray, CANTIDAD_BLOQUES/8, 1, directorio.superbloque);
	log_trace(logger_mongo, "fflusheando_superbloque");
    fflush(directorio.superbloque);

}

void actualizar_bitmap(t_list* lista_bloques_ocupados) {

	log_trace(logger_mongo, "0 actualizar bitmap");
    t_bitarray* bitmap = obtener_bitmap();
/*
    for(int i = 0; i < CANTIDAD_BLOQUES; i++) {
    	bitarray_clean_bit(bitmap, i); // limpio el bitarray, total despues le meto los que tengo registrados que andan ocupados
    }
*/
    for(int i = 0; i < CANTIDAD_BLOQUES; i++) {
    	if(esta_en_lista(lista_bloques_ocupados, i)){
    		bitarray_set_bit(bitmap, i);
    	} else{
    		bitarray_clean_bit(bitmap, i);
    	}
    }
    /*
    log_trace(logger_mongo, "Bitmap viejo:");
    imprimir_bitmap();
    log_trace(logger_mongo, "Bitmap a actualizar:");

	for(int i = 0; i < bitarray_get_max_bit(bitmap); i+=8){
		log_debug(logger_mongo, "BYTE %i  %i%i%i%i%i%i%i%i", i/8,  bitarray_test_bit(bitmap, i), bitarray_test_bit(bitmap, i+1), bitarray_test_bit(bitmap, i+2), bitarray_test_bit(bitmap, i+3), bitarray_test_bit(bitmap, i+4), bitarray_test_bit(bitmap, i+5), bitarray_test_bit(bitmap, i+6), bitarray_test_bit(bitmap, i+7));
	}
*/
    reescribir_superbloque(TAMANIO_BLOQUE, CANTIDAD_BLOQUES, bitmap);

    bitarray_destroy(bitmap);
}

void reemplazar(t_list* lista, int index, void* elemento){
	list_replace(lista, index, elemento);
	/*
	void liberar(void* un_elemento){
		free(un_elemento);
	}

	list_replace_and_destroy_element(lista, index, elemento, liberar);
	*/
}

void cargar_bitmap(){

	t_bitarray* bitmap = obtener_bitmap();
	int* aux;
	for(int i = 0; i < CANTIDAD_BLOQUES; i++){
		if(bitarray_test_bit(bitmap, i)){
			aux = malloc(sizeof(int));
			*aux = i;
			list_add(lista_bloques_ocupados, aux);
		}
	}
}
