#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
    Matriz c;
    int linha_inicio;
    int linha_fim;
    char nome_arquivo[64];
} TarefaEscrita;

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

/* Cada thread grava suas linhas num arquivo de parte proprio (sem lock);
   as partes sao concatenadas depois na ordem certa. */
static void *escrever_bloco(void *arg) {
    TarefaEscrita *tarefa = (TarefaEscrita *)arg;
    escrever_bloco_csv(tarefa->nome_arquivo, tarefa->c, tarefa->linha_inicio, tarefa->linha_fim);
    return NULL;
}

/* pthread_create/pthread_join devolvem o codigo de erro em vez de usar
   errno, entao perror() mostraria a mensagem errada: usa strerror(erro). */
static void criar_thread(pthread_t *thread, void *(*funcao)(void *), void *arg) {
    int erro = pthread_create(thread, NULL, funcao, arg);
    if (erro != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(erro));
        exit(1);
    }
}

static void esperar_thread(pthread_t thread) {
    int erro = pthread_join(thread, NULL);
    if (erro != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(erro));
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    /* Numero de threads: pedido no terminal (ou primeiro argumento);
       Enter vazio usa uma por nucleo disponivel. */
    long nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    int num_threads = ler_num_workers(argc, argv, "threads", (nucleos > 0) ? (int)nucleos : 4);

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

    if (num_threads > c.linhas) num_threads = c.linhas;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    TarefaThread *tarefas = malloc(num_threads * sizeof(TarefaThread));

    int linhas_por_thread = c.linhas / num_threads;
    int resto = c.linhas % num_threads;

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    int linha_atual = 0;
    for (int t = 0; t < num_threads; t++) {
        int tamanho_bloco = linhas_por_thread + (t < resto ? 1 : 0);
        tarefas[t] = (TarefaThread){
            .a = a, .b = b, .c = c,
            .linha_inicio = linha_atual,
            .linha_fim = linha_atual + tamanho_bloco,
        };
        linha_atual += tamanho_bloco;

        criar_thread(&threads[t], multiplicar_bloco, &tarefas[t]);
    }

    for (int t = 0; t < num_threads; t++) {
        esperar_thread(threads[t]);
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("Pthreads (%d threads): %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           num_threads, a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);
    printf("Pthreads: escrevendo resultado em paralelo...\n");

    const char *arquivo_resultado = "resultado_pthreads.csv";
    TarefaEscrita *tarefas_escrita = malloc(num_threads * sizeof(TarefaEscrita));
    for (int t = 0; t < num_threads; t++) {
        tarefas_escrita[t].c = c;
        tarefas_escrita[t].linha_inicio = tarefas[t].linha_inicio;
        tarefas_escrita[t].linha_fim = tarefas[t].linha_fim;
        snprintf(tarefas_escrita[t].nome_arquivo, sizeof(tarefas_escrita[t].nome_arquivo),
                 "%s.part%d", arquivo_resultado, t);

        criar_thread(&threads[t], escrever_bloco, &tarefas_escrita[t]);
    }
    for (int t = 0; t < num_threads; t++) {
        esperar_thread(threads[t]);
    }
    concatenar_partes_csv(arquivo_resultado, arquivo_resultado, num_threads);

    printf("Pthreads: resultado escrito em '%s'\n", arquivo_resultado);

    free(threads);
    free(tarefas);
    free(tarefas_escrita);
    liberar_matriz(a);
    liberar_matriz(b);
    liberar_matriz(c);
    return 0;
}
