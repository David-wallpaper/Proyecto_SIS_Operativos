# Proyecto ISE — Indicador de Sentimiento Económico

El objetivo del proyecto es implementar un simulador del **Indicador de Sentimiento Económico (ISE)** utilizando programación en C sobre Linux y aplicando conceptos de sistemas operativos como procesos, hilos POSIX, pipes nominales, sincronización y manejo de recursos.

---

## Descripción general

El sistema simula el procesamiento de encuestas de sentimiento económico para diferentes países.

Cada encuesta pertenece a uno de los siguientes sectores:

| Sector | Peso |
|---|---:|
| Industria | 40 % |
| Servicios | 30 % |
| Consumidores | 20 % |
| Comercio | 5 % |
| Construcción | 5 % |

Cada registro contiene un valor de sentimiento entre 0 y 100.

El sistema se divide en tres componentes principales:

```text
generadorCSV
     |
     v
archivo CSV
     |
     v
 agentePais
     |
     v
Pipe Nominal
     |
     v
 servidorISE
     |
     v
Procesamiento y cálculo ISE
```

---

## Estructura del repositorio

```text
Proyecto_SIS_Operativos/
│
├── .gitignore
├── LICENSE
├── Makefile
├── README.md
│
├── generadorCSV.c
├── agentePais.c
└── servidorISE.c
```

Durante la ejecución también se generan archivos CSV como:

```text
encuestas_ES.csv
encuestas_FR.csv
encuestas_IT.csv
```

---

# Componentes

## 1. Generador de encuestas

Archivo:

```text
generadorCSV.c
```

Este programa genera los archivos CSV que posteriormente serán utilizados por los agentes.

### Ejecución

```bash
./generadorCSV -p <pais> -e <porcentaje>
```

Donde:

- `-p`: país.
- `-e`: porcentaje de encuestas que se desea generar.

Los países permitidos son:

```text
ES
FR
IT
```

El porcentaje debe encontrarse entre `0` y `100`.

### Ejemplo

```bash
./generadorCSV -p FR -e 75
```

Esto genera un archivo:

```text
encuestas_FR.csv
```

con una estructura similar a:

```csv
ID_Encuesta,ID_Pais,Pais,Sector,ID_Sector,Sentimiento
1,1,FR,Industria,0,45.23
2,1,FR,Industria,0,67.89
3,1,FR,Servicios,1,72.31
```

---

## 2. Agente de lectura

Archivo:

```text
agentePais.c
```

El agente funciona como cliente del sistema.

Sus principales responsabilidades son:

- Recibir el nombre del archivo CSV.
- Abrir y leer el archivo.
- Contar la cantidad total de encuestas.
- Convertir cada línea del CSV en una estructura de encuesta.
- Conectarse al servidor mediante un pipe nominal.
- Enviar un mensaje de inicio.
- Enviar cada encuesta.
- Mostrar el progreso del envío cada 100 registros.
- Enviar un mensaje de finalización.
- Cerrar correctamente los recursos utilizados.

### Ejecución

```bash
./agentePais -n <archivo.csv> -p <pipe>
```

Ejemplo:

```bash
./agentePais -n encuestas_FR.csv -p TORVALDS
```

También se permite cambiar el orden de los argumentos:

```bash
./agentePais -p TORVALDS -n encuestas_FR.csv
```

Durante la ejecución se muestra un progreso similar a:

```text
100/27481
200/27481
300/27481
...
27400/27481
27481/27481
```

---

## 3. Servidor ISE

Archivo:

```text
servidorISE.c
```

El servidor corresponde al proceso principal del sistema.

Se encarga de:

- Crear y administrar el pipe nominal.
- Recibir información enviada por los agentes.
- Utilizar hilos POSIX para el procesamiento concurrente.
- Administrar buffers de encuestas.
- Sincronizar los recursos compartidos.
- Procesar las encuestas recibidas.
- Controlar el tiempo de ejecución.
- Generar reportes parciales.
- Generar el reporte final del Indicador de Sentimiento Económico.
- Liberar correctamente los recursos del sistema.

