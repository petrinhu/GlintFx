# Laboratório de VM Windows do GlintFx

Receita completa para recriar, do zero, a terceira fronteira de isolamento
de teste do GlintFx no Windows: uma máquina virtual headless (`libvirt`/
`QEMU`, `qemu:///session`), instalada de forma desassistida, isolada da
sessão do líder por desenho, usada para **executar** binário do Windows
(o container constrói; esta máquina executa -- ver `GODS_LAWS.md` do
projeto, lei L-09, "REGRA DE ROTEAMENTO ENTRE AS DUAS FRONTEIRAS").

## Por que isto está versionado, e não só em `/var/tmp`

**A árvore de trabalho da máquina (`/var/tmp/glintfx-win-lab/`) mora num
diretório que parece descartável, mas não é.** `/var/tmp` é onde este
projeto guarda todo trabalho pesado (build, disco de VM) por regra própria
(`/tmp` nesta máquina é RAM), e uma limpeza cega de temporários apagaria
de uma vez a máquina, a imagem oficial da Microsoft já conferida por
assinatura, o disco de resposta, os drivers extraídos, o portão de
isolamento e o relatório do primeiro fecho real desta fronteira: o
trabalho mais difícil de refazer desta onda.

**Este diretório (`tools/win-vm-lab/`, dentro do repositório) existe para
que isso não seja fatal.** Se `/var/tmp/glintfx-win-lab/` desaparecer por
acidente, a receita aqui basta para reconstruir a máquina do zero; o
único custo é baixar a imagem da Microsoft de novo e reconferir a
assinatura.

## O que esta fronteira PROVA, e o que ela NÃO prova (07/09/2026)

**Provado, medido, não suposto:**

- **Execução real** de binário Windows compilado pelo `cl.exe` real da
  Microsoft dentro de um container (o binário carrega o caminho de origem
  `z:/...` do container, prova de proveniência embutida no próprio
  artefato).
- **Carga de biblioteca e tempo de execução real da linguagem**: o
  container responde só se compila e liga; só a máquina virtual roda algo.
- **Mutação provada contra o sistema real**, pela primeira vez neste
  projeto: um binário mutante (mesmo teste, compilado contra código
  sabotado de propósito) reprova com código de saída diferente de zero; o
  binário real passa com código zero. Par verde/vermelho, não só "rodou".
- **Isolamento da sessão do líder**, com portão próprio de seis categorias
  (`provar-isolamento.sh`), cada uma provada mordendo por sabotagem antes
  de valer.

**Não provado ainda, dito com todas as letras para não ficar calado:**

- **Janela real** (a máquina roda headless, console serial mais VNC preso
  a `127.0.0.1`; nada disto foi tentado com uma janela de verdade).
- **Contexto gráfico com placa de vídeo real** (o vídeo da VM é
  `virtio-vga`, renderizado por software; nenhuma GPU foi repassada).
- **Sanitizador** (ASan/UBSan de binário Windows dentro desta máquina,
  não tentado).

Relatório completo do primeiro fecho, com os números crus e a linha de
comando literal de cada chamada: `RELATORIO.md` neste mesmo diretório.

## Arquivos aqui

