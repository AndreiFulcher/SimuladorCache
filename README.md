# Simulador de Memória Cache

Este repositório contém o código-fonte, executável, arquivos de entrada e resultados utilizados no trabalho **Análise dos Impactos na Memória Cache**.

O projeto consiste em um simulador de memória cache associativa por conjunto, desenvolvido em linguagem C, com parâmetros configuráveis para análise de desempenho em diferentes configurações de cache.

## Estrutura do Repositório

```text
SimuladorCache/
├── codigo/
│   ├── simuladorCache.c
│   └── simuladorCache.exe
│
├── entradas/
│   ├── oficial.cache
│   └── teste.cache
│
├── scripts/
│   └── rodar_experimentos_cache.py
│
├── resultados_cache/
│   ├── csv/
│   ├── graficos/
│   └── txt/
│
└── README.md
```

## Arquivos Principais

* `codigo/simuladorCache.c`: código-fonte do simulador.
* `codigo/simuladorCache.exe`: versão executável do simulador para Windows.
* `entradas/oficial.cache`: arquivo de entrada utilizado nos experimentos do relatório.
* `entradas/teste.cache`: arquivo de entrada menor para testes rápidos.
* `scripts/rodar_experimentos_cache.py`: script utilizado para executar os experimentos e gerar CSVs e gráficos.
* `resultados_cache/csv/`: tabelas geradas a partir das simulações.
* `resultados_cache/graficos/`: gráficos utilizados no relatório.
* `resultados_cache/txt/`: saídas brutas do simulador.

## Compilação

Para compilar o simulador no Windows, a partir da raiz do projeto, utilize:

```powershell
gcc .\codigo\simuladorCache.c -o .\codigo\simuladorCache.exe
```

No Linux, utilize:

```bash
gcc ./codigo/simuladorCache.c -o ./codigo/simuladorCache
```

## Execução Manual

O executável é um programa de terminal/console. Portanto, ele não abre uma interface gráfica ao ser executado com dois cliques. Para executar corretamente, é necessário informar os parâmetros pela linha de comando.

Formato geral de execução no Windows:

```powershell
.\codigo\simuladorCache.exe <politica_escrita> <tam_linha> <num_linhas> <associatividade> <hit_time> <politica_substituicao> <tempo_mp> <arquivo_entrada>
```

Exemplo com write-through:

```powershell
.\codigo\simuladorCache.exe 0 128 64 4 4 LRU 60 .\entradas\oficial.cache
```

Exemplo com write-back:

```powershell
.\codigo\simuladorCache.exe 1 128 64 4 4 LRU 60 .\entradas\oficial.cache
```

No Linux, a execução pode ser feita assim:

```bash
./codigo/simuladorCache 0 128 64 4 4 LRU 60 ./entradas/oficial.cache
```

## Parâmetros

| Parâmetro               | Descrição                                               |
| ----------------------- | ------------------------------------------------------- |
| `politica_escrita`      | `0` para write-through ou `1` para write-back           |
| `tam_linha`             | tamanho da linha/bloco da cache em bytes                |
| `num_linhas`            | número total de linhas da cache                         |
| `associatividade`       | número de linhas por conjunto                           |
| `hit_time`              | tempo de acerto na cache, em ns                         |
| `politica_substituicao` | `LRU`, `ALEATORIA` ou `RANDOM`                          |
| `tempo_mp`              | tempo de acesso à memória principal, em ns              |
| `arquivo_entrada`       | arquivo com os endereços e operações de leitura/escrita |

Também é possível informar tempos separados para leitura e escrita da memória principal:

```powershell
.\codigo\simuladorCache.exe 0 128 64 4 4 LRU 60 60 .\entradas\oficial.cache
```

Nesse formato, o primeiro `60` representa o tempo de leitura da memória principal, e o segundo `60` representa o tempo de escrita da memória principal.

## Exemplo de Saída

O simulador apresenta como saída:

* parâmetros de entrada;
* total de endereços do arquivo;
* total de leituras e escritas;
* leituras e escritas realizadas na memória principal;
* taxa de acerto de leitura, escrita e global;
* tempo médio de acesso à memória.

O tempo médio de acesso é calculado pela fórmula:

```text
t = tempo_cache + (1 - taxa_de_acerto) × tempo_memória
```

## Execução dos Experimentos

Para executar automaticamente todos os experimentos utilizados no relatório, utilize:

```powershell
python .\scripts\rodar_experimentos_cache.py .\codigo\simuladorCache.exe .\entradas\oficial.cache
```

O script gera os resultados em:

```text
resultados_cache/
├── txt/
├── csv/
└── graficos/
```

## Experimentos Realizados

Foram realizados experimentos para analisar:

1. Impacto do tamanho da cache;
2. Impacto do tamanho do bloco;
3. Impacto da associatividade;
4. Impacto da política de substituição;
5. Largura de banda da memória com write-through e write-back.

## Observações

Na política **write-through**, o simulador utiliza **write-non-allocate**. Assim, em uma falha de escrita, o bloco não é carregado para a cache e a escrita é feita diretamente na memória principal.

Na política **write-back**, o simulador utiliza **write-allocate**. Assim, em uma falha de escrita, o bloco é carregado para a cache, a linha é marcada como `dirty` e a escrita na memória principal é adiada até a substituição da linha ou até o flush final.

Caso o programa seja executado sem parâmetros, ele apenas exibe a mensagem de uso e encerra, pois os parâmetros da simulação são obrigatórios.

## Autor

Andrei Fulcher Ribeiro
