#ifndef MATRIZ_H
#define MATRIZ_H

typedef struct {
    int linhas;
    int colunas;
    double **dados;
} Matriz;

Matriz ler_matriz_csv(const char *caminho);
void escrever_matriz_csv(const char *caminho, Matriz m);
void escrever_bloco_csv(const char *caminho, Matriz m, int linha_inicio, int linha_fim);
void concatenar_partes_csv(const char *caminho, const char *prefixo, int quantidade);
void liberar_matriz(Matriz m);
int ler_num_workers(int argc, char *argv[], const char *nome, int padrao);

#endif
