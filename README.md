# Dashboard de Controle — Estufa Automatizada (ESP32-CAM)

Painel web hospedado diretamente na ESP32-CAM para monitorar e controlar
uma estufa automatizada: temperatura/umidade do ar (DHT11), umidade do
solo (2 sensores), bomba de irrigação, fita de LED endereçável em tom
roxo/violeta (aproximação visual de UV — **não é UV-C real, sem efeito
germicida**) e câmera ao vivo.

## Arquivos

- `estufa_esp32cam.ino` — firmware principal (WiFi, sensores, relés, servidor)
- `secrets.h.example` — modelo de credenciais (copie para `secrets.h` e preencha)
- `secrets.h` — suas credenciais reais (local, **não vai para o Git**)
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
- Módulo relé (ao menos 1 canal, 5V, com optoacoplador) — aciona a bomba
- Bomba de irrigação 5V/12V (conforme o relé/fonte)
- Fita de LED endereçável **WS2812B** ("Neopixel") — luz roxa/violeta
- Fonte externa 5V (mínimo 2A) — **não alimente a bomba nem a fita de LED
  pelo pino 5V da ESP32-CAM** (a fita sozinha já pode passar de 1A conforme
  o número de LEDs)

### Por que o ADS1115?

A ESP32-CAM já usa quase todos os GPIOs para a câmera. Os pinos analógicos
que sobram pertencem ao **ADC2**, que **não funciona enquanto o WiFi está
ativo** (limitação do chip, não do código). Por isso os sensores de solo
são lidos por um ADS1115 externo (comunica via I2C, sem esse conflito).

## Pinagem usada (GPIOs livres na ESP32-CAM)

⚠️ Algumas placas ESP32-CAM (dependendo do fabricante) **não expõem o
GPIO14** no header, mesmo ele existindo no chip. Confira o silk-screen da
sua placa antes de fiar — a pinagem abaixo já foi ajustada para uma
placa que só disponibiliza `IO12, IO13, IO15, IO2, IO4, IO16, IO0` nos
conectores laterais (sem IO14 separado).

| Sinal                    | GPIO | Observação |
|---------------------------|------|------------|
| DHT11 (data)              | 4    | pisca rapidinho o LED de flash on-board a cada leitura (ignorar) |
| I2C SDA (ADS1115)         | 13   | livre, sem restrições |
| I2C SCL (ADS1115)         | 15   | tem pull-up interno, sem problema |
| Relé bomba                | 2    | também liga o LED vermelho pequeno on-board (ignorar) |
| Fita WS2812B (DATA)       | 12   | **pino de strapping**: veja aviso abaixo |

Escolhemos DHT11 no GPIO4 (em vez do relé) de propósito: o DHT11 só
"toca" o pino por uns milissegundos a cada 2s, então o LED de flash só
pisca rapidamente. Se fosse o relé nesse pino, o LED ficaria aceso ou
apagado fixo por longos períodos — bem mais incômodo visualmente. O LED
em si **não tem como ser desligado por software**, é fisicamente ligado
a esse GPIO; só cobrindo com fita isolante ou removendo fisicamente.

⚠️ **GPIO 12**: define a tensão do flash durante o boot. Se algo externo
puxar esse pino para nível ALTO no instante do boot, a placa pode falhar
ao iniciar. Como aqui é a ESP32-CAM quem *envia* dados pra fita (não o
contrário), o risco é baixo, mas se a placa não ligar depois de conectar
a fita, adicione um resistor de pull-down de 10kΩ entre GPIO12 e GND.

⚠️ **GPIO 2**: também é pino de strapping, mas só importa durante o modo
de gravação (GPIO0 no GND). Se o upload falhar de forma esquisita com o
relé conectado, desconecte-o temporariamente, grave o firmware, e
reconecte depois.

Pinos evitados de propósito: `0,1,3,16` (usados pela câmera, boot ou
PSRAM) e `1,3` (U0T/U0R, seriais de programação).

### Fiação da fita WS2812B

- **+5V** da fita → fonte externa 5V (não tirar do pino 5V da ESP32-CAM
  se a fita tiver mais que ~8-10 LEDs)
- **GND** da fita → GND comum com a ESP32-CAM **e** com a fonte externa
  (todos os GNDs precisam estar juntos)
- **DATA** da fita → GPIO12 da ESP32-CAM — idealmente com um resistor de
  ~470Ω em série, logo na entrada da fita
- Recomendado: capacitor de 1000µF entre +5V e GND bem no início da fita
  (filtra picos de corrente ao ligar)
- O sinal de dados da ESP32-CAM é 3.3V; a maioria das fitas WS2812B aceita
  isso em fiações curtas. Para maior confiabilidade (fiação longa,
  interferência), use um level shifter (ex.: 74AHCT125) entre o GPIO12 e
  o DATA da fita.

## Bibliotecas (Arduino IDE → Library Manager)

- `DHT sensor library` (Adafruit)
- `Adafruit Unified Sensor`
- `Adafruit ADS1X15`
- `Adafruit NeoPixel`

## Configuração da placa

- Board: **AI Thinker ESP32-CAM**
- Upload Speed: 115200
- Para gravar: ligue GPIO0 ao GND antes de energizar (modo de gravação),
  remova a ligação e reinicie após o upload.

## Antes de compilar

As credenciais (Wi-Fi e login do painel) ficam num arquivo `secrets.h`
separado, que **não é versionado no Git** — assim sua senha real nunca
vai parar num repositório público.

1. Copie `secrets.h.example` para um novo arquivo chamado `secrets.h`,
   na mesma pasta.
2. Edite o `secrets.h` (não o `.example`) com seus valores reais:

```cpp
const char *WIFI_SSID     = "SEU_WIFI";
const char *WIFI_PASSWORD = "SUA_SENHA";

const char *AUTH_USER = "estufa";
const char *AUTH_PASS = "troque-esta-senha";
```

`AUTH_USER`/`AUTH_PASS` são o login do painel (dashboard e stream de
vídeo) — essa autenticação existe porque, com o acesso remoto (túnel,
seção abaixo), o painel controla atuadores reais (bomba/relé) e fica
alcançável pela internet. Nunca deixe a senha padrão se for expor a
placa fora da rede local, e nunca commite o `secrets.h` (o `.gitignore`
já cuida disso, mas confira antes de dar `git add`).

Ajuste também o número de LEDs da sua fita e, se quiser, a cor/brilho:

```cpp
#define NUM_LEDS 30
const uint32_t UV_COLOR = Adafruit_NeoPixel::Color(148, 0, 211); // violeta
const uint8_t UV_BRIGHTNESS = 120; // 0-255
```

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

- A fita de LED WS2812B roxa/violeta é **luz visível comum**, não UV-C —
  não desinfeta nem substitui uma lâmpada germicida real. Se no futuro
  quiser UV-C de verdade, isso exige lâmpada/LED UV-C dedicado, com
  cuidados específicos de exposição (pele/olhos) que uma fita RGB não tem.
- Mantenha a ESP32-CAM, o ADS1115 e o relé longe de respingos de água e
  umidade da irrigação — use um invólucro e, se possível, isole a bomba
  em um circuito separado da eletrônica de controle.
- Ao ligar a fita pela primeira vez, teste com poucos LEDs/baixo brilho:
  uma fita inteira em brilho alto pode exigir mais corrente do que a
  fonte 5V suporta.
