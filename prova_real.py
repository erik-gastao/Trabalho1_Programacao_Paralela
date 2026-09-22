import argparse
import numpy as np

# Nomes dos arquivos de entrada (as matrizes originais, lidas pelos programas em C)
ARQUIVO_MATRIZ_A = "matriz_a.csv"
ARQUIVO_MATRIZ_B = "matriz_b.csv"

# Nomes dos arquivos de saída gerados por cada versão em C
ARQUIVOS_RESULTADO = {
    "Sequencial": "resultado_sequencial.csv",
    "Fork":       "resultado_fork.csv",
    "Pthreads":   "resultado_pthreads.csv",
    "OpenMP":     "resultado_openmp.csv",
}

# Tolerância usada na comparação, para lidar com pequenas diferenças
# de arredondamento entre ponto flutuante em C e em Python
TOLERANCIA = 1e-6


def calcular_resultado_esperado():
    """Lê as matrizes de entrada e calcula A x B com numpy, como referência."""
    matriz_a = np.loadtxt(ARQUIVO_MATRIZ_A, delimiter=",")
    matriz_b = np.loadtxt(ARQUIVO_MATRIZ_B, delimiter=",")

    if matriz_a.shape[1] != matriz_b.shape[0]:
        raise ValueError(
            f"Matrizes incompatíveis: A é {matriz_a.shape}, B é {matriz_b.shape}"
        )

    return matriz_a @ matriz_b


def verificar_resultado(nome_metodo, caminho_arquivo, resultado_esperado):
    """Compara o resultado de um método (lido de um CSV) com o resultado esperado."""
    try:
        resultado_obtido = np.loadtxt(caminho_arquivo, delimiter=",")
    except OSError:
        print(f"[{nome_metodo}] Arquivo não encontrado: {caminho_arquivo}")
        return

    # Garante que as duas matrizes têm a mesma dimensão antes de comparar valor a valor
    if resultado_obtido.shape != resultado_esperado.shape:
        print(
            f"[{nome_metodo}] Dimensão incorreta: "
            f"esperado {resultado_esperado.shape}, obtido {resultado_obtido.shape}"
        )
        return

    if np.allclose(resultado_obtido, resultado_esperado, atol=TOLERANCIA):
        print(f"[{nome_metodo}] Resultado correto")
    else:
        diferenca = np.abs(resultado_obtido - resultado_esperado)
        print(f"[{nome_metodo}] Resultado DIVERGENTE (maior diferença: {diferenca.max()})")


def ler_argumentos():
    """Define e lê o argumento opcional de linha de comando: qual método verificar."""
    parser = argparse.ArgumentParser(
        description="Verifica se o resultado de uma (ou todas) as multiplicações em C está correto."
    )
    parser.add_argument(
        "metodo",
        nargs="?",  # torna o argumento opcional
        choices=list(ARQUIVOS_RESULTADO.keys()),
        default=None,
        help="Método a verificar (Sequencial, Fork, Pthreads, OpenMP). "
             "Se omitido, verifica todos.",
    )
    return parser.parse_args()


def main():
    argumentos = ler_argumentos()
    resultado_esperado = calcular_resultado_esperado()
    print("Resultado esperado (calculado com numpy):")
    print(resultado_esperado)
    print()

    if argumentos.metodo is None:
        # Nenhum método selecionado: verifica os quatro
        metodos_a_verificar = ARQUIVOS_RESULTADO
    else:
        # Apenas o método selecionado
        metodos_a_verificar = {argumentos.metodo: ARQUIVOS_RESULTADO[argumentos.metodo]}

    for nome_metodo, caminho_arquivo in metodos_a_verificar.items():
        verificar_resultado(nome_metodo, caminho_arquivo, resultado_esperado)


if __name__ == "__main__":
    main()
