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
| [`compass/`](compass/) | Firmware da bússola para a Waveshare ESP32-S3-Touch-AMOLED-1.75 (e 1.75C) | 0.4: chaveiro. A 0.2 roda na 1.75C; a 0.3 (animações, menu novo) e a 0.4 (comemoração com foto, atualização pelo Wi-Fi) compilam e passam no simulador, ainda não gravadas |
| [`site/`](site/) | bussola.oficio.app, na Vercel: o firmware para atualização pelo Wi-Fi e a função que entrega a foto e o texto da comemoração a partir do CMS | escrito e testado contra o CMS; ainda não publicado |
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

A tela escolhida em Ajustes › Tela inicial é a primeira do carrossel e as
outras seguem na ordem de sempre: escolher a Bússola, por exemplo, só leva a
"Agora" para o segundo lugar.

| Tela | O que mostra |
| --- | --- |
| Agora | hora, data, tempo litúrgico, nome do dia e o ofício que a hora pede |
| Comemoração | só nos dias que têm uma: a foto enchendo o vidro, com o nome, o epíteto e o ano. Um toque sobe as palavras (um resumo) e mostra "Ler tudo", que abre o texto inteiro |
| Bússola | o anel do ano com as festas, a data, os quatro ofícios e quantos dias faltam para o próximo tempo |
| Leituras | as leituras do dia, por referência |
| Ofícios | os quatro ofícios, a sequência de dias e os últimos sete dias (um ponto por dia, mais cheio quanto mais ofícios) |
| Semana | os próximos seis dias e a contagem para a próxima festa |
| Mais | quatro blocos: Contas, Silêncio, Lanterna e Ajustes, cada um dizendo como está ("12 de 33", "ainda 14 min") |

No topo de todas elas ficam o Wi-Fi (os arcos dizem o sinal da última rede
em que entrou; pulsam enquanto sincroniza) e a bateria com a porcentagem
(dourada carregando, vermelha abaixo de 10%).

Trocar de tela não apaga nada: a tela nova é desenhada fora de vista e
entra por cima da anterior, como uma cortina, na direção do gesto. Arrastou
para cima, ela sobe de baixo; para o lado, vem do lado; abrir uma ferramenta
abre do meio para fora; voltar fecha das bordas para o meio. A tela anterior
fica acesa até a cortina passar. Só ao acordar a luz sobe do escuro, porque
aí não há nada aceso para cobrir. Ao chegar, cada tela tem a sua entrada: o
marcador do anel percorre o último trecho do ano até hoje, as festas acendem
em volta, os ofícios chegam um a um, as leituras descem em cascata. Dá para
desligar em Ajustes › Animações.

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
| Tela | apaga em 10, 15, 30 ou 60 segundos; brilho baixo, médio ou alto; escurecer à noite; tela inicial (Agora, Bússola, Leituras, Ofícios ou Semana); animações; toque acorda; acesa na tomada |
| Som | ligado ou não; volume; sino às 6h, 12h e 18h |
| Lembretes | horários "do app" (os que você marcou no Ordo, quando há algum) ou "daqui" (um horário para cada ofício, ajustado na bússola); toca só se o ofício ainda não foi orado. Seguindo o app, vem junto o aviso de **sequência**: às 22h, se nenhum ofício foi orado no dia. **Testar aviso** põe a bússola para dormir e a faz tocar um minuto depois: é como ver, sem esperar a noite, que ela acorda sozinha |
| Rede | redes Wi-Fi guardadas (até cinco) e adicionar outra; sincronizar a cada 2, 4, 6 ou 12 horas, ou só ao acordar; sincronizar agora |
| Aparelho | atualização (procurar e instalar um firmware novo); sobre (versão, bateria, última sincronização, quantas vezes acordou, a que horas acorda sozinha, os últimos despertares e até onde cada um chegou, memória interna livre, último erro ao buscar fotos); desligar; desvincular; apagar tudo |

