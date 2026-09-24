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

## O que esta fronteira PROVA, e o que ela NÃO prova (atualizado 23/09/2026)

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
- **Isolamento da sessão do líder**, com portão próprio de sete categorias
  (`provar-isolamento.sh`), cada uma provada mordendo por sabotagem antes
  de valer. A sétima (enlace de rede do convidado) entrou em 22/09/2026 -
  ver a seção "Rede do convidado" abaixo.
- **O primeiro ARRANQUE REAL da máquina, pela cópia avulsa** (item
  `WIN-RUNNER-PROPRIO`, sub-fatias V-5b/V-5c/V-5, 23/09/2026): `virsh
  create` aceita uma cópia avulsa da definição, com o MESMO nome e
  identificador do domínio persistente, sem tocar a definição permanente -
  confirmado por `sha256sum` do `dumpxml --inactive` idêntico antes e
  depois de quatro sessões reais. Ver "O ciclo novo: liga por cópia
  avulsa" logo abaixo.
- **O par verde/vermelho, de novo, mas agora com o binário recompilado do
  HEAD no container e executado na máquina real** (não mais um par
  congelado de 07/09/2026): `wgl_proc_address_test.exe` sai com código 0
  (4 casos, 0 falhas); o mutante correspondente
  (`wgl_proc_address_test_MUTANT.exe`) sai com código 1, uma falha nomeada
  em `win32_wgl_proc_address_test.cpp:126`.
- **A área de trabalho de sessão não deixa rastro entre sessões (E3)**,
  com controle positivo: uma marca escrita numa sessão é confirmada
  AUSENTE na sessão seguinte, com o motivo da ausência classificado (erro
  de "arquivo inexistente", nunca "canal morto" disfarçado de ausência) -
  e um controle positivo (abrir `C:\Windows\win.ini`, que sempre existe)
  prova que o canal estava vivo quando a marca foi checada.
- **O convidado não alcança rede nenhuma (E4)**, com régua calibrada: as
  três sondas de saída (hospedeiro, DNS da rede de usuário, destino
  externo) falham, e um ouvinte TCP local em `127.0.0.1` (que não depende
  do adaptador de rede) prova que a sonda em si funciona - sem essa
  calibração, "falhou" não distinguiria enlace desligado de canal quebrado.
- **O limite prático de bytes por chamada `guest-file-read`, medido, não
  mais um palpite**: até 2 MiB (2.097.152 bytes) por chamada devolve
  `count` batendo com o pedido; 4 MiB (4.194.304 bytes) é recusado pelo
  RPC do libvirt com `"Unable to encode message payload"` -
  `VIR_NET_MESSAGE_STRING_MAX`, constante do libvirt
  (`src/rpc/virnetprotocol.x`) que limita um campo STRING dentro da
  mensagem RPC, valor 4.194.304. `coletar-resultados.sh` usa 2 MiB como
  bloco padrão desde a sub-fatia V-5d.
- **A segunda camada contra "ligar a máquina à mão" (E10)**: com a máquina
  já ligada pela cópia avulsa, `virsh start` do domínio permanente recusa
  (o próprio libvirt, porque o identificador já está ativo), e `lslocks`
  mostra a trava `OFDLCK` do processo `qemu-system-x86` sobre o disco base
  - conferido só por leitura, nunca tentando escrever no disco base para
  sondar a trava.

**Não provado ainda, dito com todas as letras para não ficar calado:**

- **Janela real** (a máquina roda headless, console serial mais VNC preso
  a `127.0.0.1`; nada disto foi tentado com uma janela de verdade).
- **Contexto gráfico com placa de vídeo real** (o vídeo da VM é
  `virtio-vga`, renderizado por software; nenhuma GPU foi repassada).
- **Sanitizador** (ASan/UBSan de binário Windows dentro desta máquina,
  não tentado).
- **O teto exato do limite de bytes do `guest-file-read`**: medido apenas
  que 2 MiB passa e 4 MiB não passa; a fronteira teórica exata (onde o
  texto base64 da resposta cruza os 4.194.304 bytes do
  `VIR_NET_MESSAGE_STRING_MAX`, aritmeticamente perto de 3.145.728 bytes
  crus) não foi testada ponto a ponto.
