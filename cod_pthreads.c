#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "matriz.h"

typedef struct {
    Matriz a;
    Matriz b;
    Matriz c;
    int linha_inicio;
    int linha_fim;
} TarefaThread;

/* Calcula as linhas [linha_inicio, linha_fim) de C = A x B.
   Cada thread escreve em linhas distintas de C: sem necessidade de lock. */
static void *multiplicar_bloco(void *arg) {
    TarefaThread *tarefa = (TarefaThread *)arg;
    Matriz a = tarefa->a, b = tarefa->b, c = tarefa->c;

    for (int i = tarefa->linha_inicio; i < tarefa->linha_fim; i++) {
        for (int j = 0; j < c.colunas; j++) c.dados[i][j] = 0.0;
        for (int k = 0; k < a.colunas; k++) {
            double valor_a = a.dados[i][k];
            for (int j = 0; j < c.colunas; j++) {
                c.dados[i][j] += valor_a * b.dados[k][j];
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    Matriz a = ler_matriz_csv("matriz_a.csv");
    Matriz b = ler_matriz_csv("matriz_b.csv");

    if (a.colunas != b.linhas) {
        fprintf(stderr,
                "Erro: matrizes incompativeis (A e %dx%d, B e %dx%d)\n",
                a.linhas, a.colunas, b.linhas, b.colunas);
        liberar_matriz(a);
        liberar_matriz(b);
        return 1;
    }

    Matriz c = {a.linhas, b.colunas, NULL};
    c.dados = malloc(c.linhas * sizeof(double *));
    for (int i = 0; i < c.linhas; i++) {
        c.dados[i] = malloc(c.colunas * sizeof(double));
    }

    /* Numero de threads: por padrao, uma por nucleo disponivel;
       pode ser sobrescrito pelo primeiro argumento de linha de comando. */
    long nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    int num_threads = (nucleos > 0) ? (int)nucleos : 4;
    if (argc > 1) num_threads = atoi(argv[1]);
    if (num_threads < 1) num_threads = 1;
    if (num_threads > c.linhas) num_threads = c.linhas;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    TarefaThread *tarefas = malloc(num_threads * sizeof(TarefaThread));

    int linhas_por_thread = c.linhas / num_threads;
    int resto = c.linhas % num_threads;

    struct timespec inicio, fim;
    timespec_get(&inicio, TIME_UTC);

    int linha_atual = 0;
    for (int t = 0; t < num_threads; t++) {
        int tamanho_bloco = linhas_por_thread + (t < resto ? 1 : 0);
        tarefas[t] = (TarefaThread){
            .a = a, .b = b, .c = c,
            .linha_inicio = linha_atual,
            .linha_fim = linha_atual + tamanho_bloco,
        };
        linha_atual += tamanho_bloco;

        if (pthread_create(&threads[t], NULL, multiplicar_bloco, &tarefas[t]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }

    timespec_get(&fim, TIME_UTC);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    escrever_matriz_csv("resultado_pthreads.csv", c);
    printf("Pthreads (%d threads): %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           num_threads, a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);

    free(threads);
    free(tarefas);
    liberar_matriz(a);
    liberar_matriz(b);
    liberar_matriz(c);
    return 0;
}
