/*
 * Simulador de memoria cache associativa por conjunto.
 *
 * Características do Projeto:
 *
 * Formatos de uso aceitos:
 *   1) Um unico tempo para memoria principal:
 *      ./simula_cache 0 128 64 4 4 LRU 60 oficial.cache
 *
 *   2) Tempo de leitura e escrita separados para memoria principal:
 *      ./simula_cache 0 128 64 4 4 LRU 60 60 oficial.cache
 *
 * Parametros:
 *   politica_escrita : 0 = write-through, 1 = write-back
 *   tam_linha        : tamanho da linha/bloco em bytes, potencia de 2
 *   num_linhas       : numero total de linhas da cache, potencia de 2
 *   associatividade  : numero de linhas por conjunto, potencia de 2
 *   hit_time         : tempo de acesso em caso de acerto, em ns
 *   politica_subst   : LRU ou ALEATORIA
 *   tempo_leitura_mp : tempo de leitura da memoria principal, em ns
 *   tempo_escrita_mp : tempo de escrita da memoria principal, em ns
 *   arquivo.cache    : arquivo de entrada com endereco hexadecimal e operacao R/W
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define WRITE_THROUGH 0
#define WRITE_BACK    1

#define REPL_LRU      0
#define REPL_RANDOM   1

typedef struct {
    int valido;
    uint32_t rotulo;
    int dirty;
    unsigned long long ultimo_uso;
} LinhaCache;

typedef struct {
    int politica_escrita;
    unsigned int tamanho_linha;
    unsigned int numero_linhas;
    unsigned int associatividade;
    double hit_time;
    int politica_substituicao;
    double tempo_leitura_mp;
    double tempo_escrita_mp;
    const char *arquivo_entrada;
} Config;

typedef struct {
    unsigned long long total_acessos;
    unsigned long long total_leituras;
    unsigned long long total_escritas;

    unsigned long long hits_leitura;
    unsigned long long misses_leitura;
    unsigned long long hits_escrita;
    unsigned long long misses_escrita;

    unsigned long long leituras_mp;
    unsigned long long escritas_mp;
} Estatisticas;

static int eh_potencia_de_2(unsigned int n) {
    return n != 0 && (n & (n - 1)) == 0;
}

static int compara_texto(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void imprime_uso(const char *programa) {
    printf("Uso:\n");
    printf("  %s <politica_escrita> <tam_linha> <num_linhas> <associatividade> <hit_time> <LRU|ALEATORIA|RANDOM> <tempo_mp> <arquivo.cache>\n", programa);
    printf("ou\n");
    printf("  %s <politica_escrita> <tam_linha> <num_linhas> <associatividade> <hit_time> <LRU|ALEATORIA|RANDOM> <tempo_leitura_mp> <tempo_escrita_mp> <arquivo.cache>\n\n", programa);
    printf("politica_escrita: 0 = write-through, 1 = write-back\n");
}

static int ler_config(int argc, char **argv, Config *cfg) {
    if (argc != 9 && argc != 10) {
        imprime_uso(argv[0]);
        return 0;
    }

    cfg->politica_escrita = atoi(argv[1]);
    cfg->tamanho_linha = (unsigned int)strtoul(argv[2], NULL, 10);
    cfg->numero_linhas = (unsigned int)strtoul(argv[3], NULL, 10);
    cfg->associatividade = (unsigned int)strtoul(argv[4], NULL, 10);
    cfg->hit_time = atof(argv[5]);

    if (compara_texto(argv[6], "LRU")) {
        cfg->politica_substituicao = REPL_LRU;
    } else if (compara_texto(argv[6], "ALEATORIA") || compara_texto(argv[6], "RANDOM")) {
        cfg->politica_substituicao = REPL_RANDOM;
    } else {
        printf("Erro: politica de substituicao deve ser LRU, ALEATORIA ou RANDOM.\n");
        return 0;
    }

    cfg->tempo_leitura_mp = atof(argv[7]);
    if (argc == 9) {
        cfg->tempo_escrita_mp = cfg->tempo_leitura_mp;
        cfg->arquivo_entrada = argv[8];
    } else {
        cfg->tempo_escrita_mp = atof(argv[8]);
        cfg->arquivo_entrada = argv[9];
    }

    if (cfg->politica_escrita != WRITE_THROUGH && cfg->politica_escrita != WRITE_BACK) {
        printf("Erro: politica de escrita deve ser 0 ou 1.\n");
        return 0;
    }

    if (!eh_potencia_de_2(cfg->tamanho_linha)) {
        printf("Erro: tamanho da linha deve ser potencia de 2.\n");
        return 0;
    }

    if (!eh_potencia_de_2(cfg->numero_linhas)) {
        printf("Erro: numero de linhas deve ser potencia de 2.\n");
        return 0;
    }

    if (!eh_potencia_de_2(cfg->associatividade)) {
        printf("Erro: associatividade deve ser potencia de 2.\n");
        return 0;
    }

    if (cfg->associatividade < 1 || cfg->associatividade > cfg->numero_linhas) {
        printf("Erro: associatividade deve estar entre 1 e o numero de linhas.\n");
        return 0;
    }

    if (cfg->numero_linhas % cfg->associatividade != 0) {
        printf("Erro: numero de linhas deve ser divisivel pela associatividade.\n");
        return 0;
    }

    if (cfg->hit_time < 0.0 || cfg->tempo_leitura_mp < 0.0 || cfg->tempo_escrita_mp < 0.0) {
        printf("Erro: tempos de acesso nao podem ser negativos.\n");
        return 0;
    }

    return 1;
}

static int encontrar_linha(LinhaCache *cache, unsigned int inicio, unsigned int associatividade, uint32_t rotulo) {
    for (unsigned int i = 0; i < associatividade; i++) {
        unsigned int pos = inicio + i;
        if (cache[pos].valido && cache[pos].rotulo == rotulo) {
            return (int)pos;
        }
    }
    return -1;
}

static int escolher_linha(LinhaCache *cache, unsigned int inicio, unsigned int associatividade, int politica_substituicao) {
    for (unsigned int i = 0; i < associatividade; i++) {
        unsigned int pos = inicio + i;
        if (!cache[pos].valido) {
            return (int)pos;
        }
    }

    if (politica_substituicao == REPL_RANDOM) {
        return (int)(inicio + ((unsigned int)rand() % associatividade));
    }

    unsigned int vitima = inicio;
    unsigned long long menor_uso = cache[inicio].ultimo_uso;

    for (unsigned int i = 1; i < associatividade; i++) {
        unsigned int pos = inicio + i;
        if (cache[pos].ultimo_uso < menor_uso) {
            menor_uso = cache[pos].ultimo_uso;
            vitima = pos;
        }
    }

    return (int)vitima;
}

static void substituir_linha(LinhaCache *cache,
                             int pos,
                             uint32_t rotulo,
                             int dirty,
                             unsigned long long tempo,
                             const Config *cfg,
                             Estatisticas *est) {
    if (cfg->politica_escrita == WRITE_BACK && cache[pos].valido && cache[pos].dirty) {
        est->escritas_mp++;
    }

    cache[pos].valido = 1;
    cache[pos].rotulo = rotulo;
    cache[pos].dirty = dirty;
    cache[pos].ultimo_uso = tempo;
}

static void simular_acesso(LinhaCache *cache,
                           const Config *cfg,
                           Estatisticas *est,
                           uint32_t endereco,
                           char operacao,
                           unsigned long long tempo) {
    unsigned int numero_conjuntos = cfg->numero_linhas / cfg->associatividade;

    /*
     * Mapeamento associativo por conjunto:
     * bloco_mp = endereco / tamanho_linha
     * conjunto = bloco_mp % numero_conjuntos
     * rotulo   = bloco_mp / numero_conjuntos
     */
    uint32_t bloco_mp = endereco / cfg->tamanho_linha;
    unsigned int conjunto = (unsigned int)(bloco_mp % numero_conjuntos);
    uint32_t rotulo = bloco_mp / numero_conjuntos;
    unsigned int inicio = conjunto * cfg->associatividade;

    int pos = encontrar_linha(cache, inicio, cfg->associatividade, rotulo);
    int hit = pos >= 0;

    est->total_acessos++;
    if (operacao == 'R') {
        est->total_leituras++;
    } else {
        est->total_escritas++;
    }

    if (hit) {
        cache[pos].ultimo_uso = tempo;

        if (operacao == 'R') {
            est->hits_leitura++;
        } else {
            est->hits_escrita++;

            if (cfg->politica_escrita == WRITE_THROUGH) {
                est->escritas_mp++;
            } else {
                cache[pos].dirty = 1;
            }
        }
        return;
    }

    if (operacao == 'R') {
        est->misses_leitura++;
        est->leituras_mp++;

        int vitima = escolher_linha(cache, inicio, cfg->associatividade, cfg->politica_substituicao);
        substituir_linha(cache, vitima, rotulo, 0, tempo, cfg, est);
    } else {
        est->misses_escrita++;

        if (cfg->politica_escrita == WRITE_THROUGH) {
            // Write-through com write-non-allocate: escreve direto na MP e nao carrega na cache.
            est->escritas_mp++;
        } else {
            // Write-back com write-allocate: carrega o bloco e marca dirty.
            est->leituras_mp++;

            int vitima = escolher_linha(cache, inicio, cfg->associatividade, cfg->politica_substituicao);
            substituir_linha(cache, vitima, rotulo, 1, tempo, cfg, est);
        }
    }
}