- **"48 MiB + 1 recusado" (a exigência original da decisão D-6)**: o canal
  do agente convidado quebra bem antes de chegar perto desse tamanho (a
  partir de qualquer pedido acima do teto do `STRING_MAX`, toda chamada
  seguinte ao agente falha com "Guest agent is not responding" até o fim
  da sessão) - **não mensurável por este caminho**, ausência declarada.
  Provar aquele ponto especificamente exigiria ler em blocos menores que
  o `STRING_MAX` e comparar contra o tamanho real do arquivo, nunca uma
  leitura de bloco único gigante.
- **A primeira consolidação REAL** (juntar a sobreposição de uma sessão de
  volta ao disco de 17 GiB) - o caminho está escrito e provado só contra
  um par base/sobreposição de brinquedo (`consolidar.sh`, sub-fatia V-6a);
  a consolidação real é item separado (`WIN-LAB-INSTALAR`), que exige aval
  do líder (ação irreversível, L-01).
- **Qualquer instalação dentro do convidado**, com o enlace de rede
  religado só para isso - também `WIN-LAB-INSTALAR`, também exige aval do
  líder (L-51).

Relatório completo do primeiro fecho, com os números crus e a linha de
comando literal de cada chamada: `RELATORIO.md` neste mesmo diretório.

## Arquivos aqui

