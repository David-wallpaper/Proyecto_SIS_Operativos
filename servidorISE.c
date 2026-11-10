/*##########################################################################
# Pontificia Universidad Javeriana
# Fecha: 07-11-2026
# Materia: Sistemas Operativos
# Proyecto: Indicador de Sentimiento Economico (ISE)
##########################################################################*/
 
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
 
#define MAX_AGENTES 4
#define NOMBRE_MAX 32
#define PIPE_NOMBRE_POR_DEFECTO "TORVALDS"
 
// Tamanos de los campos
#define MAX_PAIS 3
#define MAX_SECTOR 30
 
// Sectores de la encuesta y su peso en el ISE
#define NUM_SECTORES 5
static const char *NOMBRE_SECTOR[NUM_SECTORES] = {"Industria", "Servicios", "Consumidores", "Comercio", "Construccion"};
static const double PESO_SECTOR[NUM_SECTORES] = {0.40, 0.30, 0.20, 0.05, 0.05};

// Tipos de mensaje que envia el agente
typedef enum {
    MENSAJE_INICIO = 1,     // el agente se conecta e informa su pais y cuantas encuestas enviara
    MENSAJE_ENCUESTA = 2,   // una linea del archivo CSV
    MENSAJE_FIN = 3         // el agente termino e informa cuantas encuestas envio
} TipoMensaje;
 
// Una encuesta
typedef struct {
    int idEncuesta;
    int idPais;
    char pais[MAX_PAIS];
    char sector[MAX_SECTOR];
    int idSector;               // 0..4, en el orden de NOMBRE_SECTOR
    double sentimiento;         // 0..100
} Encuesta;
 
// Mensaje completo que viaja por el FIFO
typedef struct {
    TipoMensaje tipo;
    pid_t pid;                  // identifica a que agente pertenece
    char pais[MAX_PAIS];
    int totalEncuestas;         // INICIO: las que enviara  FIN: las que envio
    Encuesta encuesta;
} MensajeAgente;

// Buffer circular de un agente
typedef struct {
    MensajeAgente *elementos;   // arreglo de tamB posiciones
    size_t capacidad;
    size_t entrada;             // donde escribe el hilo recolector
    size_t salida;              // donde lee el hilo del agente
    sem_t libres;               // espacios vacios (inicia en tamB)
    sem_t ocupados;             // espacios con mensajes (inicia en 0)
    pthread_mutex_t mutex;      // protege entrada y salida
} BufferAgente;
 
// Informacion y acumulados de un agente conectado
typedef struct {
    int activo;
    int fin_enviado;            // el recolector ya puso un FIN en su buffer
    int recibio_fin;            // el agente mando su propio FIN
    int total_esperado;         // encuestas anunciadas en el INICIO
    int total_reportado;        // encuestas que dice el FIN que envio
    pid_t pid;
    char pais[NOMBRE_MAX];
    unsigned long cantidad;
    unsigned long cantidad_sector[NUM_SECTORES];
    double suma_sector[NUM_SECTORES];
    BufferAgente buffer;
    pthread_t hilo;
} Agente;
 
// Parametros recibidos por linea de comandos
typedef struct {
    char pipe_nombre[256];
    size_t tam_buffer;
    int segundos;
    int cantidad_agentes;
} Configuracion;
 
static Agente agentes[MAX_AGENTES];
static int fd_pipe = -1;
static int tiempo_terminado = 0;           // lo pone en 1 el hilo reloj cuando se acaba el tiempo
static pthread_mutex_t mutex_tiempo = PTHREAD_MUTEX_INITIALIZER;   // protege tiempo_terminado: la escribe un hilo y la lee otro   // lo pone en 1 el hilo reloj; atomica porque la escribe un hilo y la lee otro
static pthread_mutex_t mutex_salida = PTHREAD_MUTEX_INITIALIZER;   // para que los reportes no se impriman mezclados

// Convierte texto a int con strtol. Devuelve 0 si es valido, -1 si no
static int convertir_entero(const char *texto, int *resultado) {
    char *fin;
 
    errno = 0;
    long numero = strtol(texto, &fin, 10);
 
    if (fin == texto || *fin != '\0' || errno == ERANGE || numero < INT_MIN || numero > INT_MAX) {
        return -1;
    }
 
    *resultado = (int)numero;
    return 0;
}
 
