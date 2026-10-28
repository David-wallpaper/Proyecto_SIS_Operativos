#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// Tamaños máximos para los campos de la encuesta
#define MAX_LINEA 512
#define MAX_PAIS 3
#define MAX_SECTOR 30

 //tipo de dato definido que el agente envia
typedef enum {
    MENSAJE_INICIO = 1,
    MENSAJE_ENCUESTA = 2,
    MENSAJE_FIN = 3
} TipoMensaje;

 //estructura para almacenar la información de una encuesta
typedef struct {
    int idEncuesta;
    int idPais;
    char pais[MAX_PAIS];
    char sector[MAX_SECTOR];
    int idSector;
    double sentimiento;
} Encuesta;

 //estructura para enviar información al servidor
typedef struct {
    TipoMensaje tipo;

    char pais[MAX_PAIS];

    int totalEncuestas;

    Encuesta encuesta;

} MensajeAgente;

 //cuenta la cantidad de encuestas en el CSV
int contarEncuestas(FILE *archivo) {

    char linea[MAX_LINEA];

    int total = 0;

    rewind(archivo); //ubica el puntero al inicio del archivo

    //leer y verificar la cabecera del archivo
    if (fgets(linea, sizeof(linea), archivo) == NULL) {

        printf("Error: el archivo CSV esta vacio\n");
        return -1;
    }

    //cuenta una linea entera por cada encuesta en el archivo
    while (fgets(linea, sizeof(linea), archivo) != NULL) {

        total++;
    }

    // Verificar si ocurrió algún error durante la lectura
    if (ferror(archivo)) {

        perror("Error al leer el archivo");
        return -1;
    }

    rewind(archivo); //volver a ubicar el puntero al inicio del archivo

    return total;
}

 // Convierte una línea del CSV en una estructura Encuesta
int convertirLineaEncuesta(const char *linea, Encuesta *encuesta) {

    int camposLeidos;

    camposLeidos = sscanf(  //extrae los campos de la linea y los almacena en la estructura encuesta
        linea,
        "%d,%d,%2[^,],%29[^,],%d,%lf",
        &encuesta->idEncuesta,
        &encuesta->idPais,
        encuesta->pais,
        encuesta->sector,
        &encuesta->idSector,
        &encuesta->sentimiento
    );

    //valida que la se haya leido correctamenete
    if (camposLeidos != 6) {

        return 0; //erro en letura de la linea
    }

    return 1;
}

 //envia un mensaje completo hacia el servidor a traves del pipe
int enviarMensaje(int fdPipe, const MensajeAgente *mensaje) {

    ssize_t bytesEscritos;

    //write (dondeEescribir, queEscribir, cuantosBytesEnvio)
    do {

        bytesEscritos = write(
            fdPipe,
            mensaje,
            sizeof(MensajeAgente)
        );

    } while (bytesEscritos == -1 && errno == EINTR);

    //verificar si se envio todo el mensaje correctamente
    if (bytesEscritos != sizeof(MensajeAgente)) {

        printf("Error: no se pudo enviar el mensaje completo\n");
        return -1;
    }

    return 0;
}