| Arquivo | O que é |
|---|---|
| `criar-vm.sh` | Cria (e, com `--arrancar-de-verdade`, liga) a máquina via `virt-install`. Sem esse argumento, só mostra o XML que seria gerado. |
| `provar-isolamento.sh` | Portão de isolamento, sete categorias (entrada real do hospedeiro, servidor de imagem fora de `127.0.0.1`, pasta do usuário montada, montagem de escrita fora do projeto, canal de área de transferência, canal para o convidado que não seja o agente QEMU declarado, enlace de rede do convidado ainda ativo). Tem `--selftest`. |
| `check-sem-segredo.sh` | Portão que reprova se a senha do laboratório (ou algo com a forma dela) aparecer em qualquer arquivo deste diretório. Rode antes de qualquer commit aqui. |
| `type-string.sh` | Digita texto no convidado via `virsh send-key`, caractere a caractere; só necessário enquanto o canal do agente QEMU (ver abaixo) não estiver de pé. Depois dele, prefira `transferir-executar.sh`. Contém o mapeamento de teclado medido para o layout pt-BR/ABNT2 do convidado (`\` e `:` não saem das teclas óbvias do codeset `linux`). |
| `transferir-executar.sh` | Transfere um arquivo para o convidado pelo canal `org.qemu.guest_agent.0` (`guest-file-write`, em blocos, com contagem de bytes conferida). |
| `rodar-caminho.sh` | Roda um binário Windows por CAMINHO completo via `guest-exec`, espera terminar e devolve o resultado no próprio código de saída do script (0 sucesso, 1 falha ao iniciar, 3 convidado terminou com erro, 124 estourou o prazo). Prazo configurável por argumento. Tem `--selftest`. |
| `rodar-um.sh` | Mesma coisa que `rodar-caminho.sh`, mas recebe só o NOME do arquivo (resolvido para `C:\Users\glintfx\<nome>`). Tem `--selftest`. |
| `sessao.sh` | Motorista de ciclo de vida (WIN-RUNNER-PROPRIO V-2/V-5b/V-5c): trava de exclusão mútua (`flock`), segunda verificação independente por `domstate`, sobreposição qcow2 descartável do disco que nasce e morre dentro da mesma posse da trava, teardown incondicional por `trap`. Com `--ligar`: gera uma CÓPIA AVULSA da definição (mesmo nome/identificador do domínio persistente, que nunca é tocado) e liga por ela - ver "O ciclo novo" abaixo. Tem `--selftest`. |
| `consolidar.sh` | Caminho de CONSOLIDAÇÃO da sobreposição (WIN-RUNNER-PROPRIO V-6a): junta as mudanças de uma sobreposição de volta ao disco base dela (`qemu-img commit`), com três guardas - trava de exclusão mútua, máquina confirmada desligada, e cópia de segurança (`cp --reflink=always`) conferida por soma ANTES do commit. **Nunca toca o disco real de 17 GiB nesta sub-fatia**: o `--selftest` roda só contra um par base/sobreposição de brinquedo que ele mesmo cria e apaga. Tem `--selftest`. |
| `coletar-resultados.sh` | Coletor de resultado (WIN-RUNNER-PROPRIO V-3/V-5d): traz de volta TODO arquivo de um diretório de resultados do convidado via `guest-file-read`, com md5 conferido nas duas pontas (hash pedido ao convidado por `certutil -hashfile` contra o hash da cópia local reconstruída), piso de varredura não-vazia (diretório vazio é recusa, nunca sucesso silencioso) e lista de PERMISSÃO para nome de arquivo (o convidado é não confiável por desenho; nome fora do padrão `^[A-Za-z0-9][A-Za-z0-9._-]*$` é rejeitado ANTES de virar caminho no hospedeiro, nunca por `continue` calado - código de saída próprio, 2). Bloco padrão de leitura: 2 MiB, medido contra a máquina real (V-5d) - ver "O que esta fronteira PROVA" acima. Tem `--selftest`. |
| `autounattend.xml` | Arquivo de respostas do instalador do Windows, comentado linha a linha. **Senha substituída pelos marcadores `__ADMIN_PASSWORD__`/`__USER_PASSWORD__`**, nunca a senha real (ver seção própria abaixo). |
| `fixtures/dominio-limpo.xml`, `fixtures/dominio-sabotado.xml` | Definições de VM usadas pelo `--selftest` do portão de isolamento. |
| `RELATORIO.md` | Registro completo do primeiro fecho desta fronteira: decisões tomadas, defeitos achados e corrigidos (na própria receita, nunca escondidos), e o par verde/vermelho provado contra o Windows real. |

## O ciclo novo: liga por cópia avulsa, com três redireções (V-5b/V-5c, 22-23/09/2026)

Decisão do líder, 22/09/2026, por `AskUserQuestion`: **"cópia temporária da
configuração"**. Em vez de apontar a definição PERSISTENTE do domínio para
um disco descartável (o que exigiria alterá-la a cada sessão), `sessao.sh
--ligar` gera uma CÓPIA AVULSA do XML - com o MESMO nome e identificador do
domínio persistente - e liga por essa cópia via `virsh create`. A definição
persistente nunca é tocada: só lida (`dumpxml --inactive`), nunca `define`,
`edit`, nem `--config`.

**A cópia troca EXATAMENTE TRÊS coisas** em relação ao permanente - nem
menos, nem mais (`sessao.sh`, função `contar_diferencas_e_validar`, prova
isso duas vezes antes de `virsh create` rodar: conta que são três, E
reconstrói o que a cópia "deveria" ser a partir do permanente + as três
trocas, comparando byte a byte):

1. **Disco do sistema** → sobreposição qcow2 descartável (`qemu-img create
   -b/-F`), mesmo mecanismo desde a V-2. O disco base fica **somente
   leitura** durante a sessão (confirmado no `blockdev` real do QEMU:
   `"read-only":true` no nó do disco base).
2. **NVRAM (variáveis de firmware)** → **cópia INTEIRA** (`cp`), não
   sobreposição. Achado da V-5c (primeiro arranque real, 23/09/2026):
   libvirt abre o pflash do `<nvram>` com `"backing":null`, um só nó de
   blockdev - diferente do `<disk>`, que ganha dois nós com religa
   explícita. A cadeia de backing gravada no cabeçalho de um overlay de
   NVRAM NUNCA é honrada em tempo de execução; o convidado arrancava com a
   área de variáveis ZERADA (nem Secure Boot, nem ordem de arranque), e o
   `qemu-ga` nunca chegava a responder. Corrigido no mesmo molde do item 3.
3. **Estado do TPM emulado (swtpm)** → **cópia INTEIRA** (`cp -r` do
   diretório), desde a V-2 - nunca foi qcow2, é um diretório de estado do
   `swtpm`.

As três cópias nascem e morrem dentro da MESMA posse da trava
(`sessao.sh`), com teardown incondicional por `trap` em toda saída
(normal, erro no meio, `SIGTERM`) - `SIGKILL` é a única exceção conhecida
(nenhum `trap` a intercepta), e por isso a PRÓXIMA sessão recusa se achar
uma sobreposição órfã, em vez de reaproveitar um disco cujo conteúdo não é
confiável.

**Prova de que as duas guardas antes de `virsh create` funcionam:**
`sessao.sh --selftest` inclui `V5B-DIFF` (a cópia legítima aprova; listen
exposto, enlace religado, uma quarta diferença qualquer, ou só DUAS das
três trocas - todos reprovam) e `V5B-CREATE` (conta, com um duble de
`virsh`, que uma cópia sabotada nunca chega a chamar `create`, e uma cópia
legítima chama exatamente uma vez).

## Rede do convidado: desligada por padrão (22/09/2026)

Decisão do líder por `AskUserQuestion`, 22/09/2026: **"Dirigir daqui, sem
ligar ao servidor"**. Com o ciclo de vida dirigido pelo hospedeiro
(`sessao.sh` + `coletar-resultados.sh`, sub-fatias V-2/V-3), o convidado não
precisa de rede para NADA do fluxo - entrada, execução e saída passam todas
pelo canal `org.qemu.guest_agent.0` (virtio-serial, sem IP). A rede virou
**negação permanente** (GODS_LAWS.md global L-02): todo `<interface>` tem
de trazer `<link state='down'/>` explícito, e a categoria `[7/7]` do portão
de isolamento (`provar-isolamento.sh`) reprova qualquer definição que não
tenha essa marca em cada interface, seja qual for o `@type`.

**Estado real, aplicado em 22/09/2026 às ~16:19 (fato medido, não mais
`[A VERIFICAR]`):** a definição persistente do domínio `glintfx-win11-lab`
já tem `<link state='down'/>` dentro do `<interface type='user'>`
(MAC `52:54:00:fa:f5:80`). **Quem aplicou foi o próprio líder, no terminal
dele** - o classificador de permissão automático desta sessão de agente
negou a ação duas vezes ("Modify Shared Resources"), e a alteração real
não foi contornada por este agente. Comando e saída literais, do líder:

```
$ virsh -c qemu:///session domif-setlink glintfx-win11-lab 52:54:00:fa:f5:80 down --config
Dispositivo atualizado com sucesso
```

**Resposta à pergunta que ficava `[A VERIFICAR]` na seção 4 do plano:
`domif-setlink --config` MORDE numa interface `type='user'`, com o domínio
desligado.** A alternativa por `detach-device --config` não foi necessária.

Conferido de forma independente, só por leitura (nunca escrita) nesta
sessão de agente:

- `./provar-isolamento.sh --dominio glintfx-win11-lab` aprova (código 0) as
  sete categorias contra a definição real, e `./provar-isolamento.sh --file
  fixtures/dominio-sabotado.xml` continua reprovando (código 1) - a
  categoria `[7/7]` distingue as duas definições corretamente.
- `diff` entre a cópia guardada antes da mudança
  (`/var/tmp/glintfx-win-lab/dominio-antes-da-V4.xml`, md5
  `2f5a3db6f98f60552b28ea9d23966aaa`) e o `dumpxml --inactive` atual mostra
  **uma única linha acrescentada** (`<link state='down'/>`) - nada mais na
  definição mudou.
- `/home/petrus/.config/libvirt/qemu/glintfx-win11-lab.xml` (o arquivo de
  configuração persistente do libvirt) tem `mtime` de 22/09/2026 16:19:57,
  batendo com o horário do comando; o `nvram` do domínio manteve o `mtime`
  de antes (09:04:35) - nenhuma outra parte do domínio foi tocada.

**Já medido, atualizando o que este parágrafo chamava de "pendente" (V-5,
23/09/2026):** a metade viva de E4 rodou contra a máquina real - as três
sondas de saída (hospedeiro, DNS da rede de usuário, destino externo)
falharam, com um ouvinte TCP local em `127.0.0.1` provando que a sonda em
si funciona (régua calibrada, D-5) - e o limite de bytes por chamada
`guest-file-read` foi medido (2 MiB passa limpo, 4 MiB é recusado pelo
`VIR_NET_MESSAGE_STRING_MAX` do libvirt). Ver "O que esta fronteira PROVA"
no topo deste arquivo, e o relatório completo em `/var/tmp/glintfx-plan/
win-lab-estreia/V5-RELATORIO.md` (não versionado - evidência de sessão).

**Para religar a rede numa sessão de instalação autorizada:**
`virsh -c qemu:///session domif-setlink glintfx-win11-lab 52:54:00:fa:f5:80 up --config`
(comando já medido `down`/mordendo em `type='user'` - `up` usa a mesma
sintaxe), e desligar de novo (`... down --config`) ao fim da sessão -
nunca deixar a rede ligada por padrão fora de uma sessão explicitamente
autorizada.

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
   tools/win-vm-lab/rodar-caminho.sh tools/win-vm-lab/rodar-um.sh \
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
0), nas sete categorias. Portão que nasce sem essa prova não vale; não
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

Tem de aprovar (código 0) as sete categorias contra `virsh dumpxml`, não
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
