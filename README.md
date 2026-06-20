# Máquina Virtual Z80

Implementação completa de uma máquina virtual para o microprocessador Zilog
Z80, desenvolvida para a disciplina **Programação de Sistemas** (UFPel /
CDTec — Prof. Dr. Anderson Priebe Ferrugem).

O projeto é dividido em **quatro módulos independentes**, cada um com seu
próprio executável de linha de comando, mais uma **interface gráfica Qt5**
que integra o pipeline completo:

| Módulo                | Executável  | Função                                                              |
|------------------------|-------------|----------------------------------------------------------------------|
| Macro-Montador         | `z80macro`  | Expande macros (uma passagem, com aninhamento) — fonte → fonte       |
| Montador               | `z80asm`    | Monta um `.asm` em código objeto `.obj` (dois passos)                 |
| Ligador                | `z80link`   | Liga um ou mais `.obj` em um executável `.exe` (dois passos)          |
| Executor / Emulador    | `z80exec`   | Carrega e executa um `.exe` em uma CPU Z80 emulada                   |
| Interface Gráfica      | `z80vm-gui` | GUI Qt5 que integra os quatro módulos acima                          |

---

## 1. Compilação

### Pré-requisitos

- CMake ≥ 3.16
- Compilador C++17 (GCC ou Clang)
- Qt5 (`qtbase5-dev`) — **opcional**, apenas para a GUI. Sem o Qt5, o CMake
  ainda configura e compila normalmente as quatro ferramentas de linha de
  comando.

No Ubuntu/Debian:

```bash
sudo apt-get install build-essential cmake qtbase5-dev
```

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Os executáveis são gerados em `build/bin/`:

```
build/bin/z80macro
build/bin/z80asm
build/bin/z80link
build/bin/z80exec
build/bin/z80vm-gui     (somente se o Qt5 foi encontrado)
```

Para compilar sem a GUI (por exemplo, em um ambiente sem Qt5):

```bash
cmake .. -DBUILD_GUI=OFF
make -j$(nproc)
```

---

## 2. Arquitetura geral

```
 .asm fonte
     |
     v
+-------------+   expande macros (1 passagem,
|  z80macro   |   aninhamento de definicoes e
| (chamado    |   chamadas)
| automatic.  |--------------------------------+
| por z80asm) |                                 |
+-------------+                                 v
                                          +--------------+
                                          |   z80asm     |
                                          | (2 passos)   |
                                          +------+-------+
                                                 | .obj
                                                 v
                                          +--------------+
                                          |   z80link    |
                                          | (2 passos)   |
                                          +------+-------+
                                                 | .exe
                                                 v
                                          +--------------+
                                          |   z80exec    |
                                          |  (CPU Z80    |
                                          |   emulada)   |
                                          +--------------+
```

A GUI (`z80vm-gui`) executa exatamente este mesmo pipeline internamente,
adicionando visualização de registradores, flags, memória, pilha e tabela de
símbolos a cada etapa.

---

## 3. Macro-Montador (`z80macro`)

Implementado em **uma única passagem**, ativado automaticamente pelo módulo
integrador do montador (`z80asm`) antes da montagem propriamente dita — não
é necessário invocá-lo manualmente, embora isso também seja possível.

### Características

- **Definição de macros aninhadas**: uma macro pode conter, dentro do seu
  próprio corpo, a definição de outra macro (`MACRO ... ENDM` dentro de
  `MACRO ... ENDM`).
- **Chamada de macros aninhadas**: o corpo de uma macro pode chamar outras
  macros (inclusive uma macro definida dentro dela), e essas chamadas são
  expandidas recursivamente.
- **Parâmetros propagados**: os parâmetros da macro externa são substituídos
  no corpo da macro interna antes desta ser processada, permitindo
  parametrização em múltiplos níveis de aninhamento.
- **Tudo em uma só passagem**: o processador lê o fonte uma única vez,
  registrando definições de macro à medida que as encontra e expandindo
  chamadas assim que reconhecidas — sem necessidade de uma segunda
  varredura do arquivo.

### Sintaxe

```asm
NOME_DA_MACRO MACRO par1, par2, ...
    ; corpo da macro - pode referenciar par1, par2...
    ; pode conter outra definicao MACRO...ENDM (macro aninhada)
    ; pode chamar outras macros (chamada aninhada)
ENDM
```

