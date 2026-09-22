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

void liberar_matriz(Matriz m) {
    for (int i = 0; i < m.linhas; i++) free(m.dados[i]);
    free(m.dados);
}