### Ejecución

```bash
./servidorISE -b <tamB> -p <PipeN> -t <time> -a <hilos>
```

Donde:

- `-b`: tamaño del buffer.
- `-p`: nombre del pipe nominal.
- `-t`: tiempo de ejecución del servidor.
- `-a`: cantidad máxima de agentes habilitados.

Ejemplo:

```bash
./servidorISE -b 10 -p TORVALDS -t 60 -a 3
```

---

# Compilación

El proyecto incluye un `Makefile` que compila automáticamente los tres programas.

Para compilar:

```bash
make
```

Esto genera los siguientes ejecutables:

```text
generadorCSV
agentePais
servidorISE
```

La compilación utiliza:

```text
gcc
-std=c11
-Wall
-Wextra
```

y para el servidor también:

```text
-pthread
```

---

## Limpiar ejecutables

Para eliminar los binarios generados:

```bash
make clean
```

Esto elimina:

```text
generadorCSV
agentePais
servidorISE
```

sin borrar los archivos fuente.

---

# Ejecución del proyecto

El orden recomendado de ejecución es el siguiente.

## 1. Compilar

```bash
make
```

---

## 2. Generar las encuestas

Ejemplo para España:

```bash
./generadorCSV -p ES -e 100
```

Para Francia:

```bash
./generadorCSV -p FR -e 100
```

Para Italia:

```bash
./generadorCSV -p IT -e 100
```

---

## 3. Iniciar el servidor

El servidor debe ejecutarse antes que los agentes.

Ejemplo:

```bash
./servidorISE -b 10 -p TORVALDS -t 60 -a 3
```

---

## 4. Ejecutar los agentes

Cada agente debe ejecutarse desde una terminal diferente.

### Terminal 2

```bash
./agentePais -n encuestas_ES.csv -p TORVALDS
```

### Terminal 3

```bash
./agentePais -n encuestas_FR.csv -p TORVALDS
```

### Terminal 4

```bash
./agentePais -n encuestas_IT.csv -p TORVALDS
```

---

# Comunicación entre procesos

La comunicación entre los agentes y el servidor se realiza mediante un **Pipe Nominal o FIFO**.

El flujo general es:

```text
Agente ES ───┐
             │
Agente FR ───┼──> Pipe Nominal ───> servidorISE
             │
Agente IT ───┘
```

Los agentes envían tres tipos principales de mensaje:

```text
MENSAJE_INICIO
MENSAJE_ENCUESTA
MENSAJE_FIN
```

`MENSAJE_INICIO` informa que un agente empieza a transmitir.

`MENSAJE_ENCUESTA` transporta la información de una encuesta.

`MENSAJE_FIN` informa que el agente terminó su transmisión.

---

# Concurrencia y sincronización

El servidor utiliza hilos POSIX para permitir que diferentes tareas puedan ejecutarse concurrentemente.

Por ejemplo:

```text
Recepción de encuestas
Procesamiento de buffers
Atención de agentes
Control del tiempo
Generación de resultados
```

Para evitar problemas al acceder a recursos compartidos se utilizan mecanismos de sincronización POSIX.

---

# Requisitos

El proyecto está diseñado para ejecutarse en Linux.

Se requiere:

- GCC
- Make
- POSIX
- pthread

Puede comprobarse la instalación de GCC con:

```bash
gcc --version
```

y de Make con:

```bash
make --version
```

---

# Clonar el repositorio

```bash
git clone https://github.com/David-wallpaper/Proyecto_SIS_Operativos.git
```

Entrar al proyecto:

```bash
cd Proyecto_SIS_Operativos
```

Compilar:

```bash
make
```

---

# Tecnologías utilizadas

- **Lenguaje:** C
- **Sistema operativo:** Linux
- **Estándar:** C11
- **Compilador:** GCC
- **Concurrencia:** pthread
- **IPC:** Pipes nominales (FIFO)
- **Sincronización:** POSIX
- **Compilación:** Make
- **Control de versiones:** Git y GitHub
