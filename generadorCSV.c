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

    return 0;
}