| Arquivo | O que é |
|---|---|
| `criar-vm.sh` | Cria (e, com `--arrancar-de-verdade`, liga) a máquina via `virt-install`. Sem esse argumento, só mostra o XML que seria gerado. |
| `provar-isolamento.sh` | Portão de isolamento, seis categorias (entrada real do hospedeiro, servidor de imagem fora de `127.0.0.1`, pasta do usuário montada, montagem de escrita fora do projeto, canal de área de transferência, canal para o convidado que não seja o agente QEMU declarado). Tem `--selftest`. |
| `check-sem-segredo.sh` | Portão que reprova se a senha do laboratório (ou algo com a forma dela) aparecer em qualquer arquivo deste diretório. Rode antes de qualquer commit aqui. |
| `type-string.sh` | Digita texto no convidado via `virsh send-key`, caractere a caractere; só necessário enquanto o canal do agente QEMU (ver abaixo) não estiver de pé. Depois dele, prefira `transferir-executar.sh`. Contém o mapeamento de teclado medido para o layout pt-BR/ABNT2 do convidado (`\` e `:` não saem das teclas óbvias do codeset `linux`). |
| `transferir-executar.sh` | Transfere um arquivo para o convidado pelo canal `org.qemu.guest_agent.0` (`guest-file-write`, em blocos, com contagem de bytes conferida). |
| `autounattend.xml` | Arquivo de respostas do instalador do Windows, comentado linha a linha. **Senha substituída pelos marcadores `__ADMIN_PASSWORD__`/`__USER_PASSWORD__`**, nunca a senha real (ver seção própria abaixo). |
| `fixtures/dominio-limpo.xml`, `fixtures/dominio-sabotado.xml` | Definições de VM usadas pelo `--selftest` do portão de isolamento. |
| `RELATORIO.md` | Registro completo do primeiro fecho desta fronteira: decisões tomadas, defeitos achados e corrigidos (na própria receita, nunca escondidos), e o par verde/vermelho provado contra o Windows real. |

## Sobre a senha: nunca versionada, sempre gerada de novo

`autounattend.xml` traz os marcadores `__ADMIN_PASSWORD__` e
`__USER_PASSWORD__` em vez de uma senha real. `criar-vm.sh` gera uma senha
aleatória de 24 caracteres (`openssl rand -base64 24`, sem `/+=`) na
primeira execução, grava em `.senha-conta-lab-NAO-VERSIONAR.txt` (modo
`600`, fora deste repositório, dentro de `/var/tmp/glintfx-win-lab/`), e
substitui os dois marcadores só na cópia do arquivo de resposta que vai
para o CD de instalação (`answer-disc/autounattend.xml`); o template
aqui, em `tools/win-vm-lab/`, nunca vê a senha real.

Se precisar reconstruir a máquina e não tiver mais a senha antiga: apague
`.senha-conta-lab-NAO-VERSIONAR.txt` (se existir) e rode `criar-vm.sh` de
novo, ele gera uma senha nova sozinho. **Nunca** cole uma senha real
neste diretório versionado; rode `check-sem-segredo.sh` antes de qualquer
commit aqui para confirmar.

## Como refazer a máquina do zero

### 1. Baixe as duas imagens (fora deste repositório, em `/var/tmp`)

**Imagem do Windows**: Windows 11 Enterprise LTSC 2024, pt-BR, x64,
avaliação (90 dias, sem chave). Fonte oficial: Microsoft Evaluation
Center (`https://www.microsoft.com/evalcenter/`), procurando por
"Windows 11 Enterprise" e selecionando a edição/idioma corretos. **A URL
exata usada na primeira vez não foi capturada por este trabalho**;
ausência declarada, não escondida. Quem baixar de novo confere a
assinatura abaixo antes de confiar no arquivo, seja qual for a página de
onde veio:

```
sha256sum win11-ltsc2024-ptbr-x64-eval.iso
# esperado: a939263c9a98d1ee5d542fe01e4f3744c28f6140973e34da77be67be2c7dfc8b
```

Se o hash não bater, **pare, não instale um arquivo que não confere.**

**Drivers paravirtuais (virtio-win)**: fonte oficial e estável,
`https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/stable-virtio/virtio-win.iso`
(mantido pelo grupo de virtualização da Fedora, é a distribuição canônica
destes drivers). Confira o tamanho do arquivo baixado contra o anunciado
pelo servidor antes de usar.

Salve as duas ISOs em `/var/tmp/glintfx-win-lab/` com os nomes que
`criar-vm.sh` espera: `win11-ltsc2024-ptbr-x64-eval.iso` e
`virtio-win-stable.iso`.

### 2. Confirme os pré-requisitos do hospedeiro

```
rpm -q qemu-kvm virt-install edk2-ovmf swtpm osinfo-db
rpm -q virt-manager virt-viewer   # tem de dizer "nao esta instalado" para os dois
virsh -c qemu:///session version  # tem de responder sem pedir senha
```

Nunca instale `virt-manager`/`virt-viewer`: a lei L-09 (gerência só por
linha de comando) proíbe a peça gráfica de propósito, e `criar-vm.sh`
recusa rodar se algum dos dois aparecer instalado.

### 3. Copie estes arquivos para o diretório de trabalho

```
mkdir -p /var/tmp/glintfx-win-lab/fixtures
cp tools/win-vm-lab/criar-vm.sh tools/win-vm-lab/provar-isolamento.sh \
   tools/win-vm-lab/type-string.sh tools/win-vm-lab/transferir-executar.sh \
   tools/win-vm-lab/autounattend.xml /var/tmp/glintfx-win-lab/
cp tools/win-vm-lab/fixtures/*.xml /var/tmp/glintfx-win-lab/fixtures/
chmod +x /var/tmp/glintfx-win-lab/*.sh
```

### 4. Prove o portão de isolamento ANTES de criar a máquina

```
cd /var/tmp/glintfx-win-lab
./provar-isolamento.sh --selftest
```

Tem de reprovar a definição sabotada (código 1) e aprovar a limpa (código
0), nas seis categorias. Portão que nasce sem essa prova não vale; não
prossiga sem ver os dois códigos certos.

### 5. Crie e arranque a máquina

```
./criar-vm.sh                        # so mostra o XML, nada e criado
./criar-vm.sh --arrancar-de-verdade  # define e liga de verdade
```

A instalação é desassistida (o `autounattend.xml` responde tudo). O
firmware vai pedir uma tecla ("Press any key to boot from CD or DVD") na
primeira inicialização: é o carregador da própria mídia da Microsoft, não
falha nossa. Envie exatamente uma tecla pela API de gerência quando a
janela abrir (`virsh send-key <dominio> --codeset linux KEY_ENTER`), sem
esperar captura de tela para decidir. **Depois da instalação, essa mesma
tela some de novo a cada reinício automático do instalador; não a
responda:** ela existe para impedir reinstalação acidental, e é o que
evita que um reinício no meio da instalação vire laço infinito. Meça
sempre o PAR (tempo de processador do domínio mais tamanho do disco),
nunca só a tela, para saber se a máquina está trabalhando ou parada.

### 6. Prove o isolamento contra a máquina REAL, viva

```
./provar-isolamento.sh --dominio glintfx-win11-lab
```

Tem de aprovar (código 0) as seis categorias contra `virsh dumpxml`, não
contra um arquivo candidato.

### 7. Ligue o canal do agente QEMU (recomendado, substitui a ponte de arquivo)

O Windows não tem cliente 9p para o backend padrão que o `virt-install`
usaria para uma ponte de arquivo; por isso a receita atual não usa ponte
de arquivo nenhuma. Em vez disso:

```
virsh -c qemu:///session attach-device <dominio> <(echo '<controller type="virtio-serial" index="0"/>') --live --config
virsh -c qemu:///session attach-device <dominio> <(echo '<channel type="unix"><target type="virtio" name="org.qemu.guest_agent.0"/></channel>') --live --config
virsh -c qemu:///session qemu-agent-command <dominio> '{"execute":"guest-ping"}'   # tem de responder {"return":{}}
```

O `virtio-win-guest-tools.exe` (instalado sozinho pelo `FirstLogonCommands`
do `autounattend.xml`) já traz o `qemu-ga` dentro do convidado; só falta
o canal do lado do hospedeiro. Depois disso, `transferir-executar.sh` e as
chamadas `guest-exec`/`guest-exec-status` bastam para levar binário e ler
código de saída, sem tela e sem tecla.

Rode `./provar-isolamento.sh --dominio <dominio>` mais uma vez depois de
anexar o canal: mudou a definição, reprova-se a definição.
