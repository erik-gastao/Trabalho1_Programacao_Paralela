#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "matriz.h"

/* Aloca um buffer de doubles em memoria compartilhada (visivel entre pai e
   filhos apos o fork). mmap anonimo ja vem zerado pelo kernel. */
static double *alocar_buffer_compartilhado(size_t total_elementos) {
    size_t tamanho_bytes = total_elementos * sizeof(double);
    void *ptr = mmap(NULL, tamanho_bytes, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return (double *)ptr;
}

/* Calcula as linhas [linha_inicio, linha_fim) de C = A x B, escrevendo
   direto no buffer compartilhado. Ordem i-k-j por localidade de cache. */
static void multiplicar_bloco(Matriz a, Matriz b, double *buffer_c,
                               int linha_inicio, int linha_fim, int colunas_c) {
    for (int i = linha_inicio; i < linha_fim; i++) {
        double *linha_c = buffer_c + (size_t)i * colunas_c;
        for (int k = 0; k < a.colunas; k++) {
            double valor_a = a.dados[i][k];
            for (int j = 0; j < colunas_c; j++) {
                linha_c[j] += valor_a * b.dados[k][j];
            }
        }
    }
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

    int linhas_c = a.linhas;
    int colunas_c = b.colunas;

    /* Numero de processos: por padrao, um por nucleo disponivel;
       pode ser sobrescrito pelo primeiro argumento de linha de comando. */
    long nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    int num_processos = (nucleos > 0) ? (int)nucleos : 4;
    if (argc > 1) num_processos = atoi(argv[1]);
    if (num_processos < 1) num_processos = 1;
    if (num_processos > linhas_c) num_processos = linhas_c;

    double *buffer_c = alocar_buffer_compartilhado((size_t)linhas_c * colunas_c);

    int linhas_por_processo = linhas_c / num_processos;
    int resto = linhas_c % num_processos;

    struct timespec inicio, fim;
    timespec_get(&inicio, TIME_UTC);

    int linha_atual = 0;
    for (int p = 0; p < num_processos; p++) {
        int tamanho_bloco = linhas_por_processo + (p < resto ? 1 : 0);
        int linha_inicio = linha_atual;
        int linha_fim = linha_inicio + tamanho_bloco;
        linha_atual = linha_fim;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            multiplicar_bloco(a, b, buffer_c, linha_inicio, linha_fim, colunas_c);
            _exit(0);
        }
    }

    for (int p = 0; p < num_processos; p++) {
        wait(NULL);
    }

    timespec_get(&fim, TIME_UTC);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    Matriz c = {linhas_c, colunas_c, malloc(linhas_c * sizeof(double *))};
    for (int i = 0; i < linhas_c; i++) {
        c.dados[i] = buffer_c + (size_t)i * colunas_c;
    }

    escrever_matriz_csv("resultado_fork.csv", c);
    printf("Fork (%d processos): %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           num_processos, a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);

    free(c.dados);
    munmap(buffer_c, (size_t)linhas_c * colunas_c * sizeof(double));
    liberar_matriz(a);
    liberar_matriz(b);
    return 0;
}
