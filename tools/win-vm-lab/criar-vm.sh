#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Cria (mas NAO arranca) a maquina virtual Windows 11 Enterprise LTSC 2024
# do laboratorio isolado do GlintFx. Terceira fronteira de isolamento da
# L-09 (GODS_LAWS.md do projeto): teste que EXECUTA nunca toca a sessao
# viva do lider. Gerencia inteira por linha de comando (virt-install /
# virsh); virt-manager e virt-viewer NAO estao instalados de proposito.
#
# Ordem do lider, 07/09/2026, verbatim: "Faca de maneira que os testes nao
# toquem minha sessao!"
#
# Este script:
#   1. gera a senha da conta local (nao fica hardcoded no autounattend.xml
#      versionado, so no arquivo final descartavel deste laboratorio);
#   2. constroi o CD2 (answer disc): autounattend.xml + drivers virtio
#      minimos exigidos na fase windowsPE (vioscsi, NetKVM);
#   3. cria o disco qcow2 vazio de 64 GiB em /var/tmp (nunca /tmp, que
#      aqui sai da RAM);
#   4. define a VM via virt-install, com --noautoconsole e SEM arrancar
#      (usa --print-xml por default; passe --arrancar-de-verdade para
#      efetivamente definir+ligar, sob autorizacao explicita do lider).
#
# Uso:
#   ./criar-vm.sh                      # so mostra o XML que seria criado
#   ./criar-vm.sh --arrancar-de-verdade  # define e liga a VM de verdade
#
# NUNCA rode --arrancar-de-verdade sem: (a) o slot de 1-trabalho-pesado-
# -por-vez (L-11) estar livre, e (b) autorizacao explicita do lider para
# este passo especifico.

set -eu
set -o pipefail

LAB_DIR="/var/tmp/glintfx-win-lab"
VM_NAME="glintfx-win11-lab"

# Decisao do team-lead, 07/09/2026, medida e nao presumida: qemu:///session,
# nunca qemu:///system. O modo de sistema depende do polkit para autorizar
# quem nao esta no grupo 'libvirt' (nao e o nosso caso), e essa autorizacao
# as vezes expira e reabre um dialogo grafico -- na sessao do lider. O modo
# de sessao nao pede autorizacao nenhuma (confirmado: 'virsh -c
# qemu:///session version' responde sem senha), roda como o usuario comum
# (menos privilegio, nao mais), e nao precisa de nada que so o modo de
# sistema oferece (sem ponte de rede do host, sem repasse de dispositivo,
# sem armazenamento compartilhado). TPM 2.0 emulado e firmware UEFI foram
# confirmados disponiveis em modo de sessao antes de este script existir
# ('virsh -c qemu:///session domcapabilities' lista tpm-crb; OVMF e swtpm
# sao legiveis por qualquer usuario). Fallback para qemu:///system com
# 'sudo -A' fica documentado no RELATORIO.md, e so se usa se o modo de
# sessao falhar na pratica -- o que ainda nao aconteceu.
CONNECT_URI="qemu:///session"
WIN_ISO="${LAB_DIR}/win11-ltsc2024-ptbr-x64-eval.iso"
VIRTIO_ISO="${LAB_DIR}/virtio-win-stable.iso"
ANSWER_DIR="${LAB_DIR}/answer-disc"
ANSWER_ISO="${LAB_DIR}/answer-disc.iso"
DISK_PATH="${LAB_DIR}/${VM_NAME}.qcow2"
DISK_SIZE_GIB=64
MEM_GIB=8
VCPUS=4
SENHA_FILE="${LAB_DIR}/.senha-conta-lab-NAO-VERSIONAR.txt"

PROJETO_RO="/home/petrus/IDrive/Documentos/projetos_claudebrain/Projects/GlintFx"

ARRANCAR=0
if [ "${1:-}" = "--arrancar-de-verdade" ]; then
  ARRANCAR=1
fi

echo "=== 0. Checagens de pre-condicao ==="