int main(int argc, char *argv[]) {

    char *nombreArchivo = NULL;
    char *nombrePipe = NULL;

    FILE *archivo;

    int fdPipe;

    char linea[MAX_LINEA];

    int totalEncuestas;
    int encuestasEnviadas = 0;

    Encuesta encuesta;
    MensajeAgente mensaje;


    printf("========================================\n");
    printf("       AGENTE DE LECTURA DE ENCUESTAS\n");
    printf("========================================\n\n");


    for (int i = 1; i < argc; i++) {

         //bandera -n
        if (strcmp(argv[i], "-n") == 0) {

            if (nombreArchivo != NULL) { //verifica que la bandera no este repatida

                printf("Error: la bandera -n esta repetida\n");
                return 1;
            }

            if (i + 1 >= argc || strcmp(argv[i + 1], "-n") == 0 || strcmp(argv[i + 1], "-p") == 0) {

                printf("Error: falta el archivo despues de -n\n");
                return 1;
            }

            nombreArchivo = argv[i + 1];

            i++;
        }

         //bandera -p
        else if (strcmp(argv[i], "-p") == 0) {

            if (nombrePipe != NULL) {

                printf("Error: la bandera -p esta repetida\n");
                return 1;
            }

            if (i + 1 >= argc || strcmp(argv[i + 1], "-n") == 0 || strcmp(argv[i + 1], "-p") == 0) {

                printf("Error: falta el nombre del pipe despues de -p\n");
                return 1;
            }

            nombrePipe = argv[i + 1];

            i++;
        }

         //validacion de argumentos desconocidos
        else {

            printf(
                "Error: argumento desconocido: %s\n",
                argv[i]
            );

            return 1;
        }
    }

    if (nombreArchivo == NULL) {

        printf("Error: debes indicar el archivo usando -n\n");

        return 1;
    }

    if (nombrePipe == NULL) {

        printf("Error: debes indicar el pipe usando -p\n");

        return 1;
    }


    printf("Archivo: %s\n", nombreArchivo);
    printf("Pipe: %s\n\n", nombrePipe);

    archivo = fopen(nombreArchivo, "r"); //abre el archivo en modo lectira

    if (archivo == NULL) {

        perror("Error al abrir el archivo CSV");

        return 1;
    }

    totalEncuestas = contarEncuestas(archivo); //cuenta la cantidad de encuestas en el archivo

    if (totalEncuestas < 0) {

        fclose(archivo);
        return 1;
    }

    if (totalEncuestas <= 0) {

        printf("El archivo CSV no contiene encuestas.\n");
        fclose(archivo);
        return 1;
    }

    printf("Leidas %d encuestas.\n\n", totalEncuestas);

    //leer la cabecera del archivo CSV y verificar que no haya errores
    if (fgets(linea, sizeof(linea), archivo) == NULL) {

        printf("Error al leer la cabecera del archivo\n");

        fclose(archivo);

        return 1;
    }

    printf("Conectando al proceso principal...\n");

     //hace la conexion con el pipe solo en modo escritura
    fdPipe = open(nombrePipe, O_WRONLY);

    if (fdPipe == -1) {

        perror("Error al conectar con el pipe");

        fclose(archivo);

        return 1;
    }

    printf("Conectado.\n\n");

    memset(&mensaje, 0, sizeof(MensajeAgente));

    mensaje.tipo = MENSAJE_INICIO;

    mensaje.totalEncuestas = totalEncuestas;

    fgets(linea, sizeof(linea), archivo);
    convertirLineaEncuesta(linea, &encuesta);
    rewind(archivo); //volver a ubicar el puntero al inicio del archivo
    fgets(linea, sizeof(linea), archivo);//volver a leer la cabecera

    strcpy(mensaje.pais, encuesta.pais);

    if (enviarMensaje(fdPipe, &mensaje) == -1) {

        close(fdPipe);
        fclose(archivo);
        return 1;
    }

    printf("Pais: %s\n", mensaje.pais);

    printf("Enviando...\n\n");

    while (fgets(linea, sizeof(linea), archivo) != NULL) {

         //lee la linea y la pasa a encuesta
        if (!convertirLineaEncuesta(linea, &encuesta)) {

            printf("Error: encuesta con formato incorrecto en el registro %d\n", encuestasEnviadas + 1);

            close(fdPipe);
            fclose(archivo);

            return 1;
        }


         //prepara el mensaje
        memset(&mensaje, 0, sizeof(MensajeAgente));

        mensaje.tipo = MENSAJE_ENCUESTA;
        mensaje.encuesta = encuesta;


         //enviar el mensaje al servidor
        if (enviarMensaje(fdPipe, &mensaje) == -1) {

            close(fdPipe);
            fclose(archivo);

            return 1;
        }

        encuestasEnviadas++;

        //mostrar el progreso
        if (encuestasEnviadas % 100 == 0) {

            printf("%d/%d\n", encuestasEnviadas, totalEncuestas);
        }
    }

     //mostara el total de encuastas enviadas
    if (encuestasEnviadas % 100 != 0) {

        printf("%d/%d\n", encuestasEnviadas, totalEncuestas);
    }

     //ocurrio un erro en la lectura
    if (ferror(archivo)) {

        perror("Error durante la lectura del CSV");

        close(fdPipe);
        fclose(archivo);

        return 1;
    }


    printf("\nEnviadas %d/%d encuestas.\n", encuestasEnviadas, totalEncuestas);


    //mensaje de finalizacion
    memset(&mensaje, 0, sizeof(MensajeAgente));

    mensaje.tipo = MENSAJE_FIN;
    strcpy(mensaje.pais, encuesta.pais);
    mensaje.totalEncuestas = encuestasEnviadas;

    if (enviarMensaje(fdPipe, &mensaje) == -1) {

        close(fdPipe);
        fclose(archivo);

        return 1;
    }


    printf("Mensaje de finalizacion enviado.\n");

    //liberar recursos
    if (fclose(archivo) != 0) {

        perror("Error al cerrar el archivo");
        return 1;
    }


    if (close(fdPipe) == -1) {

        perror("Error al cerrar el pipe");

        return 1;
    }


    printf("\nEnvio finalizado satisfactoriamente.\n");


    return 0;
}