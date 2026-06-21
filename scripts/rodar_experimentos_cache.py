import csv
import re
import subprocess
import sys
from pathlib import Path

import matplotlib.pyplot as plt


def extrair_numero(texto, padrao, tipo=float, padrao_valor=0):
    match = re.search(padrao, texto, re.IGNORECASE)
    if not match:
        return padrao_valor
    return tipo(match.group(1).replace(",", "."))


def extrair_resultados(saida):
    return {
        "leituras_mp": extrair_numero(saida, r"Leituras na MP\s*:\s*(\d+)", int),
        "escritas_mp": extrair_numero(saida, r"Escritas na MP\s*:\s*(\d+)", int),
        "total_mp": extrair_numero(saida, r"Total de acessos na MP\s*:\s*(\d+)", int),
        "hit_global": extrair_numero(saida, r"Global\s*:\s*([0-9.,]+)%", float),
        "amat": extrair_numero(saida, r"Tempo medio de acesso \(AMAT\)\s*:\s*([0-9.,]+)", float),
    }


def executar(simulador, args, nome_txt):
    comando = [simulador] + [str(arg) for arg in args]

    processo = subprocess.run(
        comando,
        capture_output=True,
        text=True
    )

    saida = processo.stdout + processo.stderr
    nome_txt.write_text(saida, encoding="utf-8")

    if processo.returncode != 0:
        print(saida)
        raise RuntimeError("Erro ao executar: " + " ".join(comando))

    return extrair_resultados(saida)


def salvar_csv(caminho, linhas, campos):
    with open(caminho, "w", newline="", encoding="utf-8-sig") as arquivo:
        writer = csv.DictWriter(
            arquivo,
            fieldnames=campos,
            delimiter=";",
            extrasaction="ignore"
        )
        writer.writeheader()
        for linha in linhas:
            writer.writerow(linha)


def gerar_grafico(x, y, titulo, xlabel, ylabel, caminho):
    plt.figure()
    plt.plot(x, y, marker="o")
    plt.title(titulo)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(caminho, dpi=200)
    plt.close()