### Exemplo — macros aninhadas (definição e chamada)

```asm
OUTER MACRO val
    INNER MACRO x
        LD A, x
        ADD A, val
    ENDM
    INNER 10
    INNER 20
ENDM

    ORG 0x0000
    OUTER 5
    HALT
```

Expande para:

```asm
    ORG 0x0000
        LD A, 10
        ADD A, 5
        LD A, 20
        ADD A, 5
    HALT
```

### Uso direto (entrada: fonte → saída: outro fonte)

```bash
z80macro entrada.asm saida_expandida.asm
```

O programa recebe como entrada um arquivo fonte para montagem e gera como
saída **outro arquivo fonte**, já com todas as macros expandidas, pronto
para ser processado pelo montador.

---

## 4. Montador (`z80asm`)

Montador de **dois passos** para o conjunto de instruções do Z80 (transferência
de dados, aritméticas, lógicas, controle de fluxo, pilha e controle de
execução), com suporte aos modos de endereçamento imediato, direto, indireto
via registrador (HL, IX, IY), indexado (IX+d / IY+d) e implícito.

Antes de iniciar a montagem, o `z80asm` invoca internamente o
**macro-montador** sobre o arquivo de entrada — esse é o "módulo principal
integrador" mencionado no enunciado: o ponto único que ativa o
processamento de macros antes de qualquer outra etapa.

### Passo 1 (pass1)
Calcula o tamanho de cada instrução e resolve a tabela de símbolos locais
(rótulos), sem ainda gerar código objeto.

### Passo 2 (pass2)
Gera o código de máquina byte a byte. Símbolos ainda não resolvíveis
localmente (declarados com `EXTERN`) geram entradas de **relocação**
(`RelocEntry`) em vez de erro, para que o ligador as resolva depois.

### Diretivas suportadas

| Diretiva            | Significado                                            |
|----------------------|------------------------------------------------------|
| `ORG endereco`       | Define o endereço de carga do segmento corrente       |
| `SECTION` / `SEGMENT`| Inicia/seleciona um segmento nomeado                   |
| `EQU` / `=`          | Define uma constante simbólica                        |
| `DB` / `DEFB`        | Declara bytes (inclusive literais `'texto'`)           |
| `DW` / `DEFW`        | Declara palavras de 16 bits                            |
| `DS` / `DEFS`        | Reserva espaço (com valor de preenchimento opcional)   |
| `GLOBAL` / `PUBLIC`  | Exporta um símbolo para outros módulos                |
| `EXTERN` / `EXTRN`   | Declara um símbolo definido em outro módulo            |

### Uso

```bash
z80asm entrada.asm saida.obj            # com expansão de macros (padrão)
z80asm entrada.asm saida.obj --no-macro # pula a etapa de macros
```

A saída no console lista a tabela de símbolos resolvidos e, em caso de
erro, a lista de mensagens de erro com o número da linha correspondente.

---

## 5. Ligador (`z80link`)

Implementado em **dois passos**, conforme exigido:

- **Passo 1**: percorre todos os módulos objeto, calcula o endereço-base de
  cada segmento (respeitando diretivas `ORG`) e constrói a tabela global de
  símbolos, verificando duplicatas e símbolos externos não resolvidos.
- **Passo 2**: copia o conteúdo de cada segmento para o buffer final do
  executável e processa cada relocação pendente.

O ligador suporta **dois modos de operação**, selecionáveis por linha de
comando, correspondendo exatamente à distinção pedida no enunciado entre
*Ligador-Relocador* (com Carregador Absoluto) e *apenas Ligador* (com
Carregador Relocador):

### Modo `-abs` — Ligador-Relocador (padrão)

O endereço de carga já é conhecido no momento da ligação (`-org`). O
ligador executa a **relocação completa de endereços** nesse momento,
gravando os valores finais diretamente nos bytes do executável. O
`.exe` resultante já está pronto para ser carregado e executado
imediatamente — esse é o cenário de um **Carregador Absoluto**, que apenas
copia os bytes para a memória sem nenhum processamento adicional.

```bash
z80link -abs -o programa.exe -org 0000 modulo1.obj modulo2.obj
```

### Modo `-reloc` — apenas Ligador (com Carregador Relocador)

