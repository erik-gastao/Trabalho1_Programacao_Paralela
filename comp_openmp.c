#define _POSIX_C_SOURCE 200809L

/* Comparador paralelo com OpenMP (diretivas).

   Aqui nao ha criacao manual de threads nem juncao manual: o laco sobre as
   linhas e anotado com '#pragma omp parallel for' e o compilador/runtime
   cria as threads, divide as iteracoes e espera todas no fim do laco
   (barreira implicita).

   schedule(static) sem chunk: cada thread pega um bloco contiguo de
   linhas, mesma estrategia do fork e do pthreads.

   A juncao das contagens usa clausulas 'reduction' (OpenMP 4.5+, reducao
   de vetores com a sintaxe var[:tamanho]):
     - reduction(+:divergencias[:N]) -> cada thread ganha uma copia privada
       zerada do vetor; no fim o runtime soma as copias no vetor original.
     - reduction(min:primeira[:N])   -> copia privada iniciada com o maior
       valor possivel; no fim fica o menor, ou seja, a primeira divergencia.
   E o equivalente, em uma linha de diretiva, ao que o fork faz com mmap e
   o pthreads faz com o vetor de tarefas + join + soma manual. Como cada
   thread incrementa sua copia privada, tambem nao ha false sharing. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "comparador.h"

int main(int argc, char *argv[]) {
    /* Numero de threads: pedido no terminal (ou primeiro argumento);
       Enter vazio usa o padrao do OpenMP (uma por nucleo). */
    int num_threads = ler_num_workers(argc, argv, "threads", omp_get_max_threads());

    Resultado resultados[NUM_METODOS];
    Matriz referencia = carregar_resultados(resultados);

    if (num_threads > referencia.linhas) num_threads = referencia.linhas;
    omp_set_num_threads(num_threads);

    /* Metodos que podem ser comparados (arquivo existe e dimensao bate). */
    int comparavel[NUM_METODOS];
    for (int m = 0; m < NUM_METODOS; m++) {
        comparavel[m] = resultados[m].presente && resultados[m].dimensao_ok;
    }

    long divergencias[NUM_METODOS];
    long primeira[NUM_METODOS];
    for (int m = 0; m < NUM_METODOS; m++) {
        divergencias[m] = 0;
        primeira[m] = SEM_DIVERGENCIA;
    }

    int threads_usadas = 1;

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    #pragma omp parallel for schedule(static) \
        reduction(+:divergencias[:NUM_METODOS]) reduction(min:primeira[:NUM_METODOS])
    for (int i = 0; i < referencia.linhas; i++) {
        /* Numero real de threads do time (pode ser menor que o pedido se
           OMP_DYNAMIC/OMP_THREAD_LIMIT estiverem ativos). */
        if (i == 0) threads_usadas = omp_get_num_threads();

        for (int m = 0; m < NUM_METODOS; m++) {
            if (!comparavel[m]) continue;
            for (int j = 0; j < referencia.colunas; j++) {
                if (valores_divergem(referencia.dados[i][j], resultados[m].m.dados[i][j])) {
                    divergencias[m]++;
                    long posicao = (long)i * referencia.colunas + j;
                    if (posicao < primeira[m]) primeira[m] = posicao;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    Contagem total[NUM_METODOS];
    for (int m = 0; m < NUM_METODOS; m++) {
        total[m].divergencias = divergencias[m];
        total[m].primeira = primeira[m];
    }

    printf("Comparador OpenMP (%d threads): %d resultados %dx%d vs sequencial -> tempo de comparacao: %.6f s\n",
           threads_usadas, NUM_METODOS, referencia.linhas, referencia.colunas, tempo_segundos);
    int codigo = reportar(referencia, resultados, total);

    liberar_resultados(referencia, resultados);
    return codigo;
}
