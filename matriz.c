#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
