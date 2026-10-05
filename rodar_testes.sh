#!/usr/bin/env bash
# Bateria de testes: gera as matrizes, roda as 4 multiplicacoes e depois os
# 4 comparadores, guardando o comando ("prompt") e a saida de cada execucao.
#
# Uso (da raiz do projeto, pelo Windows):
#   wsl bash rodar_testes.sh                 -> N = 1000 2000 3000 4000
#   wsl bash rodar_testes.sh 1000 2000       -> so os tamanhos indicados
#   wsl env REPETICOES=3 bash rodar_testes.sh -> outra quantidade de repeticoes
#
# Cada configuracao (programa + threads + N) roda $REPETICOES vezes (padrao 5)
# e o tempo considerado e a MEDIANA, pra diminuir o efeito da variacao entre
# execucoes (no WSL2 ela passa de 15% nas versoes paralelas).
#
# Saida (pasta testes/ do projeto):
#   testes-matriz-1k.txt ...      multiplicacoes (sequencial, fork, pthreads, openmp)
#   testes-comparador-1k.txt ...  comparadores (sequencial, fork, pthreads, openmp)
#   resumo.csv                    uma linha por configuracao: mediana, min, max
#   execucoes.csv                 uma linha por execucao individual
#
# As matrizes e resultados ficam em $DIR_TRABALHO/N<tamanho>, fora do projeto:
# os matriz_a.csv/matriz_b.csv da raiz nao sao tocados, e o disco do WSL e
# bem mais rapido que /mnt/c pros CSVs grandes (4000x4000 passa de 700 MB).

set -u