// Lee las banderas -b -p -t -a (en cualquier orden) y valida sus valores
static int leer_configuracion(int argc, char **argv, Configuracion *config) {
    int vistos_b = 0;
    int vistos_p = 0;
    int vistos_t = 0;
    int vistos_a = 0;
    int valor;
 
    memset(config, 0, sizeof(*config));
    strncpy(config->pipe_nombre, PIPE_NOMBRE_POR_DEFECTO, sizeof(config->pipe_nombre) - 1);
 
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            if (vistos_b || i + 1 >= argc || convertir_entero(argv[++i], &valor) != 0) {
                return -1;
            }
            if (valor <= 0) {
                return -1;
            }
            config->tam_buffer = (size_t)valor;
            vistos_b = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            if (vistos_p || i + 1 >= argc || strlen(argv[i + 1]) >= sizeof(config->pipe_nombre)) {
                return -1;
            }
            strncpy(config->pipe_nombre, argv[++i], sizeof(config->pipe_nombre) - 1);
            vistos_p = 1;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (vistos_t || i + 1 >= argc || convertir_entero(argv[++i], &config->segundos) != 0) {
                return -1;
            }
            vistos_t = 1;
        } else if (strcmp(argv[i], "-a") == 0) {
            if (vistos_a || i + 1 >= argc || convertir_entero(argv[++i], &config->cantidad_agentes) != 0) {
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
 
    if (config->pipe_nombre[0] == '\0' || config->pipe_nombre[0] == '-') {
        fprintf(stderr, "Error: nombre del FIFO invalido\n");
        return -1;
    }
 
    if (config->segundos < 30 || config->segundos > 120) {
        fprintf(stderr, "Error: el tiempo debe estar entre 30 y 120\n");
        return -1;
    }
 
    if (config->cantidad_agentes < 1 || config->cantidad_agentes > MAX_AGENTES) {
        fprintf(stderr, "Error: la cantidad de agentes debe estar entre 1 y 4\n");
        return -1;
    }
 
    return 0;
}

// Reserva el buffer e inicializa: libres = capacidad, ocupados = 0
static int buffer_inicializar(BufferAgente *buffer, size_t capacidad) {
    buffer->elementos = calloc(capacidad, sizeof(*buffer->elementos));
    if (buffer->elementos == NULL) {
        return -1;
    }
    buffer->capacidad = capacidad;
    buffer->entrada = 0;
    buffer->salida = 0;
 
    // pshared = 0: los semaforos se comparten entre hilos, no entre procesos
    if (sem_init(&buffer->libres, 0, capacidad) != 0) {
        free(buffer->elementos);
        buffer->elementos = NULL;
        return -1;
    }
    if (sem_init(&buffer->ocupados, 0, 0) != 0) {
        sem_destroy(&buffer->libres);
        free(buffer->elementos);
        buffer->elementos = NULL;
        return -1;
    }
    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        sem_destroy(&buffer->libres);
        sem_destroy(&buffer->ocupados);
        free(buffer->elementos);
        buffer->elementos = NULL;
        return -1;
    }
    return 0;
}
 
// Libera la memoria, los semaforos y el mutex de un buffer
static void buffer_destruir(BufferAgente *buffer) {
    if (buffer->elementos == NULL) {
        return;
    }
    sem_destroy(&buffer->libres);
    sem_destroy(&buffer->ocupados);
    pthread_mutex_destroy(&buffer->mutex);
    free(buffer->elementos);
    buffer->elementos = NULL;
}

// espera un espacio libre, guarda el mensaje y avisa que hay un mensaje nuevo
static void buffer_insertar(BufferAgente *buffer, const MensajeAgente *mensaje) {
    sem_wait(&buffer->libres);                       // buffer lleno -> espera
 
    pthread_mutex_lock(&buffer->mutex);              // seccion critica
    buffer->elementos[buffer->entrada] = *mensaje;
    buffer->entrada = (buffer->entrada + 1) % buffer->capacidad;
    pthread_mutex_unlock(&buffer->mutex);
    sem_post(&buffer->ocupados);                     // hay un mensaje nuevo
}
 