def gerar_grafico_duas_curvas(linhas, caminho):
    plt.figure()

    for politica in ["LRU", "ALEATORIA"]:
        dados = [l for l in linhas if l["politica"] == politica]
        x = [l["tamanho_cache_bytes"] for l in dados]
        y = [l["hit_global"] for l in dados]
        plt.plot(x, y, marker="o", label=politica)

    plt.title("Taxa de acerto x política de substituição")
    plt.xlabel("Tamanho da cache (bytes)")
    plt.ylabel("Taxa de acerto global (%)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(caminho, dpi=200)
    plt.close()


def adicionar_media(linhas):
    media_leitura = sum(l["leituras_mp"] for l in linhas) / len(linhas)
    media_escrita = sum(l["escritas_mp"] for l in linhas) / len(linhas)
    media_total = sum(l["total_mp"] for l in linhas) / len(linhas)

    linhas.append({
        "cache_kb": "MEDIA",
        "tamanho_bloco": "",
        "associatividade": "",
        "leituras_mp": f"{media_leitura:.4f}",
        "escritas_mp": f"{media_escrita:.4f}",
        "total_mp": f"{media_total:.4f}",
        "hit_global": "",
        "amat": "",
    })


def main():
    if len(sys.argv) != 3:
        print("Uso:")
        print(r"python rodar_experimentos_cache.py .\simuladorCache.exe oficial.cache")
        return

    simulador = sys.argv[1]
    arquivo_cache = sys.argv[2]

    pasta_saida = Path("resultados_cache")
    pasta_txt = pasta_saida / "txt"
    pasta_csv = pasta_saida / "csv"
    pasta_graficos = pasta_saida / "graficos"

    pasta_txt.mkdir(parents=True, exist_ok=True)
    pasta_csv.mkdir(parents=True, exist_ok=True)
    pasta_graficos.mkdir(parents=True, exist_ok=True)

    todos = []

    # =========================================================
    # Impacto do tamanho da cache
    # =========================================================
    exp1 = []
    for numero_linhas in [8, 16, 32, 64, 128, 256, 512, 1024]:
        resultado = executar(
            simulador,
            [0, 128, numero_linhas, 4, 4, "LRU", 60, arquivo_cache],
            pasta_txt / f"exp1_{numero_linhas}.txt"
        )

        linha = {
            "numero_linhas": numero_linhas,
            "tamanho_cache_bytes": numero_linhas * 128,
            **resultado
        }

        exp1.append(linha)
        todos.append({"experimento": "exp1", **linha})

    salvar_csv(
        pasta_csv / "exp1_tamanho_cache.csv",
        exp1,
        ["numero_linhas", "tamanho_cache_bytes", "hit_global", "amat", "leituras_mp", "escritas_mp", "total_mp"]
    )

    gerar_grafico(
        [l["tamanho_cache_bytes"] for l in exp1],
        [l["hit_global"] for l in exp1],
        "Taxa de acerto x tamanho da cache",
        "Tamanho da cache (bytes)",
        "Taxa de acerto global (%)",
        pasta_graficos / "exp1_tamanho_cache.png"
    )

    # =========================================================
    # Impacto do tamanho do bloco
    # Cache fixa em 8 KB = 8192 bytes
    # =========================================================
    exp2 = []
    for tamanho_bloco in [8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096]:
        numero_linhas = 8192 // tamanho_bloco

        resultado = executar(
            simulador,
            [0, tamanho_bloco, numero_linhas, 2, 4, "LRU", 60, arquivo_cache],
            pasta_txt / f"exp2_{tamanho_bloco}.txt"
        )

        linha = {
            "tamanho_bloco": tamanho_bloco,
            "numero_linhas": numero_linhas,
            "tamanho_cache_bytes": 8192,
            **resultado
        }

        exp2.append(linha)
        todos.append({"experimento": "exp2", **linha})

    salvar_csv(
        pasta_csv / "exp2_tamanho_bloco.csv",
        exp2,
        ["tamanho_bloco", "numero_linhas", "tamanho_cache_bytes", "hit_global", "amat", "leituras_mp", "escritas_mp", "total_mp"]
    )

    gerar_grafico(
        [l["tamanho_bloco"] for l in exp2],
        [l["hit_global"] for l in exp2],
        "Taxa de acerto x tamanho do bloco",
        "Tamanho do bloco (bytes)",
        "Taxa de acerto global (%)",
        pasta_graficos / "exp2_tamanho_bloco.png"
    )

    # =========================================================
    # Impacto da associatividade
    # =========================================================
    exp3 = []
    for associatividade in [1, 2, 4, 8, 16, 32, 64]:
        resultado = executar(
            simulador,
            [1, 128, 64, associatividade, 4, "LRU", 60, arquivo_cache],
            pasta_txt / f"exp3_assoc_{associatividade}.txt"
        )

        linha = {
            "associatividade": associatividade,
            "tamanho_cache_bytes": 8192,
            "tamanho_bloco": 128,
            "numero_linhas": 64,
            **resultado
        }

        exp3.append(linha)
        todos.append({"experimento": "exp3", **linha})

    salvar_csv(
        pasta_csv / "exp3_associatividade.csv",
        exp3,
        ["associatividade", "tamanho_cache_bytes", "tamanho_bloco", "numero_linhas", "hit_global", "amat", "leituras_mp", "escritas_mp", "total_mp"]
    )

    gerar_grafico(
        [l["associatividade"] for l in exp3],
        [l["hit_global"] for l in exp3],
        "Taxa de acerto x associatividade",
        "Associatividade",
        "Taxa de acerto global (%)",
        pasta_graficos / "exp3_associatividade.png"
    )

    # =========================================================
    # Impacto da política de substituição
    # =========================================================
    exp4 = []
    for politica in ["LRU", "ALEATORIA"]:
        for numero_linhas in [16, 32, 64, 128, 256, 512, 1024]:
            resultado = executar(
                simulador,
                [0, 128, numero_linhas, 4, 4, politica, 60, arquivo_cache],
                pasta_txt / f"exp4_{politica}_{numero_linhas}.txt"
            )

            linha = {
                "politica": politica,
                "numero_linhas": numero_linhas,
                "tamanho_cache_bytes": numero_linhas * 128,
                **resultado
            }

            exp4.append(linha)
            todos.append({"experimento": "exp4", **linha})

    salvar_csv(
        pasta_csv / "exp4_substituicao.csv",
        exp4,
        ["politica", "numero_linhas", "tamanho_cache_bytes", "hit_global", "amat", "leituras_mp", "escritas_mp", "total_mp"]
    )

    gerar_grafico_duas_curvas(
        exp4,
        pasta_graficos / "exp4_substituicao.png"
    )

    # =========================================================
    # Impacto da largura de banda da memória
    # =========================================================
    exp5_wt = []
    exp5_wb = []

    for politica_escrita, nome_politica, lista in [
        (0, "write-through", exp5_wt),
        (1, "write-back", exp5_wb)
    ]:
        for cache_kb, cache_bytes in [(8, 8192), (16, 16384)]:
            for tamanho_bloco in [64, 128]:
                for associatividade in [2, 4]:
                    numero_linhas = cache_bytes // tamanho_bloco

                    resultado = executar(
                        simulador,
                        [politica_escrita, tamanho_bloco, numero_linhas, associatividade, 4, "LRU", 60, arquivo_cache],
                        pasta_txt / f"exp5_{nome_politica}_{cache_kb}kb_{tamanho_bloco}_assoc{associatividade}.txt"
                    )

                    linha = {
                        "cache_kb": cache_kb,
                        "tamanho_cache_bytes": cache_bytes,
                        "tamanho_bloco": tamanho_bloco,
                        "numero_linhas": numero_linhas,
                        "associatividade": associatividade,
                        **resultado
                    }

                    lista.append(linha)
                    todos.append({"experimento": f"exp5_{nome_politica}", **linha})

    adicionar_media(exp5_wt)
    adicionar_media(exp5_wb)

    salvar_csv(
        pasta_csv / "exp5_largura_banda_write_through.csv",
        exp5_wt,
        ["cache_kb", "tamanho_cache_bytes", "tamanho_bloco", "numero_linhas", "associatividade", "leituras_mp", "escritas_mp", "total_mp", "hit_global", "amat"]
    )

    salvar_csv(
        pasta_csv / "exp5_largura_banda_write_back.csv",
        exp5_wb,
        ["cache_kb", "tamanho_cache_bytes", "tamanho_bloco", "numero_linhas", "associatividade", "leituras_mp", "escritas_mp", "total_mp", "hit_global", "amat"]
    )

    salvar_csv(
        pasta_csv / "todos_resultados.csv",
        todos,
        ["experimento", "numero_linhas", "tamanho_cache_bytes", "tamanho_bloco", "associatividade", "politica", "hit_global", "amat", "leituras_mp", "escritas_mp", "total_mp"]
    )

    print()
    print("Concluido!")
    print(f"Resultados gerados em: {pasta_saida.resolve()}")
    print(f"CSVs: {pasta_csv.resolve()}")
    print(f"Graficos: {pasta_graficos.resolve()}")


if __name__ == "__main__":
    main()