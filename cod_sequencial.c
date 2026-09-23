#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "matriz.h"

/* Ordem de laco i-k-j: percorre b.dados[k] e c.dados[i] sequencialmente
   na memoria, o que e mais eficiente em cache que o i-j-k classico. */
static Matriz multiplicar(Matriz a, Matriz b) {
    Matriz c = {a.linhas, b.colunas, NULL};
    c.dados = malloc(c.linhas * sizeof(double *));
    for (int i = 0; i < c.linhas; i++) {
        c.dados[i] = calloc(c.colunas, sizeof(double));
        for (int k = 0; k < a.colunas; k++) {
            double valor_a = a.dados[i][k];
            for (int j = 0; j < c.colunas; j++) {
                c.dados[i][j] += valor_a * b.dados[k][j];
            }
        }
    }
    return c;
}

int main(void) {
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

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    Matriz c = multiplicar(a, b);
    clock_gettime(CLOCK_MONOTONIC, &fim);

    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("Sequencial: %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);
    printf("Sequencial: escrevendo resultado...\n");

    escrever_matriz_csv("resultado_sequencial.csv", c);

    printf("Sequencial: resultado escrito em 'resultado_sequencial.csv'\n");

    liberar_matriz(a);
    liberar_matriz(b);
    liberar_matriz(c);
    return 0;
}