O ligador resolve os símbolos e calcula os valores finais, mas **não**
aplica o patch definitivo nos bytes do executável — em vez disso, grava uma
tabela de relocações pendentes (endereço, tipo, valor já resolvido) dentro
do próprio arquivo `.exe`. A finalização da relocação fica a cargo do
**Carregador Relocador**, executado no momento da carga (ver seção 6,
opção `--load-addr` do `z80exec`), que pode posicionar o programa em
qualquer endereço de memória — inclusive um endereço diferente do usado
durante a ligação.

```bash
z80link -reloc -o programa.exe -org 0000 modulo1.obj modulo2.obj
```

### Opções

| Opção           | Descrição                                                            |
|-------------------|------------------------------------------------------------------------|
| `-o <saida.exe>`  | Caminho do executável gerado (padrão: `out.exe`)                       |
| `-m <mapa.map>`   | Gera um arquivo de mapa de ligação (módulos, segmentos, símbolos)       |
| `-org <hex>`      | Endereço de carga inicial usado durante a ligação                      |
| `-abs`            | Ligador-Relocador completo — Carregador Absoluto (padrão)              |
| `-reloc`          | Apenas Ligador — relocação final deixada para o Carregador Relocador   |

### Tipos de relocação suportados

- `ABS16` — endereço absoluto de 16 bits (operandos de `LD`, `JP`, `CALL`, etc.)
- `ABS8` — valor absoluto de 8 bits
- `REL8` — deslocamento relativo de 8 bits (`JR`, `DJNZ`)

---

## 6. Executor / Emulador (`z80exec`)

Implementa a CPU Z80 completa: registradores principais (A, B, C, D, E, H,
L), pares de 16 bits (AF, BC, DE, HL), registradores especiais (PC, SP, IX,
IY, I, R), o registrador de flags (S, Z, H, P/V, N, C), memória de 64 KB e
todos os modos de endereçamento do conjunto básico de instruções exigido
(transferência de dados, aritméticas, lógicas, controle de fluxo, pilha e
controle de execução), além de extensões úteis (CB/ED/DD/FD — rotações,
bit-test, instruções com IX/IY indexado, blocos LDIR/CPIR, etc.).

### Carregador Absoluto vs. Carregador Relocador

Ao carregar um `.exe`:

- Se o executável **não** possui relocações pendentes (gerado com
  `z80link -abs`), o `z80exec` atua como um **Carregador Absoluto**: apenas
  copia os bytes para a memória no endereço gravado pelo ligador e inicia a
  execução.
- Se o executável **possui** relocações pendentes (gerado com
  `z80link -reloc`), o `z80exec` atua como **Carregador Relocador**:
  aplica as relocações pendentes no momento da carga, podendo inclusive
  reposicionar o programa em um endereço diferente do usado durante a
  ligação, através da opção `--load-addr`.

### Uso

```bash
z80exec programa.exe                        # executa até HALT
z80exec programa.exe --trace                 # imprime o estado a cada instrução
z80exec programa.exe --max-cycles 100000      # limite de seguranca de ciclos
z80exec programa.exe --load-addr 1000         # Carregador Relocador: carrega em 0x1000
```

A saída em vídeo dos programas de teste é feita via a porta de I/O `0x00`
(`OUT (0), A` imprime o caractere em `A` no console).

---

## 7. Interface Gráfica (`z80vm-gui`)

A GUI Qt5 integra o pipeline completo em uma única janela:

- **Editor de fonte** com aba para visualizar o resultado pós-expansão de
  macros.
- **Botões de pipeline**: `1. Macro` → `2. Montar` → `3. Ligar` → `4.
  Carregar`, seguindo exatamente a ordem macro-montador → montador →
  ligador → carregador/executor.
- **Seletor de modo do ligador** (Ligador-Relocador / Ligador com
  Carregador Relocador), correspondendo às opções `-abs` / `-reloc` do
  `z80link`.
- **Visualização de registradores** (A, F, B, C, D, E, H, L e pares de 16
  bits, PC, SP, IX, IY, I, R, IFF1/IFF2, estado de HALT), atualizada após
  cada instrução ou execução contínua.
- **Visualização de flags** (S, Z, H, P/V, N, C) individualmente.
- **Visualização de memória** em formato hexadecimal, navegável por
  endereço, com destaque para a posição do PC e do SP.