static int simular(const Config *cfg, Estatisticas *est) {
    FILE *arquivo = fopen(cfg->arquivo_entrada, "r");
    if (arquivo == NULL) {
        printf("Erro: nao foi possivel abrir o arquivo '%s'.\n", cfg->arquivo_entrada);
        return 0;
    }

    LinhaCache *cache = (LinhaCache *)calloc(cfg->numero_linhas, sizeof(LinhaCache));
    if (cache == NULL) {
        printf("Erro: nao foi possivel alocar a memoria da cache.\n");
        fclose(arquivo);
        return 0;
    }

    char endereco_txt[32];
    char operacao;
    unsigned long long tempo = 0;

    while (fscanf(arquivo, "%31s %c", endereco_txt, &operacao) == 2) {
        if (operacao != 'R' && operacao != 'W') {
            printf("Aviso: operacao invalida ignorada: %c\n", operacao);
            continue;
        }

        uint32_t endereco = (uint32_t)strtoul(endereco_txt, NULL, 16);
        tempo++;
        simular_acesso(cache, cfg, est, endereco, operacao, tempo);
    }

    // Atualiza a memoria principal com todas as linhas alteradas no write-back.
    if (cfg->politica_escrita == WRITE_BACK) {
        for (unsigned int i = 0; i < cfg->numero_linhas; i++) {
            if (cache[i].valido && cache[i].dirty) {
                est->escritas_mp++;
                cache[i].dirty = 0;
            }
        }
    }

    free(cache);
    fclose(arquivo);
    return 1;
}

