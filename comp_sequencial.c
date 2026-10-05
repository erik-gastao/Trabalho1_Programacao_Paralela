#define _POSIX_C_SOURCE 200809L

/* Comparador sequencial (baseline do comparador).

   Mesma tarefa de comp_fork/comp_pthreads/comp_openmp: toma
   resultado_sequencial.csv como referencia e compara os outros 3 CSVs
   contra ele, celula a celula, com o mesmo criterio e o mesmo relatorio
   (comparador.c). A diferenca e que aqui nao ha divisao em blocos: uma
   unica chamada a comparar_linhas percorre todas as linhas. Serve de
   referencia de tempo para os comparadores paralelos, assim como
   cod_sequencial.c serve para a multiplicacao. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "comparador.h"

int main(void) {
    Resultado resultados[NUM_METODOS];
    Matriz referencia = carregar_resultados(resultados);

    Contagem total[NUM_METODOS];

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    comparar_linhas(referencia, resultados, 0, referencia.linhas, total);
    clock_gettime(CLOCK_MONOTONIC, &fim);

    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("Comparador sequencial: %d resultados %dx%d vs sequencial -> tempo de comparacao: %.6f s\n",
           NUM_METODOS, referencia.linhas, referencia.colunas, tempo_segundos);
    int codigo = reportar(referencia, resultados, total);

    liberar_resultados(referencia, resultados);
    return codigo;
}
