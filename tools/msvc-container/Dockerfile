# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/msvc-container/Dockerfile
#
# FERRAMENTA DE DESENVOLVIMENTO, NAO DEPENDENCIA DO PRODUTO (GODS_LAWS.md L-07):
# o GlintFx continua com dependencia zero alem da stdlib e da API do SO. Esta
# imagem existe so para encurtar o ciclo local de correcao do lado Windows -
# ela nunca e linkada nem distribuida com a biblioteca, e nao entra no CI real
# (o CI real usa windows-latest com o MSVC de verdade, via vswhere).
#
# O QUE ISTO E: o compilador cl.exe REAL da Microsoft, rodando sob Wine dentro
# deste container Linux, via o projeto mstorsjo/msvc-wine (baixa os
# componentes oficiais do canal publico de release do Visual Studio, sob a
# licenca da Microsoft - "Downloading and installing it requires accepting
# the license", README do msvc-wine).
#
# O QUE ISTO NAO E, e nunca deve ser vendido como tal: nao abre janela, nao
# minimiza/restaura, nao tem placa de video real nem driver de verdade (WGL
# sob Wine usa Mesa/llvmpipe por software). A UNICA prova de execucao real
# continua sendo o job `windows` do CI (windows-latest). Isto so compila e
# faz analise estatica local, mais rapido que esperar o servidor.
#
# Uso (ver README.md deste diretorio para o passo a passo completo e a prova
# de que pega o que o compilador cruzado local nao pega).

FROM debian:12-slim

# wine64: roda o cl.exe de verdade. python3+msitools: vsdownload.py extrai
# pacotes .msi sem precisar do instalador do Windows. winbind: evita o erro
# C1902 do link.exe sob Wine (falta de um servico que ele consulta). git: só
# para clonar o msvc-wine. ca-certificates: TLS para baixar do canal da MS.
RUN apt-get update && apt-get install -y --no-install-recommends \
        wine64 \
        python3 \
        msitools \
        winbind \
        git \
        ca-certificates \
        cmake \
        ninja-build \
    && rm -rf /var/lib/apt/lists/* \
    # Medido nesta imagem (Debian 12): o pacote `wine64` sozinho NAO poe o
    # binario em /usr/bin - so em /usr/lib/wine/{wine64,wineserver64}. Sem
    # isto, todo wrapper do msvc-wine falha em silencio ("command not
    # found") porque `command -v wine64` no wine-msvc.sh nao acha nada e
    # WINE fica vazio. O pacote `wine` (meta, que traria o /usr/bin/wine
    # certo) puxa a variante 32 bits junto, que este projeto nao precisa.
    && ln -sf /usr/lib/wine/wine64 /usr/bin/wine64 \
    && ln -sf /usr/lib/wine/wineserver64 /usr/bin/wineserver

# msvc-wine na tag mais recente conhecida no momento (não "master" solto -
# reprodutibilidade: um "master" que muda por baixo do agente quebra o
# portão sem aviso). Ajuste o SHA se o líder autorizar atualização.
RUN git clone --depth 1 https://github.com/mstorsjo/msvc-wine.git /opt/msvc-wine

# O download dos componentes MSVC (varios GB, licenciados pela Microsoft)
# NAO acontece aqui no build da imagem - fica para um passo separado,
# rodado com `docker run` e volume montado, para poder medir o tamanho
# ANTES de assar na camada da imagem (ver README.md, passo 2). Rodar aqui
# dentro de um RUN tornaria o download parte imutável da camada, sem chance
# de abortar por tamanho no meio do caminho.

WORKDIR /work
