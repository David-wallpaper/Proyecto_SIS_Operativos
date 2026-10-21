#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char *pais = NULL;
    char *porcentajeTexto = NULL;
    char * fin;
    double porcentaje;
    printf("Generador de encuestas\n");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0) {
            if (pais != NULL) {
                printf("Error: la bandera -p esta repetida\n");
                return 1;
            }

            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                printf("Error: falta el pais despues de -p\n");
                return 1;
            }

            pais = argv[i + 1];
            ++i;

        } else if (strcmp(argv[i], "-e") == 0) {
            if (porcentajeTexto != NULL) {
                printf("Error: la bandera -e esta repetida\n");
                return 1;
            }

            if (i + 1 >= argc) {
                printf("Error: falta el porcentaje despues de -e\n");
                return 1;
            }

            if (strcmp(argv[i + 1], "-p") == 0 ||
                strcmp(argv[i + 1], "-e") == 0) {
                printf("Error: falta el porcentaje despues de -e\n");
                return 1;
            }

            porcentajeTexto = argv[i + 1];
            ++i;

        } else {
            printf("Error: argumento desconocido: %s\n", argv[i]);
            return 1;
        }
    }

    if (pais == NULL) {
        printf("Error: debes indicar el pais con -p\n");
        return 1;
    }

    if (strcmp(pais, "ES") != 0 &&
        strcmp(pais, "FR") != 0 &&
        strcmp(pais, "IT") != 0) {
        printf("Error: el pais debe ser ES, FR o IT\n");
        return 1;
    }

    if (porcentajeTexto == NULL) {
        printf("Error: debes indicar el porcentaje con -e\n");
        return 1;
    }

   errno = 0;
   porcentaje = strtod(porcentajeTexto, &fin);

if (fin == porcentajeTexto || *fin != '\0' ||
    errno == ERANGE || !isfinite(porcentaje)) {
    printf("Error: el porcentaje debe ser un numero valido\n");
    return 1;
}

if (porcentaje < 0 || porcentaje > 100) {
    printf("Error: el porcentaje debe estar entre 0 y 100\n");
    return 1;
}
    printf("Pais seleccionado: %s\n", pais);
    printf("Porcentaje seleccionado: %.2f%%\n", porcentaje);

const char *sectores[5] = {
    "Industria", "Servicios", "Consumidores", "Comercio", "Construccion"
};

int maximos[3][5] = {
    { 1760, 1430, 2020,1080,290}, // ES
    { 3850, 4240, 1670, 2860, 24020 }, // FR
    { 5440, 3100, 2000, 1560, 1000 }  // IT

};

int indicePais;

if(strcmp(pais, "ES") == 0) {
    indicePais = 0;
} else if(strcmp(pais, "FR") == 0) {
    indicePais = 1;
} else {
    indicePais = 2;
 }

int cantidades[5];
int total=0;


for(int i=0; i<5; i++) {
    cantidades[i] = (int)(maximos[indicePais][i] * porcentaje / 100.0 + 0.5);

    total+= cantidades[i];

    printf("%s: %d/%d encuestras\n",
    sectores[i], cantidades[i], maximos[indicePais][i]);        
}

printf("Total de encuestas: %d\n", total);


char nombreArchivo[100];

snprintf(nombreArchivo, sizeof(nombreArchivo),
 "encuestas_%s_%.2f.csv", pais);

FILE *archivo = fopen(nombreArchivo, "w");

if (archivo == NULL) {
    printf("Error: no se pudo crear el archivo %s\n", nombreArchivo);
    return 1;
}

if(fprintf(archivo, "ID_Encuesta,ID_Pais,Pais,Sector,ID_Sector,Sentimiento\n") < 0) {
    perror("Error al escribir la cabecera");
    fclose(archivo);
    return 1;
}

if(fclose(archivo) != 0) {
    perror("Error al cerrar el archivo");
    return 1;
}

printf("Archivo %s creado exitosamente\n", nombreArchivo);

    return 0;
}