- **Visualização de pilha** (8 palavras a partir do SP atual).
- **Tabela de símbolos** gerada pelo montador.
- **Console de saída** (porta de I/O 0) e **log** de erros/mensagens de
  cada etapa do pipeline.
- Controles de execução: **Executar** (contínuo), **Passo** (instrução a
  instrução), **Parar** e **Reset CPU**.

### Execução

```bash
./build/bin/z80vm-gui
```

---

## 8. Estrutura do repositório

```
Z80/
|-- CMakeLists.txt          (build unificado: CLI + GUI)
|-- README.md
|-- include/                 (bibliotecas de cabecalho compartilhadas)
|   |-- types.hpp            (tipos basicos, Z80Regs, ObjectFile, RelocEntry...)
|   |-- macro.hpp            (processador de macros: uma passagem, aninhado)
|   |-- expr.hpp             (avaliador de expressoes do montador)
|   |-- encoding.hpp         (tabelas de codificacao de registradores/modos)
|   |-- assembler.hpp        (montador de dois passos)
|   |-- objfmt.hpp           (serializacao do formato .obj)
|   |-- linker.hpp           (ligador de dois passos + formato .exe)
|   `-- cpu.hpp              (CPU Z80 emulada - executor)
|-- src/
|   |-- macro.cpp            (CLI do macro-montador: z80macro)
|   |-- asm.cpp              (CLI do montador: z80asm - invoca o macro-montador)
|   |-- link.cpp             (CLI do ligador: z80link)
|   `-- exec.cpp             (CLI do executor: z80exec)
|-- gui/
|   |-- main.cpp             (ponto de entrada da aplicacao Qt5)
|   |-- mainwindow.hpp       (janela principal - declaracao)
|   `-- mainwindow.cpp       (janela principal - implementacao)
`-- tests/                   (programas .asm de exemplo/teste)
    |-- test1.asm                  (macros aninhadas, loop, flags)
    |-- test2_ix.asm               (enderecamento indexado IX, pilha)
    |-- test_nested_macro.asm      (macro definida e chamada dentro de outra)
    |-- mod_a.asm                  (modulo com EXTERN/CALL)
    `-- mod_b.asm                  (modulo com GLOBAL - ligacao multi-modulo)
```

---

## 9. Exemplos de uso ponta a ponta

### Programa único

```bash
z80asm programa.asm programa.obj
z80link -abs -o programa.exe -org 0000 programa.obj
z80exec programa.exe
```

### Múltiplos módulos com símbolos externos

```bash
z80asm modulo_a.asm modulo_a.obj
z80asm modulo_b.asm modulo_b.obj
z80link -abs -o final.exe -org 0000 -m final.map modulo_a.obj modulo_b.obj
z80exec final.exe
```

### Ligação relocável + carga em endereço diferente

```bash
z80asm programa.asm programa.obj
z80link -reloc -o programa.exe -org 0000 programa.obj
z80exec programa.exe --load-addr 1000     # Carregador Relocador reposiciona o programa
```

---

## 10. Testes incluídos

Os arquivos em `tests/` demonstram, na prática, os requisitos centrais do
trabalho:

- `test1.asm` — uma macro chama outra macro (`PRINT_AB` chama `PRINT_CHAR`
  duas vezes), validando chamadas aninhadas em uma só passagem.
- `test_nested_macro.asm` — uma macro (`OUTER`) **define** outra macro
  (`INNER`) em seu próprio corpo e a chama múltiplas vezes com parâmetros
  diferentes, validando definição aninhada de macros.
- `mod_a.asm` / `mod_b.asm` — dois módulos ligados com símbolos `GLOBAL` /
  `EXTERN`, exercitando a resolução de relocação `CALL` entre módulos.
- `test2_ix.asm` — endereçamento indexado (`IX+d`), operações de pilha
  (`PUSH`/`POP`) e troca de registradores de 16 bits.

Para rodar manualmente qualquer teste:

```bash
cd build/bin
./z80asm ../../tests/test1.asm /tmp/test1.obj
./z80link -o /tmp/test1.exe -org 0000 /tmp/test1.obj
./z80exec /tmp/test1.exe
```

---

## 11. Bibliografia

- ZILOG. *Z80 CPU User Manual*.
- STALLINGS, William. *Computer Organization and Architecture*.
- TANENBAUM, Andrew. *Structured Computer Organization*.
- KOLIVER, Cristian. *Tradução de Programas – Da Montagem à Carga*.