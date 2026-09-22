#ifndef MATRIZ_H
#define MATRIZ_H

typedef struct {
    int linhas;
    int colunas;
    double **dados;
} Matriz;

Matriz ler_matriz_csv(const char *caminho);
void escrever_matriz_csv(const char *caminho, Matriz m);
void liberar_matriz(Matriz m);

#endif