static double porcentagem(unsigned long long parte, unsigned long long total) {
    if (total == 0) {
        return 0.0;
    }
    return ((double)parte * 100.0) / (double)total;
}

static void imprimir_saida(const Config *cfg, const Estatisticas *est) {
    unsigned int numero_conjuntos = cfg->numero_linhas / cfg->associatividade;
    unsigned int tamanho_total_cache = cfg->tamanho_linha * cfg->numero_linhas;

    unsigned long long total_hits = est->hits_leitura + est->hits_escrita;
    unsigned long long total_misses = est->misses_leitura + est->misses_escrita;

    double hit_rate_leitura = porcentagem(est->hits_leitura, est->total_leituras);
    double hit_rate_escrita = porcentagem(est->hits_escrita, est->total_escritas);
    double hit_rate_global = porcentagem(total_hits, est->total_acessos);
    double miss_rate_global = 100.0 - hit_rate_global;

    // AMAT convencional: hit_time + miss_rate * penalidade_de_miss.
    double tempo_medio_amat = cfg->hit_time + ((miss_rate_global / 100.0) * cfg->tempo_leitura_mp);

    // Valor adicional util para analisar trafego real gerado na MP.
    double tempo_total_efetivo =
        ((double)est->total_acessos * cfg->hit_time) +
        ((double)est->leituras_mp * cfg->tempo_leitura_mp) +
        ((double)est->escritas_mp * cfg->tempo_escrita_mp);

    double tempo_medio_efetivo = 0.0;
    if (est->total_acessos > 0) {
        tempo_medio_efetivo = tempo_total_efetivo / (double)est->total_acessos;
    }

    printf("=======================================================\n");
    printf("           SIMULADOR DE MEMORIA CACHE                  \n");
    printf("=======================================================\n\n");

    printf("--- Parametros de Entrada ---\n");
    printf("Arquivo de entrada              : %s\n", cfg->arquivo_entrada);
    printf("Politica de escrita             : %s (%d)\n",
           cfg->politica_escrita == WRITE_THROUGH ? "write-through" : "write-back",
           cfg->politica_escrita);
    printf("Tamanho da linha                : %u bytes\n", cfg->tamanho_linha);
    printf("Numero de linhas                : %u\n", cfg->numero_linhas);
    printf("Associatividade                 : %u linhas por conjunto\n", cfg->associatividade);
    printf("Numero de conjuntos             : %u\n", numero_conjuntos);
    printf("Tamanho total da cache          : %u bytes\n", tamanho_total_cache);
    printf("Hit time                        : %.4f ns\n", cfg->hit_time);
    printf("Politica de substituicao        : %s\n",
           cfg->politica_substituicao == REPL_LRU ? "LRU" : "Aleatoria");
    printf("Tempo de leitura da MP          : %.4f ns\n", cfg->tempo_leitura_mp);
    printf("Tempo de escrita da MP          : %.4f ns\n\n", cfg->tempo_escrita_mp);

    printf("--- Totais do Arquivo de Entrada ---\n");
    printf("Total de enderecos              : %llu\n", est->total_acessos);
    printf("Total de leituras               : %llu\n", est->total_leituras);
    printf("Total de escritas               : %llu\n\n", est->total_escritas);

    printf("--- Acessos a Memoria Principal ---\n");
    printf("Leituras na MP                  : %llu\n", est->leituras_mp);
    printf("Escritas na MP                  : %llu\n", est->escritas_mp);
    printf("Total de acessos na MP          : %llu\n\n", est->leituras_mp + est->escritas_mp);

    printf("--- Taxa de Acerto ---\n");
    printf("Leitura                         : %.4f%% (%llu acertos / %llu leituras)\n",
           hit_rate_leitura, est->hits_leitura, est->total_leituras);
    printf("Escrita                         : %.4f%% (%llu acertos / %llu escritas)\n",
           hit_rate_escrita, est->hits_escrita, est->total_escritas);
    printf("Global                          : %.4f%% (%llu acertos / %llu acessos)\n",
           hit_rate_global, total_hits, est->total_acessos);
    printf("Total de falhas                 : %llu\n\n", total_misses);

    printf("--- Tempo Medio de Acesso ---\n");
    printf("Tempo medio de acesso (AMAT)    : %.4f ns\n", tempo_medio_amat);

    printf("=======================================================\n");
}

int main(int argc, char **argv) {
    Config cfg;
    Estatisticas est;

    memset(&cfg, 0, sizeof(Config));
    memset(&est, 0, sizeof(Estatisticas));

    if (!ler_config(argc, argv, &cfg)) {
        return 1;
    }

    srand(42);

    if (!simular(&cfg, &est)) {
        return 1;
    }

    imprimir_saida(&cfg, &est);
    return 0;
}