// Lo usa el hilo del agente: espera un mensaje, lo saca y avisa que hay un
// espacio libre
static void buffer_extraer(BufferAgente *buffer, MensajeAgente *mensaje) {
    sem_wait(&buffer->ocupados);                     // buffer vacio -> espera
 
    pthread_mutex_lock(&buffer->mutex);              // seccion critica
    *mensaje = buffer->elementos[buffer->salida];
    buffer->salida = (buffer->salida + 1) % buffer->capacidad;
    pthread_mutex_unlock(&buffer->mutex);
    sem_post(&buffer->libres);                       // se libero un espacio
}

// ISE = media ponderada de los promedios de sentimiento de cada sector.
// Si un sector no tiene encuestas, su peso se reparte entre los demas.
// Devuelve -1 si no hay ninguna encuesta.
static double calcular_ise(const unsigned long cantidad[], const double suma[]) {
    double ise = 0.0;
    double peso_usado = 0.0;
 
    for (int s = 0; s < NUM_SECTORES; ++s) {
        if (cantidad[s] > 0) {
            ise += PESO_SECTOR[s] * (suma[s] / (double)cantidad[s]);
            peso_usado += PESO_SECTOR[s];
        }
    }
    return peso_usado > 0.0 ? ise / peso_usado : -1.0;
}
 
// Recomendacion segun el enunciado: >50 vender, <50 comprar, =50 neutro.
// Se compara el ISE redondeado a 2 decimales (el mismo que se imprime),
// para que un 50.00% mostrado en pantalla diga NEUTRO y no VENDER.
static const char *recomendacion(double ise) {
    long centesimas = (long)(ise * 100.0 + 0.5);    // 50.0012 -> 5000
    if (centesimas > 5000) {
        return "VENDER";
    }
    if (centesimas < 5000) {
        return "COMPRAR";
    }
    return "NEUTRO";
}
 
// Estado del envio de un agente, segun su mensaje FIN
static const char *estado_envio(const Agente *agente) {
    if (!agente->recibio_fin) {
        return "SIN FIN";               // se desconecto o se acabo el tiempo
    }
    if ((unsigned long)agente->total_reportado == agente->cantidad) {
        return "COMPLETO";              // llego todo lo que dijo que envio
    }
    return "INCOMPLETO";                // se perdieron encuestas en el camino
}
 
// Imprime cantidad y promedio de cada sector
static void imprimir_sectores(const unsigned long cantidad[], const double suma[]) {
    for (int s = 0; s < NUM_SECTORES; ++s) {
        double promedio = cantidad[s] == 0 ? 0.0 : suma[s] / (double)cantidad[s];
        printf("  %-13s (peso %2.0f%%): %6lu encuestas | promedio %6.2f\n", NOMBRE_SECTOR[s], PESO_SECTOR[s] * 100.0, cantidad[s], promedio);
    }
}
 
// Reporte parcial: se emite cuando un agente termina de enviar
static void imprimir_parcial(const Agente *agente) {
    double ise = calcular_ise(agente->cantidad_sector, agente->suma_sector);
 
    pthread_mutex_lock(&mutex_salida);
    printf("\n--- Reporte parcial: %s (PID %ld) ---\n", agente->pais, (long)agente->pid);
    printf("Encuestas recibidas: %lu\n", agente->cantidad);
    imprimir_sectores(agente->cantidad_sector, agente->suma_sector);
    if (ise >= 0.0) {
        printf("ISE %s: %.2f%% -> Recomendacion: %s\n", agente->pais, ise, recomendacion(ise));
    } else {
        printf("ISE %s: sin encuestas\n", agente->pais);
    }
 
    // Verificacion con el mensaje FIN
    if (agente->recibio_fin) {
        printf("Verificacion FIN: el agente envio %d, se recibieron %lu -> %s\n", agente->total_reportado, agente->cantidad, estado_envio(agente));
    } else {
        printf("Verificacion FIN: el agente NO envio FIN; se recibieron %lu de %d anunciadas -> %s\n", agente->cantidad, agente->total_esperado, estado_envio(agente));
    }
    fflush(stdout);
    pthread_mutex_unlock(&mutex_salida);
}
 
