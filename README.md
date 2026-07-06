# Dashboard de Controle — Estufa Automatizada (ESP32-CAM)

Painel web hospedado diretamente na ESP32-CAM para monitorar e controlar
uma estufa automatizada: temperatura/umidade do ar (DHT11), umidade do
solo (2 sensores), bomba de irrigação, relé de luz UV e câmera ao vivo.

## Arquivos

- `estufa_esp32cam.ino` — firmware principal (WiFi, sensores, relés, servidor)
- `camera_pins.h` — pinagem da câmera (padrão AI-Thinker, não mexer)
- `stream_server.h` — servidor de vídeo MJPEG (porta 81)
- `dashboard_html.h` — dashboard **real**, embutido no firmware, com dados
  ao vivo da própria ESP32-CAM (é o que roda de fato na placa)
- `dashboard_demo.html` — dashboard **simulado** (dados fictícios gerados no
  navegador), independente de hardware — útil para apresentações e para
  testar o layout sem precisar da estufa montada

## Hardware

- ESP32-CAM (módulo AI-Thinker)
- Sensor DHT11
- 2x sensor de umidade do solo (capacitivo, recomendado — não enferruja)
- Módulo ADS1115 (conversor A/D via I2C) — **necessário**, veja o motivo abaixo
- Módulo relé 2 canais (5V, com optoacoplador)
- Bomba de irrigação 5V/12V (conforme o relé/fonte)
- Relé/módulo de luz UV
- Fonte externa 5V (mínimo 2A) — **não alimente a bomba pelo pino 5V da ESP32-CAM**

### Por que o ADS1115?

A ESP32-CAM já usa quase todos os GPIOs para a câmera. Os pinos analógicos
que sobram pertencem ao **ADC2**, que **não funciona enquanto o WiFi está
ativo** (limitação do chip, não do código). Por isso os sensores de solo
são lidos por um ADS1115 externo (comunica via I2C, sem esse conflito).

## Pinagem usada (GPIOs livres na ESP32-CAM)

| Sinal                | GPIO | Observação |
|-----------------------|------|------------|
| DHT11 (data)          | 13   | livre, sem restrições |
| I2C SDA (ADS1115)     | 14   | livre |
| I2C SCL (ADS1115)     | 15   | tem pull-up interno, sem problema |
| Relé bomba            | 2    | também liga o LED vermelho on-board (ignorar) |
| Relé UV               | 12   | **pino de strapping**: veja aviso abaixo |

⚠️ **GPIO 12**: define a tensão do flash durante o boot. Se algo externo
puxar esse pino para nível ALTO no instante do boot, a placa pode falhar
ao iniciar. A maioria dos módulos de relé mantém a entrada em repouso
compatível, mas se sua placa não ligar após conectar o relé de UV,
adicione um resistor de pull-down de 10kΩ entre GPIO12 e GND, ou troque
esse relé para o GPIO 4 (só que o GPIO 4 também aciona o LED de flash
branco on-board — vai piscar junto com o relé).

Pinos evitados de propósito: `0,1,3,4,16` (usados pela câmera, boot, UART
ou PSRAM).

## Bibliotecas (Arduino IDE → Library Manager)

- `DHT sensor library` (Adafruit)
- `Adafruit Unified Sensor`
- `Adafruit ADS1X15`

## Configuração da placa

- Board: **AI Thinker ESP32-CAM**
- Upload Speed: 115200
- Para gravar: ligue GPIO0 ao GND antes de energizar (modo de gravação),
  remova a ligação e reinicie após o upload.

## Antes de compilar

Edite no topo do `.ino`:

```cpp
const char *WIFI_SSID     = "SEU_WIFI";
const char *WIFI_PASSWORD = "SUA_SENHA";
```

E troque a senha padrão de acesso ao painel (usada tanto no dashboard
quanto no stream de vídeo):

```cpp
const char *AUTH_USER = "estufa";
const char *AUTH_PASS = "troque-esta-senha";
```

Essa autenticação existe porque, com o acesso remoto (túnel, seção
abaixo), o painel controla atuadores reais (bomba/relé) e fica alcançável
pela internet — nunca deixe a senha padrão se for expor a placa fora da
rede local.

## Calibração dos sensores de solo

Os valores `SOIL1_DRY/WET` e `SOIL2_DRY/WET` são brutos do ADS1115 e variam
por sensor. Para calibrar:

1. Grave o firmware e abra o Monitor Serial.
2. Adicione temporariamente `Serial.println(raw1);` dentro de `readSensors()`.
3. Anote o valor com o sensor **seco no ar** → use como `..._DRY`.
4. Anote o valor com o sensor **em água** → use como `..._WET`.
5. Reflash com os valores corretos.