for bin in virt-install virsh xmlstarlet xorriso qemu-img; do
  command -v "$bin" >/dev/null 2>&1 || { echo "ERRO: '$bin' nao encontrado. Nada instalado por este script (L-51)." >&2; exit 1; }
done

for f in "$WIN_ISO" "$VIRTIO_ISO"; do
  [ -f "$f" ] || { echo "ERRO: midia esperada nao existe: $f" >&2; exit 1; }
done

if command -v virt-manager >/dev/null 2>&1 || command -v virt-viewer >/dev/null 2>&1; then
  echo "ERRO: virt-manager ou virt-viewer estao instalados. A regra 1 exige gerencia so por linha de comando; este script recusa rodar." >&2
  exit 1
fi

echo "OK: binarios de linha de comando presentes, virt-manager/virt-viewer ausentes, midias no lugar."

echo
echo "=== 1. Senha da conta local do laboratorio ==="

if [ ! -f "$SENHA_FILE" ]; then
  # 24 bytes aleatorios em base64 -> string ASCII, sem espaco, facil de
  # colar num terminal serial. Fica SO neste arquivo, modo 600, dentro do
  # /var/tmp deste laboratorio -- nunca dentro do autounattend.xml
  # versionavel nem em log.
  SENHA="$(openssl rand -base64 24 | tr -d '/+=' | cut -c1-24)"
  umask 077
  printf '%s\n' "$SENHA" >"$SENHA_FILE"
  echo "Senha nova gerada e gravada em: $SENHA_FILE (modo 600)."
else
  SENHA="$(cat "$SENHA_FILE")"
  echo "Reaproveitando senha ja gravada em: $SENHA_FILE."
fi

echo
echo "=== 2. Montando o CD2 (answer disc: autounattend.xml + drivers virtio) ==="

rm -rf "$ANSWER_DIR"
mkdir -p "$ANSWER_DIR"

sed \
  -e "s/__ADMIN_PASSWORD__/${SENHA}/g" \
  -e "s/__USER_PASSWORD__/${SENHA}/g" \
  "${LAB_DIR}/autounattend.xml" >"${ANSWER_DIR}/autounattend.xml"

echo "  extraindo drivers virtio minimos da fase windowsPE (vioscsi, NetKVM)..."
# 7z preserva o caminho INTEIRO do arquivo dentro da ISO a partir do
# diretorio de saida -- pedir "vioscsi/w11/amd64" com -o"${ANSWER_DIR}"
# (nunca -o"${ANSWER_DIR}/vioscsi") e o que produz
# "${ANSWER_DIR}/vioscsi/w11/amd64/...", que e exatamente o caminho que o
# autounattend.xml espera em E:\vioscsi\w11\amd64. Medido em 07/09/2026:
# a primeira versao deste script usava -o"${ANSWER_DIR}/vioscsi" e
# duplicava o nivel, produzindo "vioscsi/vioscsi/w11/amd64" -- silencioso,
# so aparecia ao listar o ISO final.
7z x -y -o"${ANSWER_DIR}" "$VIRTIO_ISO" "vioscsi/w11/amd64" >/dev/null
7z x -y -o"${ANSWER_DIR}" "$VIRTIO_ISO" "NetKVM/w11/amd64" >/dev/null

N_VIOSCSI=$(find "${ANSWER_DIR}/vioscsi" -type f 2>/dev/null | wc -l)
N_NETKVM=$(find "${ANSWER_DIR}/NetKVM" -type f 2>/dev/null | wc -l)
echo "  arquivos extraidos: vioscsi=${N_VIOSCSI} NetKVM=${N_NETKVM}"
if [ "$N_VIOSCSI" -eq 0 ] || [ "$N_NETKVM" -eq 0 ]; then
  echo "ERRO: extracao de driver falhou (contagem zero). Nao prosseguindo com CD2 incompleto." >&2
  exit 1
fi

# Nao basta contar arquivo em algum lugar (L-40): confere que o .inf esta
# EXATAMENTE no caminho que o autounattend.xml vai pedir a letra E:\...\.
for esperado in \
  "${ANSWER_DIR}/vioscsi/w11/amd64/vioscsi.inf" \
  "${ANSWER_DIR}/NetKVM/w11/amd64/netkvm.inf"
