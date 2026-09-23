#include <stdio.h>
#include <stdlib.h>
#include "comparador.h"

static const char *NOMES[NUM_METODOS] = {"fork", "pthreads", "openmp"};
static const char *ARQUIVOS[NUM_METODOS] = {
    "resultado_fork.csv", "resultado_pthreads.csv", "resultado_openmp.csv"
};

static int arquivo_existe(const char *caminho) {
    FILE *arquivo = fopen(caminho, "r");
    if (!arquivo) return 0;
    fclose(arquivo);
    return 1;
}

Matriz carregar_resultados(Resultado resultados[NUM_METODOS]) {
    if (!arquivo_existe(ARQUIVO_REFERENCIA)) {
        fprintf(stderr, "Erro: '%s' nao encontrado (rodar ./cod_sequencial antes)\n",
                ARQUIVO_REFERENCIA);
        exit(1);
    }
    Matriz referencia = ler_matriz_csv(ARQUIVO_REFERENCIA);

    for (int m = 0; m < NUM_METODOS; m++) {
        Resultado *r = &resultados[m];
        r->nome = NOMES[m];
        r->arquivo = ARQUIVOS[m];
        r->presente = arquivo_existe(r->arquivo);
        r->m = (Matriz){0, 0, NULL};
        r->dimensao_ok = 0;
        if (r->presente) {
            r->m = ler_matriz_csv(r->arquivo);
            r->dimensao_ok = r->m.linhas == referencia.linhas &&
                             r->m.colunas == referencia.colunas;
        }
    }
    return referencia;
}

void dividir_em_blocos(int total_linhas, int num_blocos, Bloco *blocos) {
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

void comparar_linhas(Matriz referencia, const Resultado resultados[NUM_METODOS],
                     int linha_inicio, int linha_fim, Contagem saida[NUM_METODOS]) {
    for (int m = 0; m < NUM_METODOS; m++) {
        /* Contadores em variaveis locais (registradores), gravados em
           'saida' uma unica vez no final. Se incrementassemos saida[m]
           direto no laco, as Contagem de workers vizinhos (lado a lado na
           memoria) dividiriam a mesma linha de cache e cada escrita
           invalidaria a cache dos outros nucleos ("false sharing"). */
        long divergencias = 0;
        long primeira = SEM_DIVERGENCIA;

        const Resultado *r = &resultados[m];
        if (r->presente && r->dimensao_ok) {
            for (int i = linha_inicio; i < linha_fim; i++) {
                for (int j = 0; j < referencia.colunas; j++) {
                    if (valores_divergem(referencia.dados[i][j], r->m.dados[i][j])) {
                        /* Linhas percorridas em ordem crescente: a primeira
                           encontrada ja e a de menor posicao no bloco. */
                        if (divergencias == 0) primeira = (long)i * referencia.colunas + j;
                        divergencias++;
                    }
                }
            }
        }
        saida[m].divergencias = divergencias;
        saida[m].primeira = primeira;
    }
}

void zerar_contagens(Contagem contagens[NUM_METODOS]) {
    for (int m = 0; m < NUM_METODOS; m++) {
        contagens[m].divergencias = 0;
        contagens[m].primeira = SEM_DIVERGENCIA;
    }
}

void acumular_contagens(Contagem total[NUM_METODOS], const Contagem parcial[NUM_METODOS]) {
    for (int m = 0; m < NUM_METODOS; m++) {
        total[m].divergencias += parcial[m].divergencias;
        if (parcial[m].primeira < total[m].primeira) total[m].primeira = parcial[m].primeira;
    }
}

int reportar(Matriz referencia, const Resultado resultados[NUM_METODOS],
             const Contagem total[NUM_METODOS]) {
    int tudo_ok = 1;
    for (int m = 0; m < NUM_METODOS; m++) {
        const Resultado *r = &resultados[m];
        printf("  %-9s ", r->nome);
        if (!r->presente) {
            printf("NAO VERIFICADO: '%s' nao encontrado\n", r->arquivo);
            tudo_ok = 0;
        } else if (!r->dimensao_ok) {
            printf("DIMENSAO ERRADA: %dx%d, esperado %dx%d\n",
                   r->m.linhas, r->m.colunas, referencia.linhas, referencia.colunas);
            tudo_ok = 0;
        } else if (total[m].divergencias > 0) {
            int i = (int)(total[m].primeira / referencia.colunas);
            int j = (int)(total[m].primeira % referencia.colunas);
            printf("DIVERGENTE: %ld celula(s); primeira em [%d][%d]: esperado %.10g, obtido %.10g\n",
                   total[m].divergencias, i, j, referencia.dados[i][j], r->m.dados[i][j]);
            tudo_ok = 0;
        } else {
            printf("OK (igual ao sequencial)\n");
        }
    }
    printf(tudo_ok ? "Veredito: os 4 resultados sao consistentes\n"
                   : "Veredito: INCONSISTENCIA encontrada\n");
    return tudo_ok ? 0 : 1;
}

void liberar_resultados(Matriz referencia, Resultado resultados[NUM_METODOS]) {
    for (int m = 0; m < NUM_METODOS; m++) {
        if (resultados[m].presente) liberar_matriz(resultados[m].m);
    }
    liberar_matriz(referencia);
}
