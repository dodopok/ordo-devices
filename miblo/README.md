# Ordo para o Miblo Desk

Um app de página para a tela de 240×240 do [Miblo Desk](https://miblo.ai), no
formato da Loja de Apps do Desk (`manifest.json` + `main.js` + `page.html`).
O Miblo Hub, no computador, roda o `main.js` numa sandbox e desenha a página
na tela do aparelho.

O app mostra o dia litúrgico da conta do Ordo de quem o instalou: o anel do
ano litúrgico com a cor de cada tempo e as festas como pontos, a data, o nome
do dia (a festa ou o santo quando há), o tempo na sua cor e os quatro ofícios
(Manhã, Meio-dia, Tarde, Completas) marcados conforme a pessoa reza no app;
quando os quatro estão feitos, o anel fica dourado. Com a opção ligada, a
tela alterna com as leituras do dia. Lembretes opcionais da Oração da Manhã e
da Tarde avisam no Miblo, no horário escolhido, se o ofício ainda não foi
rezado.

Como todo aparelho do Ordo, o app **só lê**. Concluir um ofício continua no
app; a tela reflete isso em até cinco minutos. Precisa de **Ordo+**.

## Como se vincula

1. Instale o app no Miblo Hub e abra a tela dele no Desk.
2. O app pede um código à API (`POST /api/v1/device_links`, cliente
   `ordo-miblo`) e mostra `KQ7-MD4`.
3. No Ordo, em Configurações › Aparelhos › Vincular com código, digite o
   código.
4. Em alguns segundos o app recebe o token dele e passa a mostrar o dia.

O token fica em `ctx.data`, o armazenamento selado que o Hub mantém por app:
nunca aparece na tela nem no log. Para desvincular, remova o aparelho em
Configurações › Aparelhos no Ordo (o app volta a pedir um código) ou
desinstale o app.

O fluxo e o payload de `GET /api/v1/device/today` estão em
[`docs/DEVICES.md`](https://github.com/dodopok/estevao-api/blob/main/docs/DEVICES.md)
na API. O app chama só essas três rotas, nos dois hosts declarados em
`manifest.json` (`network`), e nada mais.

## Desenvolver

Precisa do Node 20+ e do kit de https://miblo.ai/sdk (ou do CLI `miblo`):

```bash
# no kit do SDK
node sim.mjs dev   /caminho/para/ordo-devices/miblo/ordo-dia   # simulador no navegador
node sim.mjs check /caminho/para/ordo-devices/miblo/ordo-dia   # as verificações da loja
node sim.mjs pack  /caminho/para/ordo-devices/miblo/ordo-dia   # ordo-dia.zip + ordo-dia-store.zip
```

A página abre sozinha num navegador, sem o Hub, com dados de exemplo:
`page.html#dia`, `#festa`, `#leituras`, `#completo`, `#offline`, `#vinculo`,
`#pedindo`, `#premium`, `#esperando`.

### Estrutura

```
ordo-dia/
├── manifest.json     id, api 6, display "page", configurações, alertas, hosts
├── main.js           run(ctx): vínculo por código, leitura do dia, lembretes
├── page.html         a tela: anel do ano, dia, leituras, ofícios, avisos
├── fonts/            Inter Tight (SIL OFL 1.1, OFL.md ao lado)
└── store/            o que a loja mostra: store.json, icon.png, shot-*.png
```

`store/` não entra no pacote de código; `apps pack` monta com ele o
`ordo-dia-store.zip` que o formulário da loja preenche sozinho.

## Publicar

`node sim.mjs pack` gera os dois zips; envie em https://miblo.ai/apps/enviar
(precisa do Miblo+). A cada versão, suba `version` no `manifest.json` e no
`store/store.json` e acrescente uma entrada em `changelog`.
