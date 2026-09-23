#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "matriz.h"

typedef struct {
    int inicio;
    int fim;
} Bloco;

/* Divide 'total_linhas' em 'num_blocos' blocos contiguos, distribuindo o
   resto entre os primeiros. Usada pra dar um arquivo de parte por thread na
   escrita. O schedule(static) do calculo tambem faz blocos contiguos, mas o
   padrao OpenMP deixa a distribuicao do resto a criterio da implementacao
   (no GCC coincide com esta); a escrita nao depende disso, so le C pronta. */
static void dividir_em_blocos(int total_linhas, int num_blocos, Bloco *blocos) {
    int linhas_por_bloco = total_linhas / num_blocos;
    int resto = total_linhas % num_blocos;
    int linha_atual = 0;
    for (int p = 0; p < num_blocos; p++) {
        int tamanho = linhas_por_bloco + (p < resto ? 1 : 0);
        blocos[p].inicio = linha_atual;
        blocos[p].fim = linha_atual + tamanho;
        linha_atual = blocos[p].fim;
    }
}

int main(int argc, char *argv[]) {
    /* Numero de threads: pedido no terminal (ou primeiro argumento);
       Enter vazio usa o padrao do OpenMP (uma por nucleo). */
    int num_threads = ler_num_workers(argc, argv, "threads", omp_get_max_threads());

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
    omp_set_num_threads(num_threads);

    int threads_usadas = 1;

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    /* schedule(static) sem chunk: divide as linhas em blocos contiguos,
       um por thread -- mesma estrategia de divisao usada no fork e no pthreads. */
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < c.linhas; i++) {
        /* Numero real de threads do time (pode ser menor que o pedido se
           OMP_DYNAMIC/OMP_THREAD_LIMIT estiverem ativos). So a thread que
           pega a linha 0 escreve, e a leitura e depois da barreira. */
        if (i == 0) threads_usadas = omp_get_num_threads();

        for (int j = 0; j < c.colunas; j++) c.dados[i][j] = 0.0;
        for (int k = 0; k < a.colunas; k++) {
            double valor_a = a.dados[i][k];
            for (int j = 0; j < c.colunas; j++) {
                c.dados[i][j] += valor_a * b.dados[k][j];
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("OpenMP (%d threads): %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           threads_usadas, a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);
    printf("OpenMP: escrevendo resultado em paralelo...\n");

    /* Cada thread grava sua faixa de linhas num arquivo de parte proprio;
       as partes sao concatenadas depois na ordem certa. */
    const char *arquivo_resultado = "resultado_openmp.csv";
    Bloco *blocos = malloc(num_threads * sizeof(Bloco));
    dividir_em_blocos(c.linhas, num_threads, blocos);

    #pragma omp parallel for schedule(static)
    for (int p = 0; p < num_threads; p++) {
        char nome_parte[64];
        snprintf(nome_parte, sizeof(nome_parte), "%s.part%d", arquivo_resultado, p);
        escrever_bloco_csv(nome_parte, c, blocos[p].inicio, blocos[p].fim);
    }
    concatenar_partes_csv(arquivo_resultado, arquivo_resultado, num_threads);

    printf("OpenMP: resultado escrito em '%s'\n", arquivo_resultado);

    free(blocos);
    liberar_matriz(a);
    liberar_matriz(b);
    liberar_matriz(c);
    return 0;
}
