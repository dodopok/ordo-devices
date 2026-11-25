<!--
Título: Conventional Commits em inglês, como os commits (regra 7 do AGENTS.md):
`feat(compass): …`, `fix(miblo): …`, `chore(site): …`.
Escreva para quem vai revisar sem ter acompanhado a conversa. Seção que não se
aplica fica com "Nada." em vez de sumir: assim fica claro que ela foi pensada.
Estes comentários não aparecem no PR.
-->

## Por quê

<!-- O problema ou a necessidade, do ponto de vista de quem usa a bússola ou o
Miblo Desk. Link para a issue, a conversa ou a foto da tela. -->

## O que muda

<!-- O que muda na bússola, no app do Miblo ou em `bussola.oficio.app`, e em
qual placa (`amoled175c`, `amoled175`). A versão nova do firmware ou do app,
se houver. Lista curta; o diff já mostra o código. -->

## Como foi feito

<!-- O que o diff não explica: em qual camada a mudança ficou (board / net /
model / ui / app) e por quê, o que foi descartado, os arquivos que valem ser
lidos primeiro. -->

## Riscos

<!--
Uma linha por risco: o que pode dar errado, quem sente, e o que segura. Pense
em:
- aparelhos em campo: firmware publicado chega às bússolas pela atualização
  Wi-Fi e não volta sozinho; o que deixaria uma placa só recuperável pelo cabo
  (OTA que não confirma, partição, boot);
- bateria: Wi-Fi, tela ou processador acordados sem motivo, despertar novo;
- o que toca sem rede: sino e lembretes passando a depender de Wi-Fi ou API;
- memória: heap, PSRAM, LittleFS cheio, NVS num formato que a versão anterior
  (ou a seguinte) não lê;
- as duas placas: pinos, 32 MB ou 16 MB de flash, PWR pelo expansor na 1.75;
- o contrato com a API (`GET /api/v1/device/today`, `docs/DEVICES.md` na
  estevao-api): campo que a API não publica, ou que firmware antigo espera;
- segredos: token do aparelho, `CMS_TOKEN`, `DEVICE_CONTENT_SECRET`, a chave
  privada de assinatura; `ORDO_TLS_INSECURE`;
- hosts: a bússola só fala com a API e com `bussola.oficio.app`
  (`net::sitePathAllowed`); o Miblo só com os hosts do `manifest.json`;
- o gateway de conteúdo: custo na Vercel e no CMS, assinatura dos caminhos
  igual à de `Devices::ContentLinks`;
- Miblo: a revisão da Loja de Apps do Desk.
Se não há risco que valha nomear, diga por quê.
-->

## Publicação

<!--
O merge no `main` publica `site/` na Vercel (`bussola.oficio.app`). Se o PR
traz um `site/firmware/` novo, o merge publica firmware: as bússolas veem a
versão na busca diária e instalam quando o dono manda. O app do Miblo não sai
com o merge. Escreva, em ordem:
- firmware: número subido em `platformio.ini`, gerado por
  `tools/release.py build`, assinado ou não, e se foi gravado numa placa antes
  de publicar;
- variável nova na Vercel (`DEVICE_CONTENT_SECRET`, `CMS_TOKEN`,
  `CMS_BASE_URL`) ou na API, com o mesmo valor dos dois lados quando é o
  segredo compartilhado;
- PR da API que precisa estar em produção antes (campo novo em
  `/device/today`, cliente registrado), com o link;
- Miblo: `node sim.mjs pack` e o envio em https://miblo.ai/apps/enviar;
- o que olhar depois (Ajustes › Sobre na bússola, o manifesto publicado, os
  logs da função na Vercel).
-->

- [ ] Nada além do merge (não publica firmware nem app do Miblo).

## Como desfazer

<!-- Reverter o merge tira o arquivo do site, mas não volta firmware já
instalado: a bússola só aceita versão mais nova. Diga como sair desse estado
(versão de correção com número maior, regravar pelo cabo) e o que fica na
placa (NVS, LittleFS) num formato que a versão anterior não lê. App do Miblo
já enviado à loja só volta com outra versão. -->

## Como testar

<!-- O que foi rodado, com o resultado (`pio run` nos dois ambientes,
`make -C compass/sim check`, `test`, `shots` ou `movie`,
`node sim.mjs check miblo/ordo-dia`, `npm test` em `site/`), e o que foi visto
na placa. Sem a placa à mão, escreva: "compilou, não foi gravado nem testado
na placa". -->

## Telas

<!-- Os PNGs de `make -C compass/sim shots` ou o GIF de
`make -C compass/sim movie`, foto da placa quando houver, a página do Miblo.
"Nada." se não muda tela. -->

## Relacionados

<!-- PRs em outros repositórios (`dodopok/estevao-api#123` para o payload ou a
assinatura, `dodopok/ordo-app#123` para o vínculo e os lembretes,
`dodopok/ordo-cms#123`), a ordem em que entram, e issues. -->

## Checklist

- [ ] `pio run -e amoled175c && pio run -e amoled175` compilam
- [ ] Tela, animação, relógio, agenda de acordar ou modelo mudaram: `make -C compass/sim check`, `test`, `shots` ou `movie`, como pede o `AGENTS.md`, e o resultado olhado
- [ ] Pino mudou: conferido com `waveshareteam/ESP32-S3-Touch-AMOLED-1.75` ou `-1.75C`, com a fonte (esquema ou `pin_config.h`) citada aqui
- [ ] Nenhum segredo no código nem no log; `ORDO_TLS_INSECURE` só em desenvolvimento, nunca o padrão
- [ ] Nada novo deixa Wi-Fi ligado, tela acesa ou processador acordado sem motivo escrito no código, e o sino não espera rede
- [ ] Rede só em `src/net/`; nada em `src/ui` ou `src/model` inclui `board/`, `net/`, `sys/` ou `store/`
- [ ] Só campos que `docs/DEVICES.md` (estevao-api) publica; assinatura dos caminhos igual à da API
- [ ] Firmware a publicar: versão subida em `platformio.ini` e gerado por `tools/release.py build`
- [ ] `miblo/ordo-dia/` mudou: `node sim.mjs check miblo/ordo-dia` termina em "ready to submit"; versão nova em `manifest.json` e `store/store.json`, com entrada no `changelog`
- [ ] `site/` mudou: `npm test` em `site/` verde
- [ ] Gravado e testado na placa, ou o PR diz "compilou, não foi gravado nem testado na placa"
- [ ] README atualizado se mudou comportamento visível ou configuração
