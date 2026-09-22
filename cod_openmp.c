#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "matriz.h"

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
    int num_threads = omp_get_max_threads();
    if (argc > 1) num_threads = atoi(argv[1]);
    if (num_threads < 1) num_threads = 1;
    omp_set_num_threads(num_threads);

    struct timespec inicio, fim;
    timespec_get(&inicio, TIME_UTC);

    /* schedule(static) sem chunk: divide as linhas em blocos contiguos,
       um por thread -- mesma estrategia de divisao usada no fork e no pthreads. */
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < c.linhas; i++) {
        for (int j = 0; j < c.colunas; j++) c.dados[i][j] = 0.0;
        for (int k = 0; k < a.colunas; k++) {
            double valor_a = a.dados[i][k];
            for (int j = 0; j < c.colunas; j++) {
                c.dados[i][j] += valor_a * b.dados[k][j];
            }
        }
    }

    timespec_get(&fim, TIME_UTC);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    escrever_matriz_csv("resultado_openmp.csv", c);
    printf("OpenMP (%d threads): %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           num_threads, a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);

    liberar_matriz(a);
    liberar_matriz(b);
    liberar_matriz(c);
    return 0;
}
