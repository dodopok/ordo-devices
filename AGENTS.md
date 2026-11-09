# Instruções para agentes — ordo-devices

Este arquivo é a fonte única das regras do repositório; `CLAUDE.md` aponta
para cá.

## O que é

Firmware e apps de aparelhos ligados a uma conta do Ordo: a bússola
(`compass/`, ESP32-S3 + AMOLED redonda, PlatformIO + Arduino-ESP32 3.x +
LVGL 9) e o app do Miblo Desk (`miblo/ordo-dia/`, um app de página da Loja de
Apps do Desk: `manifest.json`, `main.js`, `page.html`). Os aparelhos só
leem a API: a única rota que o token de aparelho abre é
`GET /api/v1/device/today`. O contrato e o fluxo de vínculo estão em
`docs/DEVICES.md` na `estevao-api`; não invente campos que a API não publica.

## Regras

1. **Pinos vêm da placa, não da memória.** `compass/include/board_pins.h` foi
   conferido com os repositórios `waveshareteam/ESP32-S3-Touch-AMOLED-1.75` e
   `-1.75C`. Mudança de pino cita a fonte (esquema ou `pin_config.h` oficial).
2. **Dois ambientes, sempre.** Toda mudança compila em `amoled175c` e em
   `amoled175` (`pio run -e amoled175c && pio run -e amoled175`). O CI faz o
   mesmo; não suba o que não compila nos dois. Mudou uma tela? Rode também
   `make -C compass/sim check` (toca em todos os controles com verificação de
   memória) e olhe o resultado de `make -C compass/sim shots`. Mudou o
   relógio, a agenda de acordar ou o modelo? `make -C compass/sim test`.
3. **Nada de segredo no código nem no log.** O token do aparelho mora no NVS
   e não é impresso. `ORDO_TLS_INSECURE` é só para desenvolvimento e nunca é o
   padrão de um ambiente.
4. **Texto na tela é curto e em português por padrão**, com variantes `en` e
   `es` onde o payload traz o idioma (`ui::tr`). Nomes litúrgicos vêm prontos
   da API; não os traduza no firmware.
5. **Rede e placa fora da UI.** Chamadas HTTP vivem em `src/net/`; a UI só
   recebe um `model::Today`, um `ui::Status` e devolve pedidos por
   `ui::Actions`. O controlador em `src/app/` decide quando buscar, tocar e
   dormir. É isso que deixa `compass/sim/` desenhar as telas num computador:
   nada em `src/ui` ou `src/model` inclui `board/`, `net/`, `sys/` ou `store/`.
6. **Bateria primeiro.** A bússola é um chaveiro: o rádio fica desligado a
   não ser por segundos, a tela apaga em segundos e o resto é sono profundo.
   Nada novo deixa o Wi-Fi ligado, a tela acesa ou o processador acordado sem
   um motivo escrito no código. Texto com acento usa as fontes de
   `src/ui/fonts` (geradas por `tools/make_fonts.sh`), nunca a Montserrat do
   LVGL, que só tem ASCII.
7. **Commits em inglês, Conventional Commits** (`feat(compass): …`,
   `fix(miblo): …`), corpo explicando o porquê.
8. **O app do Miblo passa no verificador da loja.** Toda mudança em
   `miblo/ordo-dia/` roda `node sim.mjs check miblo/ordo-dia` com o kit de
   https://miblo.ai/sdk e termina em "ready to submit". Só os hosts de
   `manifest.json` (`network`) aparecem no código e na página; o token mora em
   `ctx.data` e nunca vai para a tela nem para `ctx.log`. Versão nova sobe
   `version` no manifesto e em `store/store.json`, com entrada no `changelog`.
9. **Sem hardware aqui.** Quem não tem a placa à mão diz isso explicitamente
   ao entregar: "compilou, não foi gravado nem testado na placa".

## Fluxo de trabalho

1. Entender a mudança e em qual camada ela cabe (board / net / model / ui / app).
2. Implementar.
3. Bússola: `cd compass && pio run -e amoled175c && pio run -e amoled175`.
   Miblo: `node sim.mjs check miblo/ordo-dia` (e a página aberta no navegador
   com `page.html#dia`, `#leituras`, `#vinculo`...).
4. Atualizar o README se mudou comportamento visível ou configuração.
5. Commitar.