// Resumen general: ISE por pais y ISE de todos los paises juntos
static void imprimir_resumen(void) {
    unsigned long total = 0;
    unsigned long cantidad[NUM_SECTORES] = {0};
    double suma[NUM_SECTORES] = {0.0};
 
    pthread_mutex_lock(&mutex_salida);
    printf("\n================ RESUMEN GENERAL ISE ================\n");
    for (int i = 0; i < MAX_AGENTES; ++i) {
        if (!agentes[i].activo) {
            continue;
        }
        double ise = calcular_ise(agentes[i].cantidad_sector, agentes[i].suma_sector);
        if (ise >= 0.0) {
            printf("%-8s | encuestas: %6lu | ISE: %6.2f%% | %-7s | envio: %s\n", agentes[i].pais, agentes[i].cantidad, ise, recomendacion(ise), estado_envio(&agentes[i]));
        } else {
            printf("%-8s | encuestas: %6lu | sin encuestas | envio: %s\n", agentes[i].pais, agentes[i].cantidad, estado_envio(&agentes[i]));
        }
        // acumula por sector para el ISE general
        for (int s = 0; s < NUM_SECTORES; ++s) {
            cantidad[s] += agentes[i].cantidad_sector[s];
            suma[s] += agentes[i].suma_sector[s];
        }
        total += agentes[i].cantidad;
    }
 
    printf("\nTotal de encuestas: %lu\n", total);
    imprimir_sectores(cantidad, suma);
    double ise_general = calcular_ise(cantidad, suma);
    if (ise_general >= 0.0) {
        printf("\nISE GENERAL: %.2f%% -> RECOMENDACION: %s\n", ise_general, recomendacion(ise_general));
    } else {
        printf("\nNo se recibieron encuestas.\n");
    }
    printf("=====================================================\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_salida);
}

// Hilo de cada agente: saca mensajes de su buffer, acumula por sector y al
// recibir FIN emite el reporte parcial y termina
static void *hilo_agente(void *argumento) {
    Agente *agente = argumento;
    MensajeAgente mensaje;
 
    while (1) {
        buffer_extraer(&agente->buffer, &mensaje);
        if (mensaje.tipo == MENSAJE_FIN) {
            // El FIN real trae el PID del agente; el que pone el servidor al
            // acabarse el tiempo (esperar_agentes) trae PID 0
            if (mensaje.pid == agente->pid) {
                agente->recibio_fin = 1;
                agente->total_reportado = mensaje.totalEncuestas;
            }
            imprimir_parcial(agente);
            return NULL;
        }
 
        if (mensaje.tipo == MENSAJE_ENCUESTA) {
            int s = mensaje.encuesta.idSector;
            if (s < 0 || s >= NUM_SECTORES) {
                continue;                       // sector invalido: se descarta
            }
            agente->cantidad++;
            agente->cantidad_sector[s]++;
            agente->suma_sector[s] += mensaje.encuesta.sentimiento;
        }
    }
}
 
// Busca un agente ya registrado por su PID
static Agente *buscar_agente(pid_t pid) {
    for (int i = 0; i < MAX_AGENTES; ++i) {
        if (agentes[i].activo && agentes[i].pid == pid) {
            return &agentes[i];
        }
    }
    return NULL;
}
 
// Registra un agente nuevo: le crea su buffer y su hilo.
// Devuelve NULL si ya se alcanzo el maximo de agentes (-a).
// Solo la llama el hilo recolector, por eso no necesita mutex.
static Agente *registrar_agente(const MensajeAgente *mensaje, const Configuracion *config) {
    Agente *agente = NULL;
 
    for (int i = 0; i < config->cantidad_agentes; ++i) {
        if (!agentes[i].activo) {
            agente = &agentes[i];
            memset(agente, 0, sizeof(*agente));
            agente->activo = 1;
            agente->pid = mensaje->pid;
            snprintf(agente->pais, sizeof(agente->pais), "%s", mensaje->pais);
            agente->total_esperado = mensaje->totalEncuestas;   // lo anuncia el INICIO
            if (buffer_inicializar(&agente->buffer, config->tam_buffer) != 0 || pthread_create(&agente->hilo, NULL, hilo_agente, agente) != 0) {
                buffer_destruir(&agente->buffer);
                memset(agente, 0, sizeof(*agente));
                agente = NULL;
            }
            break;
        }
    }
    return agente;
}
 
// Hilo de tiempo: duerme <time> segundos y avisa que se acabo el tiempo
static void *hilo_reloj(void *argumento) {
    const Configuracion *config = argumento;

    sleep(config->segundos);
    pthread_mutex_lock(&mutex_tiempo);       // nuevo
    tiempo_terminado = 1;
    pthread_mutex_unlock(&mutex_tiempo);     // nuevo
    return NULL;
}
// Consulta tiempo_terminado con el mutex tomado (la usa el hilo recolector)
static int se_acabo_el_tiempo(void) {
    pthread_mutex_lock(&mutex_tiempo);
    int valor = tiempo_terminado;
    pthread_mutex_unlock(&mutex_tiempo);
    return valor;
}
 
// Hilo recolector: lee los mensajes del FIFO y deja cada uno en el buffer
// del agente que lo envio
static void *hilo_recolector(void *argumento) {
    const Configuracion *config = argumento;
 
    while (!se_acabo_el_tiempo()) {
        struct pollfd descriptor = {fd_pipe, POLLIN, 0};
        MensajeAgente mensaje;
 
        // espera datos maximo 500 ms para revisar si ya se acabo el tiempo
        int resultado = poll(&descriptor, 1, 500);
 
        if (resultado == -1) {
            perror("Error esperando mensajes");
            break;
        }
        if (resultado == 0 || !(descriptor.revents & POLLIN)) {
            continue;
        }
 
        // cada mensaje mide sizeof(MensajeAgente) (< PIPE_BUF), por lo que
        // las escrituras de los agentes son atomicas y no se mezclan
        ssize_t leidos = read(fd_pipe, &mensaje, sizeof(mensaje));
        if (leidos == -1) {
            if (errno == EAGAIN) {                  // no habia datos todavia
                continue;
            }
            perror("Error leyendo el FIFO");
            break;
        }
        if ((size_t)leidos != sizeof(mensaje)) {
            fprintf(stderr, "Mensaje incompleto ignorado (%zd bytes)\n", leidos);
            continue;
        }
 
        Agente *agente = buscar_agente(mensaje.pid);
 
        // Evento "conexion de un agente"
        if (mensaje.tipo == MENSAJE_INICIO && agente == NULL) {
            agente = registrar_agente(&mensaje, config);
            pthread_mutex_lock(&mutex_salida);
            if (agente != NULL) {
                printf("Agente conectado: %s (PID %ld) - %d encuestas por enviar\n", agente->pais, (long)agente->pid, mensaje.totalEncuestas);
            } else {
                fprintf(stderr, "Se rechazo el agente %s (PID %ld): limite de %d agentes alcanzado\n", mensaje.pais, (long)mensaje.pid, config->cantidad_agentes);
            }
            fflush(stdout);
            pthread_mutex_unlock(&mutex_salida);
            continue;
        }
 
        // Mensajes de un agente rechazado o desconocido: se ignoran sin
        // imprimir, para no llenar la pantalla (ya se aviso al rechazarlo)
        if (agente == NULL || agente->fin_enviado) {
            continue;
        }
 
        if (mensaje.tipo == MENSAJE_ENCUESTA || mensaje.tipo == MENSAJE_FIN) {
            buffer_insertar(&agente->buffer, &mensaje);
            if (mensaje.tipo == MENSAJE_FIN) {
                agente->fin_enviado = 1;
            }
        }
    }
    return NULL;
}
 
// Al acabar el tiempo: a los agentes que no alcanzaron a enviar FIN se les
// pone uno en el buffer para que su hilo termine; luego espera a todos
static void finalizar_servidor(const Configuracion *config) {
    for (int i = 0; i < MAX_AGENTES; ++i) {
        if (agentes[i].activo) {
            if (!agentes[i].fin_enviado) {            // si no alcanzo a mandar FIN, se le pone uno
                MensajeAgente fin;
                memset(&fin, 0, sizeof(fin));
                fin.tipo = MENSAJE_FIN;
                buffer_insertar(&agentes[i].buffer, &fin);
                agentes[i].fin_enviado = 1;
            }
            pthread_join(agentes[i].hilo, NULL);       // espera a que su hilo termine
            buffer_destruir(&agentes[i].buffer);       // el buffer ya no lo usa nadie
        }
    }

    if (fd_pipe != -1) {
        close(fd_pipe);
        fd_pipe = -1;
    }
    unlink(config->pipe_nombre);
}
 
int main(int argc, char **argv) {
    Configuracion config;
    pthread_t reloj;
    pthread_t recolector;
 
    // 1. Argumentos
    if (leer_configuracion(argc, argv, &config) != 0) {
        fprintf(stderr, "Error: configuracion invalida\n");
        //Muestra la forma correcta de invocar el servidor
        fprintf(stderr, "Uso: %s -b <tamB> -p <PipeN> -t <time> -a <hilos>\n", argv[0]);
        fprintf(stderr, "  -b  capacidad del buffer de cada agente (registros)\n");
        fprintf(stderr, "  -p  nombre del FIFO nominal (recomendado: TORVALDS)\n");
        fprintf(stderr, "  -t  tiempo de ejecucion, entre 30 y 120 segundos\n");
        fprintf(stderr, "  -a  cantidad maxima de agentes, entre 1 y 4\n");
        return EXIT_FAILURE;
    }
 
    // 2. Crear el FIFO (si ya existe, verificar que si sea un FIFO)
    if (mkfifo(config.pipe_nombre, 0660) == -1) {
        if (errno != EEXIST) {
            perror("No se pudo crear el FIFO");
            return EXIT_FAILURE;
        }
        struct stat estado;
        if (stat(config.pipe_nombre, &estado) != 0 || !S_ISFIFO(estado.st_mode)) {
            fprintf(stderr, "La ruta existente no es un FIFO: %s\n", config.pipe_nombre);
            return EXIT_FAILURE;
        }
    }
 
    // O_RDWR: el servidor cuenta como escritor, asi open() no se bloquea
    // esperando agentes y read() no da EOF cuando un agente se desconecta
    fd_pipe = open(config.pipe_nombre, O_RDWR | O_NONBLOCK);
    if (fd_pipe == -1) {
        perror("No se pudo abrir el FIFO");
        unlink(config.pipe_nombre);
        return EXIT_FAILURE;
    }
 
    printf("Servidor Control Proceso Principal ISE\n");
    printf("FIFO: %s | Buffer: %zu | Tiempo: %d s | Agentes: %d\n", config.pipe_nombre, config.tam_buffer, config.segundos, config.cantidad_agentes);
    printf("Esperando agentes...\n");
    fflush(stdout);
 
    // 3. Hilo de tiempo y hilo de lectura (cada actividad en un hilo POSIX)
    if (pthread_create(&reloj, NULL, hilo_reloj, &config) != 0) {
        perror("No se pudo crear el hilo de tiempo");
        finalizar_servidor(&config);
        return EXIT_FAILURE;
    }
    if (pthread_create(&recolector, NULL, hilo_recolector, &config) != 0) {
        perror("No se pudo crear el hilo recolector");
        finalizar_servidor(&config);      // al salir de main termina el hilo reloj
        return EXIT_FAILURE;
    }
 
    // 4. Esperar a que se acabe el tiempo y a que el recolector se detenga
    pthread_join(reloj, NULL);
    pthread_join(recolector, NULL);
    printf("\nTiempo de ejecucion finalizado.\n");
 
    // 5. Cerrar los hilos de los agentes, resumen general y limpieza
    finalizar_servidor(&config);
    imprimir_resumen();
    return EXIT_SUCCESS;
}