### A comemoração do dia

Nos dias com comemoração a API manda o nome, o epíteto e o ano ("Monge e
Bispo, 397") e dois caminhos assinados para a foto e o texto. A foto e o
texto **moram no CMS do Ordo** e não passam pela API: a bússola os busca em
`bussola.oficio.app` (ver [`site/`](site/)), que lê o CMS e devolve a imagem
já recortada para o vidro (466 px, 20 a 65 KB) e o texto já limpo.

- Busca na mesma hora em que sincroniza, hoje primeiro e depois os sete dias
  seguintes, e guarda no LittleFS: sem rede, a foto do dia já está lá.
- No máximo seis arquivos por sincronização na bateria; o resto vem nas
  próximas. Na prática entra uma foto nova por dia.
- Sem foto no CMS (ou com uma pequena demais), a página aparece só em texto.
- Em dia de jejum ou abstinência, a tela Agora diz qual, sob o nome do dia.

### Atualização pelo Wi-Fi

Em Ajustes › Atualização. A bússola procura versão nova uma vez por dia,
quando sincroniza com a tela acesa, e quando você toca em "Procurar agora".
Havendo uma, mostra o número e o que mudou; **só instala se você mandar**,
com a bateria acima de 40% ou no cabo.

- O firmware novo é gravado na outra metade da memória. O que está rodando
  continua inteiro até o novo provar que liga: se ele não chegar a acender a
  tela, o próximo reinício volta ao anterior sozinho.
- O arquivo só é aceito se o SHA-256 conferir com o manifesto e, se o
  firmware foi compilado com uma chave pública, se a assinatura conferir.
- Como publicar uma versão e como passar a assiná-las:
  [`site/README.md`](site/README.md).

### Bateria

A bússola foi feita para dormir:

- Sem toque por 15 segundos (o padrão), entra em **sono profundo**: tela e
  toque dormindo, rádio desligado, só os botões e um temporizador acordam.
- Acordada por um botão, desenha o dia a partir do que está guardado, sem
  esperar rede. Só então, se o dado tem mais de meia hora, liga o Wi-Fi por
  alguns segundos e atualiza.
- Sozinha, acorda logo depois da meia-noite, no intervalo de sincronização
  escolhido, dez minutos antes de cada lembrete e na hora dele, sem acender
  a tela a não ser para tocar.
- O despertar de dez minutos antes só sincroniza: traz os ofícios já orados
  e acerta o relógio. Na hora do lembrete ela toca com o que tem guardado,
  sem ligar o Wi-Fi: o sino não depende de rede.
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
números e como medi-los estão no plano. A 0.4.0 foi gravada e usada numa
1.75C; a 0.4.2 compila para as duas placas e passa no simulador, mas ainda
não foi gravada num aparelho.

### Se der errado

| Tela | Causa | O que fazer |
| --- | --- | --- |
| "Assinatura Ordo+ vencida" | A API respondeu `403 PREMIUM_REQUIRED` | Renove no app; a bússola volta sozinha na próxima sincronização |
| "Aparelho desativado" | `403 CLIENT_DISABLED` | Fale com quem mantém o Ordo |
| Volta a pedir código | `401`: o aparelho foi desvinculado no app ou ficou 90 dias sem conectar | Vincule de novo |
| Wi-Fi apagado com o ponto vermelho | O dia na tela não é confirmado pela API há mais de 12 horas | Leve-a para perto de uma rede conhecida, ou adicione uma em Ajustes › Wi-Fi |
| Wi-Fi apagado, ponto cinza | Nenhuma rede guardada, ou a última tentativa não achou nenhuma | Ajustes › Wi-Fi |
| Data em vez de hora | Ainda não sincronizou desde que foi ligada | Sincronize: Ajustes › Sincronizar agora |
| Comemoração sem foto | O CMS não tem imagem para ela, a imagem é pequena demais, a foto ainda não foi baixada, ou a API está sem `DEVICE_CONTENT_SECRET` | Espere a próxima sincronização; se continuar, veja `site/README.md` |
| "não deu para procurar agora" na Atualização | Sem Wi-Fi, ou o site não respondeu. A linha pequena embaixo diz o motivo: `sem Wi-Fi`, `HTTP 404`, `conexão -1, TLS -0x2700` (certificado não confiado), `TLS -0x7F00` (faltou memória) | Tente perto do Wi-Fi. Persistindo, o código dessa linha diz onde olhar |
| Aviso no celular e nada na bússola | O horário não chegou à bússola, "Horários" está em "Daqui", ou ela não chegou a tocar | Ajustes › Lembretes mostra os horários que ela conhece (ofícios e sequência) e tem o **Testar aviso**: ela dorme e toca em um minuto. Sobre › "acorda sozinha" mostra o próximo despertar, e "últimos despertares" diz, de cada vez que acordou, a hora, o que estava para tocar e até onde chegou (`22:00 · sozinha · Completas · tocou`; em vermelho, um despertar que parou no meio ou veio depois de uma falha) |
| "Não atualizou: o arquivo não conferiu" | O firmware baixado não bate com o manifesto, ou não está assinado e este firmware exige assinatura | Nada foi mudado na bússola. Publique a versão de novo com `tools/release.py build` |
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
make -C sim movie               # sim/out/ordo.gif: um passeio pelas telas, com as transições
make -C sim check               # toca em todos os controles com verificação de memória
make -C sim test                # relógio, agenda de acordar, lembretes, o rastro dos despertares, as faixas da troca de tela, o dia sem rede, o texto e a foto da comemoração, o manifesto
```

A página da comemoração precisa de uma foto. As de verdade são do CMS, então
o simulador pinta uma de mentira (`sim/make_art.py`) e a passa pelo mesmo
decodificador de JPEG da placa. Para ver as reais, aponte `ORDO_SIM_CONTENT`
para uma pasta com `<slug>.jpg` e `<slug>.pt-BR.json` vindos do site.

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
├── tools/release.py        publica um firmware em site/firmware (e cria a chave de assinatura)
├── sim/                    simulador das telas
└── src/
    ├── main.cpp
    ├── app_config.h        URLs, tempos, brilho
    ├── app/controller.*    acordar, sincronizar, tocar, dormir
    ├── app/schedule.*      quando acordar sozinho e o que tocar (testado em sim/)
    ├── board/              tela (CO5300), toque (CST9217), energia e sono (AXP2101), sino (ES8311)
    ├── sys/                relógio emprestado da API; estado que sobrevive ao sono; o rastro dos últimos despertares
    ├── net/                Wi-Fi com várias redes, página de configuração, cliente da API, raízes TLS
    ├── net/site.*, ota.*   bussola.oficio.app: foto e texto da comemoração, e o firmware novo
    ├── content/art.*       a foto da comemoração, de JPEG para pixels
    ├── model/today.*       o payload de /device/today, e o dia visto de outra data
    ├── model/content.*     o texto da comemoração, o manifesto do firmware, versões
    ├── store/settings.*    NVS: token, ajustes, redes, último payload
    ├── store/content.*     LittleFS: fotos e textos das comemorações
    └── ui/                 as telas, as fontes e o tema; wipe.h: a ordem das faixas de uma troca de tela
```

## O app do Miblo Desk

A mesma face, na tela de 240×240 do Miblo Desk: o anel do ano, o dia na cor
do tempo, os quatro ofícios e, alternando, as leituras. Vincula-se com um
código como a bússola, guarda o token no armazenamento selado do Miblo Hub e
só lê. Lembretes opcionais da Oração da Manhã e da Tarde. Detalhes, como
desenvolver e publicar: [`miblo/README.md`](miblo/README.md).

## Licença

Apache 2.0, como o restante do Ordo.