do
  if [ ! -f "$esperado" ]; then
    echo "ERRO: caminho esperado pelo autounattend.xml nao existe apos extracao: $esperado" >&2
    echo "       (a extracao pode ter ficado aninhada um nivel a mais -- confira com 7z l)" >&2
    exit 1
  fi
done
echo "  caminhos exatos esperados pelo DriverPaths confirmados presentes."

xorriso -as genisoimage -o "$ANSWER_ISO" -V "AUTOUNATTEND" -J -R -iso-level 3 "$ANSWER_DIR" >/dev/null

echo "  CD2 pronto: $ANSWER_ISO ($(stat -c%s "$ANSWER_ISO") bytes)"

echo
echo "=== 3. Disco da VM (qcow2 ${DISK_SIZE_GIB}GiB, sparse, em /var/tmp) ==="

if [ -f "$DISK_PATH" ]; then
  echo "  disco ja existe, mantendo: $DISK_PATH"
else
  qemu-img create -f qcow2 "$DISK_PATH" "${DISK_SIZE_GIB}G" >/dev/null
  echo "  disco criado: $DISK_PATH"
fi

echo
echo "=== 4. Definicao da VM (virt-install) ==="
echo "  arrancar de verdade: $([ "$ARRANCAR" -eq 1 ] && echo SIM || echo 'NAO -- so --print-xml')"

# --- as cinco regras da secao 'maquina virtual' da L-09, aplicadas aqui ---
#
# Regra 1 (linha de comando, nunca grafica): nenhum --autoconsole grafico,
#   --noautoconsole sempre. Checado acima que virt-manager/virt-viewer nao
#   existem.
#
# Regra 2 (sem tela ligada a sessao do lider): --graphics vnc,listen=127.0.0.1
#   (nunca 0.0.0.0, nunca sdl/desktop que abriria janela local), mais
#   --autoconsole none. O console de texto e a serial (isa-serial COM1),
#   que qualquer teste de janela real usara via VNC preso a loopback,
#   aberto manualmente por tunel SSH quando alguem quiser olhar -- nunca
#   sozinho.
#
# Regra 3 (nenhum dispositivo de entrada real repassado): nenhum --hostdev,
#   nenhum --input type=evdev, nenhum --usb-redir. O unico --input e o
#   tablet USB VIRTUAL padrao do proprio QEMU (nao existe do lado do
#   hospedeiro).
#
# Regra 4 (ponte de arquivo estreita, somente leitura, resultado por
#   arquivo): --filesystem aponta so para a raiz do projeto GlintFx, modo
#   'mapped', com --readonly. Sem --filesystem adicional, sem clipboard
#   (nenhum --channel spicevmc, nenhum --graphics ...,clipboard.copypaste=).
#
# Regra 5 (provar isolamento antes de interagir): feito por
#   provar-isolamento.sh, DEPOIS deste script definir a VM, lendo
#   'virsh dumpxml --inactive' -- nunca por impressao de que "esta tudo
#   certo".

