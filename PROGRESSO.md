# Progresso — Estufa Automatizada (ESP32-CAM)

Última atualização: 2026-07-06, ~23h30. Este arquivo é só pra retomar de
onde paramos — não é documentação final do projeto (isso é o README.md).

## Onde as coisas estão

- **Firmware**: `/Users/apple/Desktop/Claude Code/PROJETOS/estufa_esp32cam/`
  — também em https://github.com/Prof-Diego-Prado/estufa-firmware
- **Dashboard simulado (GitHub Pages)**: `/Users/apple/Desktop/Claude Code/PROJETOS/estufa-painel/`
  — publicado em https://prof-diego-prado.github.io/estufa-painel/
  — repositório: https://github.com/Prof-Diego-Prado/estufa-painel
- Os dois repositórios estão com tudo commitado e enviado (nada pendente).

## Hardware montado até agora

Placa: ESP32-CAM AI-Thinker (marca "RL Eletrônica"), com uma base
programadora USB (micro-USB, chip CH340) — sem FTDI separado.

Pinagem atual (só o que já está no firmware; sensores ainda não
fisicamente conectados, exceto testes pontuais):

| Componente | GPIO |
|---|---|
| DHT11 (data) | 4 |
| ADS1115 SDA | 13 |
| ADS1115 SCL | 15 |
| Relé da bomba | 2 |
| Fita WS2812B (DATA, 75 LEDs) | 12 |

Essa placa específica **não expõe o GPIO14** no header — por isso a
pinagem é diferente da "padrão" documentada em tutoriais genéricos.

## Credenciais e configuração

Ficam em `secrets.h` (não versionado, veja `secrets.h.example` pro
modelo). Se for mexer amanhã, confira esse arquivo — ele tem:
- SSID/senha do Wi-Fi atual (rede de casa, "Quartos")
- Usuário/senha do painel (`AUTH_USER`/`AUTH_PASS`)

## O que já está funcionando

- Firmware compila e grava (Arduino IDE 2.3.10, board "AI Thinker
  ESP32-CAM", pacote esp32 by Espressif Systems instalado)
- Conecta no Wi-Fi de casa e pega IP normalmente (`192.168.0.27` na
  última sessão — pode mudar)
- Dashboard local abre e autentica certinho em `http://<IP>/`
  (`WiFi.setSleep(false)` resolveu lentidão/timeout que tínhamos antes)
- `/data` retorna leituras (mesmo sem sensores conectados, retorna
  valores neutros sem travar — corrigido pra não travar o I2C quando o
  ADS1115 não está plugado)
- Túnel Cloudflare (`cloudflared`) funcionando, expõe o stream da
  câmera publicamente. **Esse túnel só existe enquanto o processo
  `cloudflared` continuar rodando neste Mac** — se fechar o terminal ou
  reiniciar o Mac, a URL cai e precisa gerar uma nova (`cloudflared
  tunnel --url http://<IP-da-ESP32>:81`, README tem o passo a passo)
- Testei via `curl` direto (Bash) que o stream entrega dados normalmente
  tanto local quanto pelo túnel — ou seja, o servidor da câmera em si
  funciona

## O que NÃO está funcionando ainda (retomar por aqui amanhã)

**A câmera real não aparece no dashboard do GitHub Pages
(`estufa-painel`)**, mesmo depois de resolver, em sequência:
1. Credenciais embutidas na URL bloqueadas pelo navegador → trocado por
   busca via `fetch()` com cabeçalho `Authorization`
2. Popup nativo de login bloqueado por proteção anti-rastreamento
   cross-site → trocado por decodificar os frames JPEG manualmente
   (procura marcadores `FFD8`/`FFD9`) e mostrar via Blob URL
3. Preflight CORS (`OPTIONS`) não respondido pelo firmware → adicionado
   handler `OPTIONS` no `stream_server.h`

Depois desse 3º ajuste, o firmware foi gravado de novo, mas a câmera
**ainda não carregou** no dashboard online. Não sabemos ainda se é:
- Firmware novo não gravou de verdade (confirmar Serial Monitor)
- Túnel morreu nesse meio tempo (o `cloudflared` pode ter caído)
- Ainda falta algum cabeçalho CORS na resposta real do GET (só
  adicionamos no preflight OPTIONS — conferir se a resposta principal
  do `stream_handler` também precisa de `Access-Control-Allow-Headers`)
- Outro erro específico do navegador (abrir o Console do DevTools ao
  tentar carregar a câmera real vai mostrar o erro exato — ainda não
  fizemos isso, seria o próximo passo mais direto)

**Próximo passo sugerido**: abrir o Console do navegador (F12 /
Cmd+Option+I no Chrome/Safari) na página do GitHub Pages, tentar
conectar a câmera real, e ver a mensagem de erro exata que aparece —
isso vai dizer exatamente qual é o problema, em vez de continuar
tentando às cegas.

## Outras pendências conhecidas

- Sensores (DHT11, ADS1115, sensores de solo, relé, fita) ainda não
  estão fisicamente conectados/testados com hardware real — só a placa
  nua foi testada até agora
- Calibração dos sensores de solo (`SOIL1_DRY/WET`, `SOIL2_DRY/WET`) não
  foi feita (valores são placeholder)
- Pump relay physical wiring, alimentação da fita WS2812B (fonte externa
  5V) e alimentação da bomba — ver seção de alimentação no README.md
- Túnel usado é do tipo "quick tunnel" (URL temporária) — se quiser um
  endereço fixo permanente, precisa de um túnel nomeado + domínio próprio
  (não configurado ainda)
