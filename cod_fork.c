#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "matriz.h"

typedef struct {
    int inicio;
    int fim;
} Bloco;

/* Divide 'total_linhas' em 'num_blocos' blocos contiguos, distribuindo o
   resto entre os primeiros. Mesma divisao usada tanto pro calculo quanto
   pra escrita, entao cada processo cuida sempre das mesmas linhas. */
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

/* Espera todos os filhos e aborta se algum nao terminou normalmente
   (sinal, falta de memoria, exit != 0): as linhas daquele filho ficariam
   zeradas/faltando e o resultado sairia errado sem nenhum aviso. */
static void esperar_filhos(int quantidade) {
    int algum_falhou = 0;
    for (int p = 0; p < quantidade; p++) {
        int status;
        if (wait(&status) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            algum_falhou = 1;
        }
    }
    if (algum_falhou) {
        fprintf(stderr, "Erro: algum processo filho falhou; resultado invalido\n");
        exit(1);
    }
}

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
    /* Numero de processos: pedido no terminal (ou primeiro argumento);
       Enter vazio usa um por nucleo disponivel. */
    long nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    int num_processos = ler_num_workers(argc, argv, "processos", (nucleos > 0) ? (int)nucleos : 4);

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

    if (num_processos > linhas_c) num_processos = linhas_c;

    double *buffer_c = alocar_buffer_compartilhado((size_t)linhas_c * colunas_c);

    Bloco *blocos = malloc(num_processos * sizeof(Bloco));
    dividir_em_blocos(linhas_c, num_processos, blocos);

    /* Esvazia o buffer do stdout antes de duplicar o processo; senao cada
       filho herdaria texto pendente e poderia imprimi-lo de novo. */
    fflush(stdout);

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (int p = 0; p < num_processos; p++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            multiplicar_bloco(a, b, buffer_c, blocos[p].inicio, blocos[p].fim, colunas_c);
            _exit(0);
        }
    }
    esperar_filhos(num_processos);

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("Fork (%d processos): %dx%d * %dx%d -> tempo de multiplicacao: %.6f s\n",
           num_processos, a.linhas, a.colunas, b.linhas, b.colunas, tempo_segundos);
    printf("Fork: escrevendo resultado em paralelo...\n");

    Matriz c = {linhas_c, colunas_c, malloc(linhas_c * sizeof(double *))};
    for (int i = 0; i < linhas_c; i++) {
        c.dados[i] = buffer_c + (size_t)i * colunas_c;
    }

    /* Cada filho grava sua faixa de linhas num arquivo de parte proprio;
       o pai so concatena no final, sem disputa de escrita entre eles. */
    const char *arquivo_resultado = "resultado_fork.csv";
    fflush(stdout);
    for (int p = 0; p < num_processos; p++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            char nome_parte[64];
            snprintf(nome_parte, sizeof(nome_parte), "%s.part%d", arquivo_resultado, p);
            escrever_bloco_csv(nome_parte, c, blocos[p].inicio, blocos[p].fim);
            _exit(0);
        }
    }
    esperar_filhos(num_processos);
    concatenar_partes_csv(arquivo_resultado, arquivo_resultado, num_processos);

    printf("Fork: resultado escrito em '%s'\n", arquivo_resultado);

    free(c.dados);
    free(blocos);
    munmap(buffer_c, (size_t)linhas_c * colunas_c * sizeof(double));
    liberar_matriz(a);
    liberar_matriz(b);
    return 0;
}
