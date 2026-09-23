#ifndef COMPARADOR_H
#define COMPARADOR_H

/* Parte comum do comparador paralelo de resultados (comp_fork.c,
   comp_pthreads.c, comp_openmp.c).

   Ideia: o resultado do sequencial e a referencia. Os outros 3 CSVs
   (fork, pthreads, openmp) sao comparados celula a celula contra ele.
   As linhas da matriz sao divididas em blocos contiguos entre os workers,
   igual a multiplicacao; cada worker conta quantas celulas divergem no
   seu bloco e onde esta a primeira divergencia. No fim, as contagens
   parciais sao juntadas (soma das divergencias, minimo da posicao).

   So a parte "dividir + comparar + juntar" muda entre fork, pthreads e
   openmp; leitura dos CSVs, criterio de divergencia e relatorio ficam
   aqui para as tres versoes usarem exatamente a mesma regra. */

#include <limits.h>
#include "matriz.h"

#define ARQUIVO_REFERENCIA "resultado_sequencial.csv"
#define NUM_METODOS 3                 /* fork, pthreads, openmp */
#define TOLERANCIA 1e-9               /* relativa ao valor de referencia */
#define SEM_DIVERGENCIA LONG_MAX      /* "nenhuma divergencia encontrada" */

typedef struct {
    int inicio;
    int fim;      /* exclusivo: linhas [inicio, fim) */
} Bloco;

/* Um resultado a ser verificado (resultado_fork.csv, etc.). */
typedef struct {
    const char *nome;
    const char *arquivo;
    int presente;       /* o arquivo existe? */
    int dimensao_ok;    /* mesmas linhas/colunas da referencia? */
    Matriz m;
} Resultado;

/* O que cada worker produz para um metodo, no seu bloco de linhas.
   'primeira' e a posicao linear (i * colunas + j) da primeira celula
   divergente, ou SEM_DIVERGENCIA. Usar posicao linear permite juntar
   as contagens de varios workers com um simples minimo. */
typedef struct {
    long divergencias;
    long primeira;
} Contagem;

/* Criterio de "inconsistencia": |obtido - esperado| > TOLERANCIA * (1 + |esperado|).
   Mistura tolerancia absoluta (valores perto de 0) e relativa (valores
   grandes). Como as 4 versoes somam na mesma ordem (i-k-j) e gravam com o
   mesmo formato, o esperado e diferenca zero; a tolerancia so evita falso
   alarme por arredondamento se a ordem das somas mudar um dia.
   Escrito como !(dif <= limite) para que NaN tambem conte como divergente
   (qualquer comparacao com NaN da falso). */
static inline int valores_divergem(double esperado, double obtido) {
    double dif = obtido - esperado;
    if (dif < 0) dif = -dif;
    double abs_esperado = esperado < 0 ? -esperado : esperado;
    return !(dif <= TOLERANCIA * (1.0 + abs_esperado));
}

/* Le a referencia e os 3 resultados. Arquivo de metodo ausente ou com
   dimensao diferente nao aborta: fica marcado e aparece no relatorio.
   Sem a referencia nao ha o que comparar, entao ai aborta. */
Matriz carregar_resultados(Resultado resultados[NUM_METODOS]);

/* Mesma divisao da multiplicacao: blocos contiguos, resto nos primeiros. */
void dividir_em_blocos(int total_linhas, int num_blocos, Bloco *blocos);

/* Nucleo usado por fork e pthreads: compara as linhas [linha_inicio,
   linha_fim) de cada metodo contra a referencia e grava o resultado em
   'saida' (um Contagem por metodo). */
void comparar_linhas(Matriz referencia, const Resultado resultados[NUM_METODOS],
                     int linha_inicio, int linha_fim, Contagem saida[NUM_METODOS]);

void zerar_contagens(Contagem contagens[NUM_METODOS]);

/* Junta uma contagem parcial no total: soma divergencias, minimo da posicao. */
void acumular_contagens(Contagem total[NUM_METODOS], const Contagem parcial[NUM_METODOS]);

/* Imprime o veredito de cada metodo. Retorna 0 se os 3 batem com a
   referencia, 1 caso contrario (serve como codigo de saida do programa). */
int reportar(Matriz referencia, const Resultado resultados[NUM_METODOS],
             const Contagem total[NUM_METODOS]);

void liberar_resultados(Matriz referencia, Resultado resultados[NUM_METODOS]);

#endif
