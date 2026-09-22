import random


def escrever_matriz_aleatoria(caminho, n, minimo=0, maximo=99):
    """Gera uma matriz N x N com valores inteiros aleatorios e grava em CSV."""
    with open(caminho, "w") as arquivo:
        for _ in range(n):
            linha = [str(random.randint(minimo, maximo)) for _ in range(n)]
            arquivo.write(",".join(linha) + "\n")


def main():
    n = int(input("Tamanho das matrizes: "))
    escrever_matriz_aleatoria("matriz_a.csv", n)
    escrever_matriz_aleatoria("matriz_b.csv", n)
    print(f"matriz_a.csv e matriz_b.csv geradas: {n}x{n}, valores aleatorios de 0 a 99")


if __name__ == "__main__":
    main()
