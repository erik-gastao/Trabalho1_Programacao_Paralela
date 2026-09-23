#define _POSIX_C_SOURCE 200809L

/* Comparador paralelo com fork (processos).

   Processos nao compartilham memoria por padrao: depois do fork() o filho
   tem uma COPIA do espaco de enderecos do pai. Isso serve para LER as
   matrizes (referencia e resultados, carregadas antes do fork, chegam aos
   filhos via copy-on-write, sem copia real enquanto ninguem escreve), mas
   nao serve para DEVOLVER as contagens: o que o filho escrevesse numa
   variavel comum ficaria so na copia dele.

   Por isso as contagens parciais vao numa area mmap(MAP_SHARED |
   MAP_ANONYMOUS), criada antes dos fork()s: essa regiao e a mesma pagina
   fisica no pai e em todos os filhos. Cada filho escreve so no seu slot
   (sem disputa, sem lock); o pai espera todos com wait() e soma os slots. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "comparador.h"

int main(int argc, char *argv[]) {
    /* Numero de processos: pedido no terminal (ou primeiro argumento);
       Enter vazio usa um por nucleo disponivel. */
    long nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    int num_processos = ler_num_workers(argc, argv, "processos", (nucleos > 0) ? (int)nucleos : 4);

    Resultado resultados[NUM_METODOS];
    Matriz referencia = carregar_resultados(resultados);

    if (num_processos > referencia.linhas) num_processos = referencia.linhas;

    Bloco *blocos = malloc(num_processos * sizeof(Bloco));
    if (!blocos) { perror("malloc"); exit(1); }
    dividir_em_blocos(referencia.linhas, num_processos, blocos);

    /* Slot do processo p: parciais[p * NUM_METODOS .. p * NUM_METODOS + 2]. */
    size_t tamanho_parciais = (size_t)num_processos * NUM_METODOS * sizeof(Contagem);
    Contagem *parciais = mmap(NULL, tamanho_parciais, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (parciais == MAP_FAILED) { perror("mmap"); exit(1); }

    /* Esvazia o buffer do stdout antes de duplicar o processo; senao cada
       filho herdaria texto pendente e poderia imprimi-lo de novo. */
    fflush(stdout);

    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (int p = 0; p < num_processos; p++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            comparar_linhas(referencia, resultados, blocos[p].inicio, blocos[p].fim,
                            parciais + (size_t)p * NUM_METODOS);
            _exit(0);   /* _exit: sai sem rodar atexit/flush herdados do pai */
        }
    }

    /* Espera todos os filhos e confere se cada um terminou normalmente.
       Um filho que morreu (sinal, falta de memoria) deixaria seu slot sem
       preencher, e a comparacao pareceria OK sem ter olhado aquelas linhas. */
    int algum_falhou = 0;
    for (int p = 0; p < num_processos; p++) {
        int status;
        if (wait(&status) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            algum_falhou = 1;
        }
    }

    /* Juncao das contagens parciais (parte do trabalho, entra no tempo). */
    Contagem total[NUM_METODOS];
    zerar_contagens(total);
    for (int p = 0; p < num_processos; p++) {
        acumular_contagens(total, parciais + (size_t)p * NUM_METODOS);
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_segundos = (fim.tv_sec - inicio.tv_sec) +
                             (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    if (algum_falhou) {
        fprintf(stderr, "Erro: algum processo filho falhou; comparacao invalida\n");
        exit(1);
    }

    printf("Comparador Fork (%d processos): %d resultados %dx%d vs sequencial -> tempo de comparacao: %.6f s\n",
           num_processos, NUM_METODOS, referencia.linhas, referencia.colunas, tempo_segundos);
    int codigo = reportar(referencia, resultados, total);

    munmap(parciais, tamanho_parciais);
    free(blocos);
    liberar_resultados(referencia, resultados);
    return codigo;
}