PROJETO="$(cd "$(dirname "$0")" && pwd)"
DIR_TRABALHO="${DIR_TRABALHO:-$HOME/trabalho1_testes}"
DIR_LOGS="${DIR_LOGS:-$PROJETO/testes}"
TAMANHOS=("$@")
[ ${#TAMANHOS[@]} -eq 0 ] && TAMANHOS=(1000 2000 3000 4000)
THREADS=(2 4 8 16)
METODOS=(fork pthreads openmp)
REPETICOES="${REPETICOES:-5}"   # execucoes por configuracao (mediana)
CFLAGS="-std=c11 -Wall -Wextra -O2"

mkdir -p "$DIR_TRABALHO" "$DIR_LOGS"
RESUMO="$DIR_LOGS/resumo.csv"
EXECUCOES="$DIR_LOGS/execucoes.csv"
echo "fase,n,programa,threads,execucoes,mediana_tempo_medido_s,min_tempo_medido_s,max_tempo_medido_s,mediana_tempo_total_processo_s,execucoes_com_erro,tempos_medidos_s" > "$RESUMO"
echo "fase,n,programa,threads,execucao,tempo_medido_s,tempo_total_processo_s,codigo_saida" > "$EXECUCOES"

# 1000 -> 1k, 2500 -> 2500
rotulo() {
    if [ $(( $1 % 1000 )) -eq 0 ]; then echo "$(( $1 / 1000 ))k"; else echo "$1"; fi
}

cabecalho() {   # cabecalho <arquivo> <titulo> <n>
    {
        echo "################################################################"
        echo "# $2"
        echo "# Matriz: $3 x $3"
        echo "# Data: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "# CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ //'), $(nproc) CPUs logicas"
        echo "# Compilacao: gcc $CFLAGS (+ -pthread / -fopenmp)"
        echo "# Pasta de execucao: $DIR_TRABALHO/N$3"
        echo "################################################################"
        echo
    } > "$1"
}

# Mediana de uma lista de numeros (argumentos). Vazia -> nada.
mediana() {
    [ $# -eq 0 ] && return
    printf '%s\n' "$@" | sort -g | awk '{ v[NR] = $1 }
        END { if (NR == 0) exit;
              if (NR % 2) printf "%.6f", v[(NR + 1) / 2];
              else        printf "%.6f", (v[NR / 2] + v[NR / 2 + 1]) / 2 }'
}

# executar <log> <fase> <n> <programa> <threads ou -> <repeticoes> <prompt> <comando...>
# Roda o comando <repeticoes> vezes. Cada execucao vai pro log com o prompt
# (como seria digitado na pasta do teste) e a saida; no fim vai um resumo
# com a mediana. Cada execucao vira uma linha em execucoes.csv e a
# configuracao inteira uma linha em resumo.csv.
executar() {
    local log="$1" fase="$2" n="$3" programa="$4" threads="$5" repeticoes="$6" prompt="$7"
    shift 7

    local medidos=() totais=() falhas=0
    local r
    for (( r = 1; r <= repeticoes; r++ )); do
        echo ">> [N=$n] $prompt   (execucao $r/$repeticoes)"
        {
            echo "================================================================"
            echo "PROMPT: \$ $prompt   [execucao $r de $repeticoes]"
            echo "----------------------------------------------------------------"
        } >> "$log"

        local inicio fim saida codigo total medido
        inicio=$(date +%s.%N)
        saida=$("$@" < /dev/null 2>&1)
        codigo=$?
        fim=$(date +%s.%N)
        total=$(awk -v a="$inicio" -v b="$fim" 'BEGIN { printf "%.3f", b - a }')

        {
            echo "$saida"
            echo "----------------------------------------------------------------"
            echo "[codigo de saida: $codigo | tempo total do processo (leitura + calculo + escrita): ${total} s]"
            echo
        } >> "$log"

        # Tempo que o proprio programa mede ("multiplicacao: X s" ou "comparacao: X s").
        medido=$(echo "$saida" | grep -oE '(multiplicacao|comparacao): [0-9.]+' | head -1 | awk '{print $2}')
        echo "$fase,$n,$programa,$threads,$r,${medido:-},$total,$codigo" >> "$EXECUCOES"

        [ -n "$medido" ] && medidos+=("$medido")
        totais+=("$total")
        if [ $codigo -ne 0 ]; then
            falhas=$(( falhas + 1 ))
            echo "   !! codigo de saida $codigo (ver $log)"
        fi
    done

    local med_medido med_total minimo maximo lista
    med_medido=$(mediana "${medidos[@]}")
    med_total=$(mediana "${totais[@]}")
    minimo=$( [ ${#medidos[@]} -gt 0 ] && printf '%s\n' "${medidos[@]}" | sort -g | head -1)
    maximo=$( [ ${#medidos[@]} -gt 0 ] && printf '%s\n' "${medidos[@]}" | sort -g | tail -1)
    lista="${medidos[*]}"

    {
        echo "****************************************************************"
        echo "RESUMO: \$ $prompt   ($repeticoes execucoes, $falhas com erro)"
        [ -n "$lista" ] && echo "  tempos medidos (s): $lista"
        [ -n "$med_medido" ] && echo "  MEDIANA tempo medido: $med_medido s   (min $minimo | max $maximo)"
        echo "  mediana tempo total do processo: $med_total s"
        echo "****************************************************************"
        echo
    } >> "$log"

    echo "$fase,$n,$programa,$threads,$repeticoes,${med_medido:-},${minimo:-},${maximo:-},$med_total,$falhas,${lista// /;}" >> "$RESUMO"
}

# ---------------------------------------------------------------- compilacao
echo "== Compilando (gcc $CFLAGS) =="
BIN="$DIR_TRABALHO/bin"
mkdir -p "$BIN"
cd "$PROJETO" || exit 1
set -e
gcc $CFLAGS -o "$BIN/cod_sequencial" cod_sequencial.c matriz.c
gcc $CFLAGS -o "$BIN/cod_fork" cod_fork.c matriz.c
gcc $CFLAGS -o "$BIN/cod_pthreads" cod_pthreads.c matriz.c -pthread
gcc $CFLAGS -fopenmp -o "$BIN/cod_openmp" cod_openmp.c matriz.c
gcc $CFLAGS -o "$BIN/comp_sequencial" comp_sequencial.c comparador.c matriz.c
gcc $CFLAGS -o "$BIN/comp_fork" comp_fork.c comparador.c matriz.c
gcc $CFLAGS -pthread -o "$BIN/comp_pthreads" comp_pthreads.c comparador.c matriz.c
gcc $CFLAGS -fopenmp -o "$BIN/comp_openmp" comp_openmp.c comparador.c matriz.c
set +e

# ------------------------------------------------- fase 1: multiplicacoes
for n in "${TAMANHOS[@]}"; do
    pasta="$DIR_TRABALHO/N$n"
    mkdir -p "$pasta"
    cd "$pasta" || exit 1
    log="$DIR_LOGS/testes-matriz-$(rotulo "$n").txt"
    cabecalho "$log" "Multiplicacao de matrizes" "$n"

    echo "== N=$n: gerando matrizes =="
    executar "$log" geracao "$n" gerar_matriz - 1 "echo $n | python3 gerar_matriz.py" \
        bash -c "echo $n | python3 '$PROJETO/gerar_matriz.py'"

    echo "== N=$n: multiplicacoes =="
    executar "$log" multiplicacao "$n" cod_sequencial 1 "$REPETICOES" "./cod_sequencial" "$BIN/cod_sequencial"
    for metodo in "${METODOS[@]}"; do
        for t in "${THREADS[@]}"; do
            executar "$log" multiplicacao "$n" "cod_$metodo" "$t" "$REPETICOES" "./cod_$metodo $t" "$BIN/cod_$metodo" "$t"
        done
    done
done

# ------------------------------------------------- fase 2: comparadores
# Comparam os resultado_*.csv que ficaram na pasta de cada N, ou seja, os da
# ultima execucao de cada metodo (16 threads, ultima repeticao).
for n in "${TAMANHOS[@]}"; do
    cd "$DIR_TRABALHO/N$n" || exit 1
    log="$DIR_LOGS/testes-comparador-$(rotulo "$n").txt"
    cabecalho "$log" "Comparadores (resultado_sequencial.csv vs fork/pthreads/openmp da ultima execucao, 16 threads)" "$n"

    echo "== N=$n: comparadores =="
    executar "$log" comparacao "$n" comp_sequencial 1 "$REPETICOES" "./comp_sequencial" "$BIN/comp_sequencial"
    for metodo in "${METODOS[@]}"; do
        for t in "${THREADS[@]}"; do
            executar "$log" comparacao "$n" "comp_$metodo" "$t" "$REPETICOES" "./comp_$metodo $t" "$BIN/comp_$metodo" "$t"
        done
    done
done

echo
echo "Pronto. Logs em: $DIR_LOGS"
echo "Matrizes/resultados em: $DIR_TRABALHO (podem ser apagados: rm -rf $DIR_TRABALHO)"