## Acessando o dashboard

Após o boot, veja o IP impresso no Monitor Serial, ou acesse:

```
http://estufa.local/
```

(mDNS funciona na mesma rede local; em alguns roteadores/Android pode ser
necessário usar o IP direto em vez do nome `.local`.)

- Dashboard: porta 80 (`/`)
- Vídeo ao vivo: porta 81 (`/stream`)

## Acesso remoto (fora da rede da estufa)

A ESP32-CAM só existe na rede Wi-Fi local dela — para ver os dados reais
de qualquer lugar (fora dessa rede), é preciso um **túnel** que ligue essa
rede local à internet. A própria ESP32 não roda esse túnel sozinha: é
necessário um computador sempre ligado, **na mesma rede da estufa**, para
fazer essa ponte. Usamos o [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/)
(`cloudflared`), que é gratuito e não exige abrir portas no roteador.

Como o dashboard e o stream de vídeo são dois serviços em portas
diferentes (80 e 81), cada um recebe seu próprio túnel/URL.

### Passo 1 — descubra o IP da ESP32-CAM

Veja no Monitor Serial ao ligar a placa, ou use `http://estufa.local` na
mesma rede. Anote o IP (ex.: `192.168.1.42`).

### Passo 2 — instale o `cloudflared` no computador-ponte

**macOS** (com [Homebrew](https://brew.sh)):
```bash
brew install cloudflared
```

**Raspberry Pi / Linux (Debian/Ubuntu)**:
```bash
curl -L --output cloudflared.deb https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-arm64.deb
sudo dpkg -i cloudflared.deb
```
(troque `arm64` por `amd64` se for um mini PC/NAS x86)

### Passo 3 — abra os dois túneis

Em dois terminais (ou duas abas), com o IP da ESP32-CAM do passo 1:

```bash
cloudflared tunnel --url http://192.168.1.42:80   # dashboard
cloudflared tunnel --url http://192.168.1.42:81   # stream da câmera
```

Cada comando imprime uma URL própria, tipo:
```
https://algo-aleatorio-1.trycloudflare.com   → dashboard
https://algo-aleatorio-2.trycloudflare.com   → stream
```

⚠️ Essas URLs de "quick tunnel" são temporárias: mudam toda vez que você
reinicia o `cloudflared`. Para um link fixo permanente, é preciso um
**túnel nomeado** vinculado a um domínio seu no Cloudflare (grátis, mas
exige ter/registrar um domínio) — me avise se quiser esse próximo passo.

### Passo 4 — configure o acesso no dashboard

Abra a URL do **dashboard** (a primeira). No primeiro acesso ele já abre
a caixa "Acesso ao painel" — preencha:

- **Usuário/Senha**: os mesmos definidos em `AUTH_USER`/`AUTH_PASS` no firmware
- **Endereço do stream da câmera**: cole a URL do **segundo** túnel (a do stream)

Isso fica salvo só no navegador (localStorage). Se o painel e a câmera
estivessem no mesmo endereço (uso local, sem túnel), esse campo pode
ficar em branco.

### Manter o túnel sempre ligado

Rodar os comandos acima manualmente só funciona enquanto o terminal
estiver aberto. Para deixar rodando 24/7 no dispositivo dedicado:

- **Raspberry Pi/Linux**: registre como serviço com
  `sudo cloudflared service install` (usa um túnel nomeado, requer conta
  Cloudflare) ou rode em `screen`/`tmux` para túneis rápidos.
- **Mac**: use `brew services start cloudflared` (mesma observação sobre
  túnel nomeado) ou mantenha o Terminal aberto para uso pontual.

## Comportamento da irrigação automática

O toggle "Irrigação automática" no dashboard liga a bomba por 5s sempre
que o solo mais seco dos dois sensores cair abaixo do limite ajustável
(padrão 30%), respeitando um intervalo mínimo de 60s entre ciclos para
evitar encharcar a terra. Ligar a bomba manualmente desativa o modo
automático até ser reativado pelo próprio toggle.

## Avisos de segurança

- Se a luz UV for uma lâmpada de rede elétrica (110/220V), o relé estará
  chaveando tensão perigosa: use um módulo de relé apropriado, isole os
  contatos, e **nunca toque nas conexões com a placa energizada**. Prefira
  fitas/lâmpadas UV de baixa tensão (5V/12V) sempre que possível.
- Mantenha a ESP32-CAM, o ADS1115 e os relés longe de respingos de água
  e umidade da irrigação — use um invólucro e, se possível, isole a bomba
  em um circuito separado da eletrônica de controle.
- Radiação UV é nociva à pele e aos olhos: posicione a lâmpada de forma
  que não haja exposição direta de pessoas/animais na estufa.
