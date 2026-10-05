/* _GNU_SOURCE: libera sched_setaffinity/CPU_SET (afinidade de CPU). */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include "matriz.h"

#define TAM_INICIAL_LINHA 256
#define CAPACIDADE_INICIAL_LINHAS 8

/* Le uma linha do arquivo, de tamanho arbitrario, sem \n ou \r.
   Retorna NULL em EOF sem ter lido nada. */
static char *ler_linha(FILE *arquivo) {
    size_t capacidade = TAM_INICIAL_LINHA;
    size_t tamanho = 0;
    char *linha = malloc(capacidade);
    if (!linha) { perror("malloc"); exit(1); }

    int c;
    int leu_algo = 0;
    while ((c = fgetc(arquivo)) != EOF) {
        leu_algo = 1;
        if (c == '\n') break;
        if (tamanho + 1 >= capacidade) {
            capacidade *= 2;
            char *nova = realloc(linha, capacidade);
            if (!nova) { perror("realloc"); free(linha); exit(1); }
            linha = nova;
        }
        if (c != '\r') linha[tamanho++] = (char)c;
    }
    if (!leu_algo) { free(linha); return NULL; }
    linha[tamanho] = '\0';
    return linha;
}

static int contar_colunas(const char *linha) {
    int colunas = 1;
    for (const char *p = linha; *p; p++) if (*p == ',') colunas++;
    return colunas;
}

Matriz ler_matriz_csv(const char *caminho) {
    FILE *arquivo = fopen(caminho, "r");
    if (!arquivo) {
        fprintf(stderr, "Erro ao abrir '%s': %s\n", caminho, strerror(errno));
        exit(1);
    }

    Matriz m = {0, 0, NULL};
    int capacidade_linhas = CAPACIDADE_INICIAL_LINHAS;
    m.dados = malloc(capacidade_linhas * sizeof(double *));
    if (!m.dados) { perror("malloc"); exit(1); }

    char *linha;
    while ((linha = ler_linha(arquivo)) != NULL) {
        if (linha[0] == '\0') { free(linha); continue; }

        int colunas = contar_colunas(linha);
        if (m.linhas == 0) {
            m.colunas = colunas;
        } else if (colunas != m.colunas) {
            fprintf(stderr, "Erro: linha %d de '%s' tem %d colunas, esperado %d\n",
                    m.linhas + 1, caminho, colunas, m.colunas);
            exit(1);
        }

        if (m.linhas >= capacidade_linhas) {
            capacidade_linhas *= 2;
            double **novo = realloc(m.dados, capacidade_linhas * sizeof(double *));
            if (!novo) { perror("realloc"); exit(1); }
            m.dados = novo;
        }

        double *linha_valores = malloc(m.colunas * sizeof(double));
        if (!linha_valores) { perror("malloc"); exit(1); }

        char *token = strtok(linha, ",");
        for (int j = 0; j < m.colunas; j++) {
            if (!token) {
                fprintf(stderr, "Erro: linha %d de '%s' malformada\n", m.linhas + 1, caminho);
                exit(1);
            }
            linha_valores[j] = strtod(token, NULL);
            token = strtok(NULL, ",");
        }
        m.dados[m.linhas++] = linha_valores;
        free(linha);
    }

    fclose(arquivo);

    if (m.linhas == 0) {
        fprintf(stderr, "Erro: '%s' esta vazio\n", caminho);
        exit(1);
    }
    return m;
}

void escrever_matriz_csv(const char *caminho, Matriz m) {
    FILE *arquivo = fopen(caminho, "w");
    if (!arquivo) {
        fprintf(stderr, "Erro ao criar '%s': %s\n", caminho, strerror(errno));
        exit(1);
    }
    for (int i = 0; i < m.linhas; i++) {
        for (int j = 0; j < m.colunas; j++) {
            fprintf(arquivo, "%.10g", m.dados[i][j]);
            if (j + 1 < m.colunas) fputc(',', arquivo);
        }
        fputc('\n', arquivo);
    }
    fclose(arquivo);
}

/* Escreve so as linhas [linha_inicio, linha_fim) de m num arquivo proprio.
   Usado pelas versoes paralelas: cada processo/thread grava sua parte num
   arquivo separado (sem disputa de lock), concatenados depois. */
void escrever_bloco_csv(const char *caminho, Matriz m, int linha_inicio, int linha_fim) {
    FILE *arquivo = fopen(caminho, "w");
    if (!arquivo) {
        fprintf(stderr, "Erro ao criar '%s': %s\n", caminho, strerror(errno));
        exit(1);
    }
    for (int i = linha_inicio; i < linha_fim; i++) {
        for (int j = 0; j < m.colunas; j++) {
            fprintf(arquivo, "%.10g", m.dados[i][j]);
            if (j + 1 < m.colunas) fputc(',', arquivo);
        }
        fputc('\n', arquivo);
    }
    fclose(arquivo);
}

/* Concatena 'quantidade' arquivos "<prefixo>.part0", "<prefixo>.part1", ...
   no arquivo final 'caminho', na ordem, e apaga as partes. */
