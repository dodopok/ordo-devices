# ordo-devices

As instruções deste repositório vivem em **[AGENTS.md](AGENTS.md)**. Leia esse
arquivo antes de mexer no código.

Resumo do que é obrigatório:

1. Pinos conferidos com os repositórios oficiais da Waveshare, com a fonte citada.
2. Toda mudança compila nos dois ambientes: `pio run -e amoled175c && pio run -e amoled175`.
3. Nenhum segredo no código ou no log; `ORDO_TLS_INSECURE` só em desenvolvimento.
4. Commits em inglês, Conventional Commits.

Regra nova? Edite o `AGENTS.md`, não este arquivo.
