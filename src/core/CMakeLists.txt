# src/core/CMakeLists.txt
#
# Camada núcleo (GODS_LAWS.md L-19): tipos e valores puros (hoje,
# versão em runtime) que não conhecem o sistema operacional. Nenhum
# arquivo aqui inclui header de SO ou de camada acima — o gate em
# tests/tools/check_layers.sh reprova a violação em vez de confiar na
# disciplina de quem escreve.
#
# Assunto único: os fontes desta camada. Propriedades do alvo `glintfx`
# continuam em src/CMakeLists.txt — este arquivo só soma fontes a um
# alvo já criado no diretório pai.

target_sources(glintfx PRIVATE version.cpp)
