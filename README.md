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
| [`compass/`](compass/) | Firmware da bússola para a Waveshare ESP32-S3-Touch-AMOLED-1.75 (e 1.75C) | fase 3, primeira versão |
| `miblo/` | App de página para o Miblo Hub | fase 4, ainda não começou |

## A bússola

Um anel com o ano litúrgico, do Advento (no topo) até a véspera do próximo
Advento, cada tempo na sua cor e as festas principais como pontos. O marcador
aponta para hoje. No centro, a data, o nome do dia e os quatro ofícios, com o
próximo pulsando devagar. Arrastar para cima ou para baixo troca para as
leituras do dia e para o resumo dos ofícios com a sequência de dias. Quando os
quatro ofícios estão feitos, o anel fica dourado.

A bússola só lê. Concluir um ofício continua no app; a bússola reflete isso em
até cinco minutos (ou no instante da virada do dia, no fuso de quem vinculou).

### Placas

| Ambiente | Placa | Diferenças |
| --- | --- | --- |
| `amoled175c` (padrão) | ESP32-S3-Touch-AMOLED-1.75C, com ou sem bateria | 32 MB de flash, reset da tela no GPIO1, reset do toque no GPIO2, botão PWR no GPIO3 |
| `amoled175` | ESP32-S3-Touch-AMOLED-1.75, -B e -G | 16 MB de flash, resets nos GPIO39/40, PWR lido pelo expansor TCA9554 |

Os pinos estão em [`compass/include/board_pins.h`](compass/include/board_pins.h),
conferidos com os repositórios oficiais da Waveshare.

### Compilar e gravar

Precisa do [PlatformIO](https://platformio.org/install/cli). A plataforma
(Arduino-ESP32 3.x, via pioarduino) e as bibliotecas são baixadas na primeira
compilação.

```bash
cd compass
pio run -e amoled175c              # compila
pio run -e amoled175c -t upload    # grava pelo USB-C
pio device monitor -b 115200       # log serial
```

Se a placa não entrar em modo de gravação sozinha: segure BOOT, toque em
RESET (ou religue), solte BOOT e grave.

O GitHub Actions compila os dois ambientes a cada push e anexa `firmware.bin`,
`bootloader.bin` e `partitions.bin` como artefatos.

### Primeira vez

1. **Wi-Fi.** A bússola abre uma rede `Ordo-XXXX`. Conecte o celular nela (há um
   QR na tela) e escolha o seu Wi-Fi na página que abre.
2. **Vínculo.** A bússola pede um código à API e mostra `KQ7-MD4` com um QR. No
   Ordo, em Configurações › Aparelhos › Vincular com código, digite o código,
   ou leia o QR com a câmera do celular. Precisa de Ordo+.
3. Em alguns segundos a bússola recebe o token dela e passa a mostrar o dia.

O token fica no NVS do ESP32 e nunca aparece na tela nem no log. Segurar
**BOOT** por 10 segundos apaga Wi-Fi e vínculo e reinicia.

### Telas e botões

Arrastar para cima ou para baixo percorre bússola → leituras → ofícios →
ajustes. O botão **PWR** desliga a tela; apertar de novo, ou tocar no vidro,
acende.

### Bateria e modo de espera

A tela desliga sozinha depois de um tempo sem toque (padrão: 10 minutos) e o
processador baixa para 80 MHz; o Wi-Fi continua ligado para o dia ficar
atualizado e a tela acender já certa. O tempo é escolhido na tela de ajustes
da própria bússola: nunca, 2, 5, 10 ou 30 minutos. Antes de desligar, um
minuto sem toque já baixa o brilho. Na AMOLED, pixels pretos não gastam, por
isso o fundo é preto e só a cor litúrgica acende.

### Se der errado

| Tela | Causa | O que fazer |
| --- | --- | --- |
| "Assinatura Ordo+ vencida" | A API respondeu `403 PREMIUM_REQUIRED` | Renove no app; a bússola volta sozinha no próximo ciclo |
| "Aparelho desativado" | `403 CLIENT_DISABLED` | Fale com quem mantém o Ordo |
| Volta a pedir código | `401`: o aparelho foi desvinculado no app ou ficou 90 dias sem conectar | Vincule de novo |
| Ponto cinza no topo | Sem rede; mostrando o último dia que conseguiu buscar | Confira o Wi-Fi |

### Configuração em tempo de compilação

Em `compass/src/app_config.h`, ou por `build_flags`:

| Define | Padrão | Para quê |
| --- | --- | --- |
| `ORDO_API_BASE_URL` | `https://api.caminhoanglicano.com.br/api/v1` | API principal |
| `ORDO_API_FALLBACK_URL` | `https://api.oficio.app/api/v1` | Segundo host, tentado quando o primeiro não responde |
| `ORDO_DEVICE_CLIENT_ID` | `ordo-compass` | Cliente registrado na API |
| `TOUCH_MIRROR_X` / `TOUCH_MIRROR_Y` | `true` | Orientação do toque, se uma revisão da placa diferir |
| `ORDO_TLS_INSECURE` | indefinido | Só para desenvolvimento contra um host sem certificado válido |

As conexões validam o certificado contra as raízes da Let's Encrypt
(`compass/src/net/root_certs.h`). Se a API mudar de autoridade, acrescente a
raiz nova ali.

### Estrutura

```
compass/
├── platformio.ini          ambientes amoled175c e amoled175
├── partitions/             tabelas de partição (OTA dupla + LittleFS)
├── include/
│   ├── board_pins.h        pinos por placa
│   └── lv_conf.h           configuração do LVGL 9
└── src/
    ├── main.cpp
    ├── app_config.h        URLs, intervalos, brilho
    ├── app/controller.*    máquina de estados: Wi-Fi → vínculo → leitura
    ├── board/              display (CO5300 + LVGL), toque (CST9217), energia (AXP2101, botões)
    ├── net/                portal Wi-Fi, cliente da API, raízes TLS
    ├── model/today.*       o payload de /device/today como as telas usam
    ├── store/settings.*    NVS: token, ETag, último payload
    └── ui/                 bússola, leituras, ofícios, vínculo e avisos
```

## Licença

Apache 2.0, como o restante do Ordo.
