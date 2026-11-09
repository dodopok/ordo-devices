# site/ — bussola.oficio.app

O que a bússola procura na internet além da API do Ordo. Duas coisas, no
mesmo projeto da Vercel:

| Caminho | O quê |
| --- | --- |
| `/firmware/manifest.json` e `/firmware/<versão>/<placa>.bin` | A versão mais recente do firmware, para a atualização pelo Wi-Fi. Arquivos estáticos, gerados por `compass/tools/release.py`. |
| `/c/<slug>.jpg` e `/c/<slug>.<idioma>.json` | A foto e o texto da comemoração do dia, lidos do CMS e cortados para o vidro de 466 px. Uma função (`api/c.js`). |

As fotos e os textos **continuam no CMS**. Nada é copiado para a API nem
guardado aqui: a função busca o post pelo `post_slug`, recorta a imagem em
quadrado, reduz para 466 px, grava como JPEG baseline (o único que o
decodificador da placa lê) e devolve. O cache da Vercel guarda a resposta por
um dia.

## Publicar na Vercel

1. Novo projeto a partir deste repositório, com **Root Directory = `site`**.
   Framework: "Other". Sem comando de build.
2. Domínio: `bussola.oficio.app`.
3. Variáveis de ambiente (Production):

   | Variável | Valor |
   | --- | --- |
   | `DEVICE_CONTENT_SECRET` | Um segredo longo e aleatório (`openssl rand -hex 32`). **O mesmo valor** vai na API (Railway), com o mesmo nome. |
   | `CMS_TOKEN` | Um token de leitura do Strapi. |
   | `CMS_BASE_URL` | Opcional. Padrão: `https://cms.caminhoanglicano.com.br`. |

4. Na API, depois de definir `DEVICE_CONTENT_SECRET`, o payload de
   `/device/today` passa a trazer `celebration.content.art` e `.text`. Sem o
   segredo a API não assina nada e a bússola mostra a comemoração só em texto
   curto (nome, epíteto e ano), sem foto.

A bússola valida o certificado contra as raízes da Let's Encrypt (ISRG Root X1
e X2), que é quem emite os certificados da Vercel. Se isso mudar um dia,
acrescente a raiz nova em `compass/src/net/root_certs.h`.

## Por que os caminhos são assinados

Sem assinatura, `/c/...` seria uma API pública e gratuita dos textos do CMS.
Cada caminho leva `t` (a semana em que a API o emitiu) e `s` (HMAC-SHA256 do
nome do arquivo e da semana, com o segredo compartilhado). A API só entrega
caminhos a aparelhos vinculados de contas Ordo+. A função recusa o que não
confere, o que tem mais de uma semana, qualquer parâmetro além de `t` e `s`,
e qualquer nome de arquivo fora do formato. O contrato está em
`docs/DEVICES.md` da estevao-api, seção "Content gateway".

Todos os aparelhos recebem o mesmo caminho durante a semana, então a primeira
bússola paga o custo de ler o CMS e redimensionar; as outras recebem do cache.

## Testar

```bash
cd site
npm install
npm test          # assinatura, recorte, limpeza do texto: sem rede
```

Um caminho assinado, para conferir a função publicada:

```bash
node -e "
const c = require('./api/_content.js');
const file = 'celebration-martin-of-tours.jpg';
const t = c.bucketAt(Date.now());
console.log('https://bussola.oficio.app/c/' + file + '?t=' + t + '&s=' + c.sign(process.env.DEVICE_CONTENT_SECRET, file, t));
"
```

Respostas: `200` com `image/jpeg` ou JSON; `404` quando o post não tem imagem
ou ela é pequena demais (menos de 320 px no menor lado; o cabeçalho
`X-Ordo-Reason` diz qual); `403` assinatura errada ou vencida; `400` pedido
fora do formato; `503` sem `DEVICE_CONTENT_SECRET`; `502` CMS fora do ar.

## Publicar um firmware

```bash
cd compass
# 1. suba ORDO_FIRMWARE_VERSION em platformio.ini
# 2. compile, copie e escreva o manifesto:
tools/release.py build --pt "O que mudou, em uma ou duas frases."
# 3. commit de site/firmware/ e push: a Vercel publica.
```

Uma bússola procura versão nova uma vez por dia, quando sincroniza com a tela
acesa, e em Ajustes › Atualização › Procurar agora. Ela só instala o que o
dono mandar instalar, com a bateria acima de 40% ou no cabo.

### Assinar as versões

Por padrão a bússola aceita um firmware cujo SHA-256 confere com o manifesto,
baixado por HTTPS deste domínio. Quem tomasse conta do projeto na Vercel
poderia então publicar um firmware. Para fechar isso:

```bash
tools/release.py keygen
```

Isso cria uma chave ECDSA. A metade privada fica em
`~/.config/ordo/firmware-signing.pem` (fora do repositório: **faça cópia**);
a pública vai para `compass/include/ota_public_key.h`, que entra no commit. A
partir do firmware compilado com esse arquivo, a bússola recusa qualquer
atualização sem assinatura válida, e `release.py build` passa a assinar.
Perder a chave privada não trava nada de vez: grava-se um firmware novo pelo
cabo.
