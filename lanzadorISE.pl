#!/usr/bin/perl
#**************************************************************
#               Pontificia Universidad Javeriana
#     Fecha:
#     Materia: Sistemas Operativos
#     Proyecto: Indicador de Sentimiento Economico (ISE)
#     Fichero: lanzadorISE.pl - automatizacion de las pruebas
#              de rendimiento por lotes
#**************************************************************
#
# Ejecuta el servidor ISE con cada combinacion de tamano de buffer
# (-b) y cantidad de agentes, y mide cuanto tardan los agentes en
# enviar la carga de encuestas. Cada medicion se agrega al archivo
#   Soluciones/ISE-Buffer-<tamB>-Agentes-<k>.dat
# Al final crea Soluciones/resumen.csv (promedio, desviacion,
# minimo y maximo) y Soluciones/integridad.csv.
#
# Uso (en la carpeta de los ejecutables):
#   chmod +x lanzadorISE.pl
#   ./lanzadorISE.pl
#   nohup ./lanzadorISE.pl > salida_lanzador.txt 2>&1 &   (en segundo plano)

use strict;
use warnings;
use Time::HiRes qw(time sleep);     # time() con decimales (microsegundos)

my $Path = `pwd`;
chomp($Path);

my @Size_Buffer  = (1, 10, 100);   # valores de -b
my @Num_Agentes  = (1, 2, 4);            # agentes enviando a la vez
my $Repeticiones = 30;                       # mediciones por combinacion
my $Tiempo       = 30;                      # -t del servidor (segundos)
my $Resultados   = "Soluciones";            # carpeta de resultados
my $Pipe         = "PIPE_LANZADOR";         # nombre del FIFO de prueba
my $Carga        = "carga_FR.csv";          # archivo que envia cada agente

my $Servidor  = "$Path/servidorISE";
my $Agente    = "$Path/agentePais";
my $Generador = "$Path/generadorCSV";

# Verifica que existan los ejecutables
foreach my $prog ($Servidor, $Agente, $Generador) {
    die "No se encuentra $prog (compilelo primero)\n" unless -x $prog;
}

# Carga de prueba: Francia al 100% = 36640 encuestas por agente
system("$Generador -p FR -e 100 > /dev/null") == 0 or die "Fallo el generador\n";
rename("encuestas_FR.csv", $Carga) or die "No se pudo renombrar la carga\n";
my $Encuestas = `wc -l < $Carga` - 1;
printf("Carga de prueba: %d encuestas por agente\n", $Encuestas);

mkdir("$Path/$Resultados") unless -d "$Path/$Resultados";
open(my $integridad, '>', "$Path/$Resultados/integridad.csv") or die;
print $integridad "tamB,agentes,esperadas,recibidas,agentes_completos\n";

# Lanza un programa en segundo plano y devuelve su PID.
# Su salida estandar y de error van al archivo indicado.
sub lanzar {
    my ($salida, @comando) = @_;
    my $pid = fork();
    die "fork fallo\n" unless defined $pid;
    if ($pid == 0) {                                # proceso hijo
        open(STDOUT, '>', $salida);
        open(STDERR, '>&', \*STDOUT);
        exec(@comando) or exit(127);
    }
    return $pid;                                    # proceso padre
}

printf("Inicio: %s", scalar(localtime) . "\n");

foreach my $tamB (@Size_Buffer) {
    foreach my $agentes (@Num_Agentes) {
        my $file = "$Path/$Resultados/ISE-Buffer-$tamB-Agentes-$agentes.dat";
        printf("$file \n");
        open(my $dat, '>', $file) or die "No se pudo crear $file\n";

        # El servidor acepta maximo 4 agentes en toda su ejecucion, asi que
        # cada ejecucion alcanza para int(4 / agentes) mediciones
        my $por_sesion = int(4 / $agentes);
        my $hechas = 0;

        while ($hechas < $Repeticiones) {
            my $lotes = $Repeticiones - $hechas;
            $lotes = $por_sesion if $lotes > $por_sesion;

            # 1. Arranca el servidor
            unlink($Pipe);
            my $spid = lanzar("log_servidor.txt", $Servidor, "-b", $tamB, "-p", $Pipe, "-t", $Tiempo, "-a", 4);
            sleep(1);

            # 2. Cada lote: lanza los agentes a la vez y mide hasta que terminan
            for (my $i = 0; $i < $lotes; $i++) {
                my $inicio = time();
                my @pids;
                for (my $a = 0; $a < $agentes; $a++) {
                    push(@pids, lanzar("/dev/null", $Agente, "-n", $Carga, "-p", $Pipe));
                }
                waitpid($_, 0) foreach @pids;
                my $segundos = time() - $inicio;
                printf $dat ("%.4f\n", $segundos);
                printf("   tamB=%-5d agentes=%d repeticion %d: %.4f s\n", $tamB, $agentes, $hechas + $i + 1, $segundos);
            }

            # 3. Espera a que el servidor termine y revisa la integridad
            waitpid($spid, 0);
            open(my $log, '<', "log_servidor.txt") or die;
            my ($recibidas, $completos) = (0, 0);
            while (my $linea = <$log>) {
                $recibidas = $1 if $linea =~ /Total de encuestas: (\d+)/;
                $completos++ if $linea =~ /Verificacion FIN.*COMPLETO$/;
            }
            close($log);
            printf $integridad ("%d,%d,%d,%d,%d\n", $tamB, $agentes, $lotes * $agentes * $Encuestas, $recibidas, $completos);

            $hechas += $lotes;
        }
        close($dat);
    }
}
close($integridad);

# Resumen: promedio, desviacion estandar, minimo y maximo de cada .dat
open(my $resumen, '>', "$Path/$Resultados/resumen.csv") or die;
print $resumen "tamB,agentes,n,promedio_s,desviacion_s,minimo_s,maximo_s,encuestas_por_s\n";
foreach my $tamB (@Size_Buffer) {
    foreach my $agentes (@Num_Agentes) {
        open(my $dat, '<', "$Path/$Resultados/ISE-Buffer-$tamB-Agentes-$agentes.dat") or next;
        my @t = map { chomp; $_ } <$dat>;
        close($dat);
        my $n = scalar(@t);
        next if $n == 0;
        my $suma = 0;
        $suma += $_ foreach @t;
        my $prom = $suma / $n;
        my $var = 0;
        $var += ($_ - $prom) ** 2 foreach @t;
        my $desv = $n > 1 ? sqrt($var / ($n - 1)) : 0;
        my @orden = sort { $a <=> $b } @t;
        printf $resumen ("%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.0f\n", $tamB, $agentes, $n, $prom, $desv, $orden[0], $orden[-1], $agentes * $Encuestas / $prom);
    }
}
close($resumen);

unlink($Pipe, "log_servidor.txt", $Carga);
printf("Fin: %s", scalar(localtime) . "\n");
print "\n===== $Resultados/resumen.csv =====\n";
system("cat $Path/$Resultados/resumen.csv");
print "\n===== $Resultados/integridad.csv =====\n";
system("cat $Path/$Resultados/integridad.csv");