void concatenar_partes_csv(const char *caminho, const char *prefixo, int quantidade) {
    FILE *destino = fopen(caminho, "w");
    if (!destino) {
        fprintf(stderr, "Erro ao criar '%s': %s\n", caminho, strerror(errno));
        exit(1);
    }

    char nome_parte[512];
    char buffer[65536];
    for (int p = 0; p < quantidade; p++) {
        snprintf(nome_parte, sizeof(nome_parte), "%s.part%d", prefixo, p);

        FILE *parte = fopen(nome_parte, "r");
        if (!parte) {
            fprintf(stderr, "Erro ao abrir '%s': %s\n", nome_parte, strerror(errno));
            exit(1);
        }

        size_t lidos;
        while ((lidos = fread(buffer, 1, sizeof(buffer), parte)) > 0) {
            fwrite(buffer, 1, lidos, destino);
        }
        fclose(parte);
        remove(nome_parte);
    }
    fclose(destino);
}

void liberar_matriz(Matriz m) {
    for (int i = 0; i < m.linhas; i++) free(m.dados[i]);
    free(m.dados);
}

#define MAX_WORKERS 1024

/* Numero de processos/threads ('nome' so aparece na pergunta).
   Se veio como argumento (ex.: ./cod_openmp 4), usa ele sem perguntar;
   senao pergunta no terminal. Enter vazio (ou fim da entrada) usa 'padrao'.
   Os programas chamam isto antes de ler os CSVs, fora da medicao de tempo:
   a espera pela digitacao nao entra no tempo medido. */
int ler_num_workers(int argc, char *argv[], const char *nome, int padrao) {
    if (argc > 1) {
        int n = atoi(argv[1]);
        return n >= 1 ? n : 1;
    }

    char entrada[64];
    for (;;) {
        printf("Numero de %s (Enter = %d): ", nome, padrao);
        fflush(stdout);
        if (!fgets(entrada, sizeof(entrada), stdin)) {
            printf("\n");
            return padrao;
        }

        char *p = entrada;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || *p == '\0') return padrao;

        char *fim;
        long n = strtol(p, &fim, 10);
        while (*fim == ' ' || *fim == '\t' || *fim == '\n' || *fim == '\r') fim++;
        if (fim != p && *fim == '\0' && n >= 1 && n <= MAX_WORKERS) return (int)n;

        printf("Valor invalido: digite um inteiro entre 1 e %d.\n", MAX_WORKERS);
    }
}

/* ---- Afinidade: travar cada worker num nucleo fixo ----

   Sem isto, o escalonador do Linux pode mover threads/processos de um
   nucleo pro outro no meio do calculo (perdendo a cache L1/L2 daquele
   nucleo) ou colocar dois workers no mesmo nucleo fisico enquanto outro
   fica livre. Isso faz o tempo variar bastante entre execucoes.

   Ordem dos nucleos: primeiro uma CPU logica de cada nucleo fisico, depois
   as "irmas" de SMT (hyperthreading). Ex.: Ryzen 8 nucleos/16 threads,
   irmas (0,1), (2,3)... -> ordem 0,2,4,...,14, 1,3,...,15. Assim, com ate
   8 workers cada um fica sozinho num nucleo fisico. A topologia e lida de
   /sys; se nao der, usa a numeracao direta 0,1,2,... */

static int ordem_nucleos[MAX_WORKERS];
static int total_nucleos = 0;

/* Primeira CPU listada em thread_siblings_list (ex.: "2-3" -> 2), ou -1. */
static int primeira_irma(int cpu) {
    char caminho[96];
    snprintf(caminho, sizeof(caminho),
             "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", cpu);
    FILE *arquivo = fopen(caminho, "r");
    if (!arquivo) return -1;
    int primeira = -1;
    if (fscanf(arquivo, "%d", &primeira) != 1) primeira = -1;
    fclose(arquivo);
    return primeira;
}

void preparar_afinidade(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    if (n > MAX_WORKERS) n = MAX_WORKERS;

    int principal[MAX_WORKERS];
    int topologia_ok = 1;
    for (int cpu = 0; cpu < n; cpu++) {
        int irma = primeira_irma(cpu);
        if (irma < 0) topologia_ok = 0;
        principal[cpu] = (irma == cpu);
    }

    total_nucleos = 0;
    if (topologia_ok) {
        for (int cpu = 0; cpu < n; cpu++) if (principal[cpu]) ordem_nucleos[total_nucleos++] = cpu;
        for (int cpu = 0; cpu < n; cpu++) if (!principal[cpu]) ordem_nucleos[total_nucleos++] = cpu;
    } else {
        for (int cpu = 0; cpu < n; cpu++) ordem_nucleos[total_nucleos++] = cpu;
    }
}

void fixar_worker(int worker) {
    if (total_nucleos == 0) return;   /* preparar_afinidade nao foi chamada */
    cpu_set_t conjunto;
    CPU_ZERO(&conjunto);
    CPU_SET(ordem_nucleos[worker % total_nucleos], &conjunto);
    /* pid 0 = a thread/processo que chamou. Se falhar, segue sem travar. */
    sched_setaffinity(0, sizeof(conjunto), &conjunto);
}
