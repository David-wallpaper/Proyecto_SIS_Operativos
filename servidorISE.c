#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_AGENTES 4
#define NOMBRE_MAX 32
#define PIPE_NOMBRE_POR_DEFECTO "TORVALDS"

typedef enum {
    MENSAJE_INICIO = 1,
    MENSAJE_ENCUESTA = 2,
    MENSAJE_FIN = 3
} TipoMensaje;

typedef struct {
    int tipo;
    pid_t pid;
    char pais[NOMBRE_MAX];
    char sector[NOMBRE_MAX];
    int id_sector;
    double sentimiento;
} MensajeEncuesta;

typedef struct {
    MensajeEncuesta *elementos;
    size_t capacidad;
    size_t entrada;
    size_t salida;
    sem_t libres;
    sem_t ocupados;
    pthread_mutex_t mutex;
} BufferAgente;

typedef struct {
    int activo;
    int finalizado;
    pid_t pid;
    char pais[NOMBRE_MAX];
    unsigned long cantidad;
    double suma;
    BufferAgente buffer;
    pthread_t hilo;
} Agente;

typedef struct {
    char pipe_nombre[256];
    size_t tam_buffer;
    int segundos;
    int cantidad_agentes;
} Configuracion;

static void mostrar_uso(const char *programa) {
    fprintf(stderr,
            "Uso: %s -b <tamB> -p <PipeN> -t <time> -a <hilos>\n"
            "  -b  capacidad del buffer de cada agente (registros)\n"
            "  -p  nombre del FIFO nominal (recomendado: TORVALDS)\n"
            "  -t  tiempo de ejecucion, entre 30 y 120 segundos\n"
            "  -a  cantidad maxima de agentes, entre 1 y 4\n",
            programa);
}

static int convertir_entero(const char *texto, int *resultado) {
    char *fin;

    errno = 0;
    long numero = strtol(texto, &fin, 10);

    if (fin == texto || *fin != '\0' ||
        errno == ERANGE ||
        numero < INT_MIN || numero > INT_MAX) {
        return -1;
    }

    *resultado = (int)numero;
    return 0;
}

static int leer_configuracion(int argc, char **argv, Configuracion *config) {
    int vistos_b = 0;
    int vistos_p = 0;
    int vistos_t = 0;
    int vistos_a = 0;
    int valor;

    memset(config, 0, sizeof(*config));
    strncpy(config->pipe_nombre, PIPE_NOMBRE_POR_DEFECTO,
            sizeof(config->pipe_nombre) - 1);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            if (vistos_b || i + 1 >= argc ||
                convertir_entero(argv[++i], &valor) != 0) {
                return -1;
            }
            if (valor <= 0) {
                return -1;
            }
            config->tam_buffer = (size_t)valor;
            vistos_b = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            if (vistos_p || i + 1 >= argc ||
                strlen(argv[i + 1]) >= sizeof(config->pipe_nombre)) {
                return -1;
            }
            strncpy(config->pipe_nombre, argv[++i],
                    sizeof(config->pipe_nombre) - 1);
            vistos_p = 1;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (vistos_t || i + 1 >= argc ||
                convertir_entero(argv[++i], &config->segundos) != 0) {
                return -1;
            }
            vistos_t = 1;
        } else if (strcmp(argv[i], "-a") == 0) {
            if (vistos_a || i + 1 >= argc ||
                convertir_entero(argv[++i], &config->cantidad_agentes) != 0) {
                return -1;
            }
            vistos_a = 1;
        } else {
            return -1;
        }
    }

        if (!vistos_b || !vistos_p || !vistos_t || !vistos_a) {
        fprintf(stderr, "Error: debes indicar -b, -p, -t y -a\n");
        return -1;
    }

    if (config->pipe_nombre[0] == '\0' ||
        config->pipe_nombre[0] == '-') {
        fprintf(stderr, "Error: nombre del FIFO invalido\n");
        return -1;
    }

    if (config->segundos < 30 || config->segundos > 120) {
        fprintf(stderr, "Error: el tiempo debe estar entre 30 y 120\n");
        return -1;
    }

    if (config->cantidad_agentes < 1 ||
        config->cantidad_agentes > MAX_AGENTES) {
        fprintf(stderr, "Error: la cantidad de agentes debe estar entre 1 y 4\n");
        return -1;
    }

    return 0;
}


int main(int argc, char **argv) {
    Configuracion config;

    if (leer_configuracion(argc, argv, &config) != 0) {
        fprintf(stderr, "Error: configuración invalida\n");
        mostrar_uso(argv[0]);
        return 1;
    }

    // Aquí iría la lógica principal del servidor ISE
    // Inicialización de agentes, creación de hilos, manejo de mensajes, etc.

    return 0;
}