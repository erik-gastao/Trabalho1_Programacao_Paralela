#define _POSIX_C_SOURCE 200809L

/* Comparador paralelo com Pthreads (threads).

   Diferente do fork, threads do mesmo processo ja compartilham toda a
   memoria: as matrizes e o vetor de tarefas sao vistos por todas, sem mmap.
   Cada thread recebe um ponteiro para a SUA TarefaComparacao, com o bloco
   de linhas a comparar e o espaco onde deixar suas contagens. Como cada
   thread so escreve na propria tarefa, nao precisa de mutex.

   A "reducao" (juntar as contagens) e manual: a thread principal espera
   todas com pthread_join e so depois soma os resultados parciais. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "comparador.h"

typedef struct {
    Matriz referencia;
    const Resultado *resultados;   /* compartilhado, so leitura */
    int linha_inicio;
    int linha_fim;
    Contagem parcial[NUM_METODOS]; /* saida desta thread */
} TarefaComparacao;

static void *comparar_bloco(void *arg) {
    TarefaComparacao *tarefa = (TarefaComparacao *)arg;
    comparar_linhas(tarefa->referencia, tarefa->resultados,
                    tarefa->linha_inicio, tarefa->linha_fim, tarefa->parcial);
    return NULL;
}

int main(int argc, char *argv[]) {
    /* Numero de threads: pedido no terminal (ou primeiro argumento);
       Enter vazio usa uma por nucleo disponivel. */
    long nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    int num_threads = ler_num_workers(argc, argv, "threads", (nucleos > 0) ? (int)nucleos : 4);

    Resultado resultados[NUM_METODOS];
    Matriz referencia = carregar_resultados(resultados);

    if (num_threads > referencia.linhas) num_threads = referencia.linhas;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    TarefaComparacao *tarefas = malloc(num_threads * sizeof(TarefaComparacao));
    Bloco *blocos = malloc(num_threads * sizeof(Bloco));
    if (!threads || !tarefas || !blocos) { perror("malloc"); exit(1); }
    dividir_em_blocos(referencia.linhas, num_threads, blocos);

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (int t = 0; t < num_threads; t++) {
        tarefas[t].referencia = referencia;
        tarefas[t].resultados = resultados;
        tarefas[t].linha_inicio = blocos[t].inicio;
        tarefas[t].linha_fim = blocos[t].fim;

        /* pthread_create devolve o codigo de erro (nao usa errno). */
        int erro = pthread_create(&threads[t], NULL, comparar_bloco, &tarefas[t]);
        if (erro != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(erro));
            exit(1);
        }
    }

    /* Barreira: so junta as contagens depois que todas as threads acabaram. */
    for (int t = 0; t < num_threads; t++) {
        int erro = pthread_join(threads[t], NULL);
        if (erro != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(erro));
            exit(1);
        }
    }

    Contagem total[NUM_METODOS];
    zerar_contagens(total);
    for (int t = 0; t < num_threads; t++) acumular_contagens(total, tarefas[t].parcial);

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("Comparador Pthreads (%d threads): %d resultados %dx%d vs sequencial -> tempo de comparacao: %.6f s\n",
           num_threads, NUM_METODOS, referencia.linhas, referencia.colunas, tempo_segundos);
    int codigo = reportar(referencia, resultados, total);

    free(threads);
    free(tarefas);
    free(blocos);
    liberar_resultados(referencia, resultados);
    return codigo;
}
