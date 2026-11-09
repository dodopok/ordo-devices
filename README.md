# ordo-devices

Firmware e apps de aparelhos que mostram o dia litúrgico de quem usa o
[Ordo](https://github.com/dodopok/ordo-app): a **bússola** (ESP32-S3 com tela
AMOLED redonda) e, em seguida, o app do **Miblo Desk**. Os dois falam com a
[estevao-api](https://github.com/dodopok/estevao-api) pelo mesmo caminho que
qualquer outro aparelho: o manual é
[`docs/DEVICES.md`](https://github.com/dodopok/estevao-api/blob/main/docs/DEVICES.md)
lá na API.

| Pasta | O quê | Estado |
| --- | --- | --- |
| [`compass/`](compass/) | Firmware da bússola para a Waveshare ESP32-S3-Touch-AMOLED-1.75 (e 1.75C) | 0.2: chaveiro, compila, ainda não testado na placa |
| [`miblo/`](miblo/) | App de página para o Miblo Desk, publicado na Loja de Apps do Desk | fase 4, primeira versão |

## A bússola

Um chaveiro litúrgico. Dorme quase o dia inteiro e acorda num toque de botão,
já mostrando o dia: a hora, o tempo litúrgico na sua cor, o nome do dia e os
quatro ofícios. O anel do ano vai do Advento (no topo) até a véspera do
próximo Advento, com as festas principais como pontos e um marcador em hoje.
Quando os quatro ofícios estão orados, o anel fica dourado.

A bússola só lê. Concluir um ofício continua no app; ela reflete isso na
próxima sincronização.

### Placas

| Ambiente | Placa | Diferenças |
| --- | --- | --- |
| `amoled175c` (padrão) | ESP32-S3-Touch-AMOLED-1.75C, com ou sem bateria | 32 MB de flash, reset da tela no GPIO1, reset do toque no GPIO2, botão PWR no GPIO3, sino pelo alto-falante |
| `amoled175` | ESP32-S3-Touch-AMOLED-1.75, -B e -G | 16 MB de flash, resets nos GPIO39/40, PWR lido pelo expansor TCA9554; sem sino por enquanto e só BOOT (ou o toque) acorda do sono |
| `amoled175c-debug` | a 1.75C com log serial | para ver o que o firmware faz; acorda mais devagar |

Os pinos estão em [`compass/include/board_pins.h`](compass/include/board_pins.h),
conferidos com os repositórios e o esquema oficiais da Waveshare.

### Compilar e gravar

Precisa do [PlatformIO](https://platformio.org/install/cli). A plataforma
(Arduino-ESP32 3.x, via pioarduino) e as bibliotecas são baixadas na primeira
compilação.

```bash
cd compass
pio run -e amoled175c              # compila
pio run -e amoled175c -t upload    # grava pelo USB-C
pio run -e amoled175c-debug -t upload && pio device monitor -b 115200   # com log
```

A bússola dormindo some do USB. Para gravar, acorde-a com o PWR (no cabo ela
fica acordada por padrão) ou entre no modo de gravação: segure BOOT, toque em
RESET, solte BOOT.

Quem vem do firmware 0.1 não perde nada ao gravar: o vínculo, o Wi-Fi e o
último dia continuam lá.

### Primeira vez

1. **Wi-Fi.** A bússola abre uma rede `Ordo-XXXX` e mostra um QR. Aponte a
   câmera do celular (ou entre na rede): abre sozinha uma página com as redes
   por perto. Escolha a sua e digite a senha.
2. **Vínculo.** A bússola pede um código à API e mostra `KQ7-MD4` com um QR.
   No Ordo, em Configurações › Aparelhos › Vincular com código, digite o
   código, ou leia o QR com a câmera do celular. Precisa de Ordo+.
3. Em alguns segundos a bússola recebe o token dela e passa a mostrar o dia.

O token fica no NVS do ESP32 e nunca aparece na tela nem no log. A rede
`Ordo-XXXX` só existe durante a configuração e fecha sozinha em dez minutos.

### Telas

Arrastar em qualquer direção percorre um carrossel circular: para cima ou
para a esquerda avança, para baixo ou para a direita volta, e depois da
última vem a primeira. O botão **BOOT** também avança.

| Tela | O que mostra |
| --- | --- |
| Agora | hora, data, tempo litúrgico, nome do dia e o ofício que a hora pede |
| Bússola | o anel do ano com as festas, a data e os quatro ofícios |
| Leituras | as leituras do dia, por referência |
| Ofícios | os quatro ofícios e a sequência de dias |
| Semana | os próximos seis dias e a contagem para a próxima festa |
| Mais | Contas, Silêncio, Lanterna e Ajustes |

- **Contas**: terço anglicano (33 contas, com a invitatória e as cruciformes)
  ou corda de 33, 50 ou 100 para a Oração de Jesus. BOOT ou um toque conta
  uma; segurar BOOT zera. A contagem sobrevive ao sono.
- **Silêncio**: 5, 10 ou 20 minutos com um sino no começo e outro no fim. A
  tela pode apagar; a bússola acorda sozinha para tocar.
- **Lanterna**: tela branca no brilho máximo por até um minuto.

Dentro das ferramentas e dos ajustes há um "voltar" no topo; arrastar para a
direita e o BOOT fazem o mesmo.

### Botões

| Botão | Gesto | Faz |
| --- | --- | --- |
| PWR | toque | apaga a tela e dorme; dormindo, acorda |
| PWR | segurar seis segundos | desliga a placa pelo gerenciador de energia, mesmo que o firmware tenha travado |
| BOOT | toque | próxima tela; nas Contas, conta uma; dormindo, acorda |
| BOOT | segurar um segundo | volta à tela inicial; nas Contas, zera |
| BOOT | segurar dez segundos | apaga conta, Wi-Fi e ajustes |

### Ajustes

Em Mais › Ajustes, direto na bússola:

| Grupo | Ajuste |
| --- | --- |
| Tela | apaga em 10, 15, 30 ou 60 segundos; brilho baixo, médio ou alto; escurecer à noite; tela inicial (Agora ou Bússola); toque acorda; acesa na tomada |
| Som | ligado ou não; volume; sino às 6h, 12h e 18h |
| Lembretes | um horário para cada ofício; toca só se o ofício ainda não foi orado |
| Rede | redes Wi-Fi guardadas (até cinco) e adicionar outra; sincronizar a cada 2, 4, 6 ou 12 horas, ou só ao acordar; sincronizar agora |
| Aparelho | sobre (versão, bateria, última sincronização, quantas vezes acordou); desligar; desvincular; apagar tudo |

### Bateria

A bússola foi feita para dormir:

- Sem toque por 15 segundos (o padrão), entra em **sono profundo**: tela e
  toque dormindo, rádio desligado, só os botões e um temporizador acordam.
- Acordada por um botão, desenha o dia a partir do que está guardado, sem
  esperar rede. Só então, se o dado tem mais de meia hora, liga o Wi-Fi por
  alguns segundos e atualiza.
- Sozinha, acorda logo depois da meia-noite, no intervalo de sincronização
  escolhido e na hora de cada lembrete, sem acender a tela a não ser para
  tocar.
- Longe de qualquer rede conhecida, não insiste: mostra o que tem e tenta de
  novo mais tarde.
- **Na tomada** ela vira relógio de mesa: tela acesa (mais fraca depois de
  um minuto), atualização a cada cinco minutos. Dá para desligar isso em
  Ajustes.
- Com 10% de bateria, sem sino e sem lanterna no máximo. Quase no fim, avisa
  e desliga a placa para proteger a célula.

A placa 1.75C não tem chip de relógio. A hora vem da resposta da API e é
mantida pelo temporizador do ESP32, que deriva um pouco dormindo; o desvio
medido entre duas sincronizações é descontado no trecho seguinte. Depois de
ficar sem energia, a hora volta na primeira sincronização.

Sem rede, o dia vira sozinho: a API manda os próximos sete dias, e além
deles o anel do ano ainda diz o tempo e a cor. Os ofícios orados aparecem
como estavam na última sincronização.

**Ainda não medido.** O consumo em cada estado foi estimado, não medido; os
números e como medi-los estão no plano. O firmware compila para as duas
placas, mas esta versão ainda não foi gravada nem testada num aparelho.

### Se der errado

| Tela | Causa | O que fazer |
| --- | --- | --- |
| "Assinatura Ordo+ vencida" | A API respondeu `403 PREMIUM_REQUIRED` | Renove no app; a bússola volta sozinha na próxima sincronização |
| "Aparelho desativado" | `403 CLIENT_DISABLED` | Fale com quem mantém o Ordo |
| Volta a pedir código | `401`: o aparelho foi desvinculado no app ou ficou 90 dias sem conectar | Vincule de novo |
| Ponto cinza no topo | O dia na tela não é confirmado pela API há mais de 12 horas | Leve-a para perto de uma rede conhecida, ou adicione uma em Ajustes › Wi-Fi |
| Data em vez de hora | Ainda não sincronizou desde que foi ligada | Sincronize: Ajustes › Sincronizar agora |
| Não acorda | Bateria no fim, ou desligada pelo PWR | Ligue no cabo; aperte PWR |

### Configuração em tempo de compilação

Em `compass/src/app_config.h`, ou por `build_flags`:

| Define | Padrão | Para quê |
| --- | --- | --- |
| `ORDO_API_BASE_URL` | `https://api.caminhoanglicano.com.br/api/v1` | API principal |
| `ORDO_API_FALLBACK_URL` | `https://api.oficio.app/api/v1` | Segundo host, tentado quando o primeiro não responde |
| `ORDO_DEVICE_CLIENT_ID` | `ordo-compass` | Cliente registrado na API |
| `TOUCH_MIRROR_X` / `TOUCH_MIRROR_Y` | `true` | Orientação do toque, se uma revisão da placa diferir |
| `ORDO_DEBUG` | indefinido | Log serial (`ORDO_LOG`); é o que o ambiente `-debug` liga |
| `ORDO_TLS_INSECURE` | indefinido | Só para desenvolvimento contra um host sem certificado válido |

As conexões validam o certificado contra as raízes da Let's Encrypt
(`compass/src/net/root_certs.h`). Se a API mudar de autoridade, acrescente a
raiz nova ali.

### Ver as telas sem a placa

`compass/sim/` desenha todas as telas num computador, com o mesmo LVGL, as
mesmas fontes e o mesmo código de `src/ui` que vai para a placa.

```bash
cd compass
pio pkg install -e amoled175c   # uma vez: baixa o LVGL e o ArduinoJson
make -C sim shots               # PNGs em sim/out/
make -C sim check               # toca em todos os controles com verificação de memória
make -C sim test                # relógio, agenda de acordar, lembretes e o dia sem rede
```

As fontes (Raleway e Newsreader, as do app, com acentos) são geradas por
`compass/tools/make_fonts.sh`.

### Estrutura

```
compass/
├── platformio.ini          ambientes amoled175c, amoled175 e amoled175c-debug
├── partitions/             OTA dupla, NVS, um segundo NVS para o dia, LittleFS
├── include/
│   ├── board_pins.h        pinos por placa
│   └── lv_conf.h           configuração do LVGL 9
├── tools/make_fonts.sh     gera src/ui/fonts
├── sim/                    simulador das telas
└── src/
    ├── main.cpp
    ├── app_config.h        URLs, tempos, brilho
    ├── app/controller.*    acordar, sincronizar, tocar, dormir
    ├── app/schedule.*      quando acordar sozinho e o que tocar (testado em sim/)
    ├── board/              tela (CO5300), toque (CST9217), energia e sono (AXP2101), sino (ES8311)
    ├── sys/                relógio emprestado da API; estado que sobrevive ao sono
    ├── net/                Wi-Fi com várias redes, página de configuração, cliente da API, raízes TLS
    ├── model/today.*       o payload de /device/today, e o dia visto de outra data
    ├── store/settings.*    NVS: token, ajustes, redes, último payload
    └── ui/                 as telas, as fontes e o tema
```

## O app do Miblo Desk

A mesma face, na tela de 240×240 do Miblo Desk: o anel do ano, o dia na cor
do tempo, os quatro ofícios e, alternando, as leituras. Vincula-se com um
código como a bússola, guarda o token no armazenamento selado do Miblo Hub e
só lê. Lembretes opcionais da Oração da Manhã e da Tarde. Detalhes, como
desenvolver e publicar: [`miblo/README.md`](miblo/README.md).

## Licença

Apache 2.0, como o restante do Ordo.