VIRT_INSTALL_ARGS=(
  --connect "$CONNECT_URI"
  --name "$VM_NAME"
  --memory "$((MEM_GIB * 1024))"
  --vcpus "$VCPUS"
  --cpu host-passthrough
  --os-variant win11
  --machine q35

  # UEFI + Secure Boot (chaves da Microsoft ja inscritas) + SMM (exigido
  # por Secure Boot em q35). TPM 2.0 emulado por swtpm.
  --boot "firmware=efi,firmware.feature0.name=secure-boot,firmware.feature0.enabled=yes"
  --features "smm.state=on"
  --tpm "emulator,model=tpm-crb,version=2.0"

  # Ordem de arranque por DISPOSITIVO, ancorada no alvo (sdb/sda), nunca no
  # rotulo do menu de arranque (a numeracao QM0000N do menu nao bate 1:1
  # com a ordem que 'domblklist' mostra -- medido em 07/09/2026, achado
  # ao vivo, nao suposto). Sem isto, o <os><boot dev="hd"/></os> generico
  # que o virt-install geraria por padrao so lista o disco (vazio na
  # primeira vez), nenhum CD entra na lista automatica, o firmware esgota
  # a lista unica e cai no Menu de Arranque interativo -- foi exatamente
  # o que aconteceu na primeira tentativa desta VM, medido por
  # 'virsh dumpxml --inactive' mostrando 'grep -c "<boot order"' = 0.
  #
  # boot_order=1 no CD do Windows: arranca dele primeiro enquanto o disco
  # esta vazio. boot_order=2 no disco: depois que o Windows estiver
  # instalado nele, o proprio Setup.exe do CD desiste sozinho quando
  # ninguem aperta a tecla que ele pede (a mesma tela que atrapalhou agora
  # e a que protege depois, no reinicio do meio da instalacao), e o
  # firmware cai para o disco, que ja tem sistema. Os outros dois CDs (
  # resposta, virtio-win) ficam SEM boot_order de proposito: nunca devem
  # ser candidatos a arrancar sozinhos.
  --disk "path=${DISK_PATH},format=qcow2,bus=scsi,boot_order=2"
  --controller "type=scsi,model=virtio-scsi"

  # CD1: midia do Windows (prioridade 1, ver acima). CD2: answer disc
  # (autounattend + drivers). CD3: virtio-win completo, para o
  # FirstLogonCommands instalar as ferramentas convidadas depois do
  # primeiro boot.
  --disk "path=${WIN_ISO},device=cdrom,bus=sata,readonly=on,boot_order=1"
  --disk "path=${ANSWER_ISO},device=cdrom,bus=sata,readonly=on"
  --disk "path=${VIRTIO_ISO},device=cdrom,bus=sata,readonly=on"

  # Rede: 'qemu:///session' nao tem rede virtual 'default' (isso e um
  # objeto do libvirtd de sistema, que o modo de sessao nao roda) --
  # confirmado por 'virsh -c qemu:///session net-list --all' vazio. Em vez
  # dela, rede de usuario (SLIRP) embutida no proprio QEMU: NAT puro,
  # dentro do processo do convidado, sem bridge nenhuma no host e sem
  # tocar a rede real do lider. Mais isolado que a rede virtual de
  # sistema, nao menos.
  --network "user,model=virtio"

  # Regra 2: nunca 0.0.0.0, nunca sdl/desktop.
  --graphics "vnc,listen=127.0.0.1"
  --video "virtio"

  # Console de texto por porta serial (regra 2).
  --serial pty
  --console "pty,target_type=serial"

  # Regra 4: unica ponte de arquivo, somente leitura, so a raiz do projeto.
  --filesystem "type=mount,accessmode=mapped,source=${PROJETO_RO},target=glintfx_ro,readonly=on"

  # --wait 0: define e liga a VM e devolve o terminal na hora, sem esperar
  # a instalacao terminar (que e longa e roda sozinha, desassistida, la
  # dentro). Esperar aqui bloquearia este script indefinidamente; o
  # acompanhamento e feito depois, de fora, por 'virsh dumpxml'/'virsh
  # domstate', nunca ficando pendurado dentro deste comando.
  --noautoconsole
  --autoconsole none
  --wait 0
)

if [ "$ARRANCAR" -eq 0 ]; then
  virt-install --dry-run --print-xml "${VIRT_INSTALL_ARGS[@]}"
  echo
  echo "=== Modo SO-MOSTRA (default). Nada foi definido nem ligado. ==="
  echo "Para definir e ligar de verdade, autorizado pelo lider e com o slot"
  echo "de trabalho pesado livre (L-11): ./criar-vm.sh --arrancar-de-verdade"
  exit 0
fi

virt-install "${VIRT_INSTALL_ARGS[@]}"

echo
echo "=== VM definida e ligada. Rode agora: ./provar-isolamento.sh --dominio ${VM_NAME} ==="
