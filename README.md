# ESP32 Spotify Now Playing

Firmware for ESP32 that displays the currently playing Spotify track in real time, with album art, title, artist, and progress bar on an ST7789 240x240 IPS display.

> **[Leia em Português](#português)** abaixo.

## Hardware

- **ESP32 with PSRAM** (ESP32-WROVER or similar)
- **SPI Display ST7789 240x240** IPS (square)
  - Can be swapped for GC9A01 (round) by changing only `platformio.ini`

### Default Pinout (adjust in `platformio.ini`)

| Display | ESP32 |
|---------|-------|
| MOSI    | GPIO 23 |
| SCLK    | GPIO 18 |
| CS      | GPIO 5  |
| DC      | GPIO 16 |
| RST     | GPIO 17 |
| BL      | GPIO 4  |
| VCC     | 3.3V    |
| GND     | GND     |

## Features

- Wi-Fi connection via captive portal (WiFiManager) — no hardcoded SSID/password
- Spotify API polling every ~4 seconds
- Display: album cover, title (marquee scroll), artist, progress bar
- Progress bar interpolated locally (advances with `millis()` between polls)
- Automatic access_token renewal when expired (~1h)
- Credentials stored in NVS (flash, survives reboot)
- Handles: nothing playing, paused, network error, rate limit (429)
- Serial configuration (write credentials without recompiling)

## Initial Setup

### 1. Create a Spotify Developer App

1. Go to [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard)
2. Create a new app
3. In **Settings** → **Redirect URIs**, add: `http://127.0.0.1:8888/callback`
4. Copy the **Client ID** and **Client Secret**

### 2. Get Refresh Token (run once on PC)

```bash
cd tools
pip install -r requirements.txt
python get_refresh_token.py
```

The script opens the browser for login, receives the callback, and prints the `refresh_token`.

Alternatively, configure via environment variables:
```bash
export SPOTIFY_CLIENT_ID=your_client_id
export SPOTIFY_CLIENT_SECRET=your_client_secret
python get_refresh_token.py
```

### 3. Flash Credentials to ESP32

**Option A: Via Serial (recommended)**

After flashing the firmware, open Serial Monitor (115200 baud) and send:
```
SET_CREDS:your_client_id:your_client_secret:your_refresh_token
```

**Option B: Via secrets.h (at compile time)**

1. Copy `include/secrets.h.example` to `include/secrets.h`
2. Fill in your credentials
3. Compile and upload

Credentials are saved to NVS on first run. After that, `secrets.h` is no longer needed.

### 4. Compile and Upload

```bash
# With PlatformIO CLI
pio run -t upload

# Open serial monitor
pio device monitor
```

### 5. Configure Wi-Fi

On first boot (or after Wi-Fi reset):
1. The ESP32 creates an Access Point: **SpotifyDisplay-Setup**
2. Connect to it from your phone/PC
3. A captive portal opens — select your Wi-Fi and enter the password
4. The ESP32 saves the credentials and reboots connected

## Serial Commands

| Command | Description |
|---------|-------------|
| `SET_CREDS:id:secret:token` | Save credentials to NVS |
| `RESET_WIFI` | Erase saved SSID/password (reopens portal) |
| `STATUS` | Show current state, free RAM, etc. |

## PC Simulator

Test the display layout without hardware:

```bash
cd tools
pip install -r requirements.txt
python display_simulator.py
```

Opens a window replicating the 240x240 display with real-time Spotify data.

## Project Structure

```
ProjetoSpotify/
├── platformio.ini              # PlatformIO configuration
├── include/
│   ├── secrets.h.example       # Credentials template
│   ├── display_driver.h        # Display abstraction
│   ├── wifi_manager_setup.h    # Wi-Fi captive portal
│   ├── spotify_auth.h          # OAuth management
│   ├── spotify_poller.h        # API polling
│   ├── json_parser.h           # JSON parser with filters
│   └── ui_renderer.h           # UI rendering
├── src/
│   ├── main.cpp                # Main state machine
│   ├── display_driver.cpp
│   ├── wifi_manager_setup.cpp
│   ├── spotify_auth.cpp
│   ├── spotify_poller.cpp
│   ├── json_parser.cpp
│   └── ui_renderer.cpp
└── tools/
    ├── get_refresh_token.py    # OAuth script for PC
    ├── test_api.py             # API test (terminal)
    └── display_simulator.py    # Visual display simulator
```

## Switching to GC9A01 (Round Display)

1. In `platformio.ini`, change build_flags:
   ```ini
   # Remove:
   -DST7789_DRIVER=1
   # Add:
   -DGC9A01_DRIVER=1
   ```
2. Adjust pins if needed
3. The rest of the code remains unchanged

## Libraries Used

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — SPI display driver
- [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) — Lightweight JPEG decoder
- [ArduinoJson](https://arduinojson.org/) — JSON parser with filters
- [WiFiManager](https://github.com/tzapu/WiFiManager) — Wi-Fi captive portal

---

# Português

## ESP32 Spotify Now Playing

Firmware para ESP32 que mostra em tempo real a música tocando no Spotify, com capa do álbum, título, artista e barra de progresso.

## Hardware

- **ESP32 com PSRAM** (ESP32-WROVER ou similar)
- **Display SPI ST7789 240x240** IPS (quadrado)
  - Pode ser trocado por GC9A01 (redondo) alterando apenas o `platformio.ini`

### Pinagem padrão (ajuste em `platformio.ini`)

| Display | ESP32 |
|---------|-------|
| MOSI    | GPIO 23 |
| SCLK    | GPIO 18 |
| CS      | GPIO 5  |
| DC      | GPIO 16 |
| RST     | GPIO 17 |
| BL      | GPIO 4  |
| VCC     | 3.3V    |
| GND     | GND     |

## Funcionalidades

- Conexão Wi-Fi via portal cativo (WiFiManager) — sem SSID/senha hardcoded
- Polling da API do Spotify a cada ~4 segundos
- Exibição: capa do álbum, título (marquee scroll), artista, barra de progresso
- Barra de progresso interpolada localmente (avança com `millis()` entre polls)
- Renovação automática do access_token quando expira (~1h)
- Armazenamento de credenciais na NVS (flash, sobrevive a reboot)
- Tratamento de: nada tocando, pausado, erro de rede, rate limit (429)
- Configuração via Serial (gravar credenciais sem recompilar)

## Setup Inicial

### 1. Criar App no Spotify Developer

1. Acesse [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard)
2. Crie um novo app
3. Em **Settings** → **Redirect URIs**, adicione: `http://127.0.0.1:8888/callback`
4. Anote o **Client ID** e **Client Secret**

### 2. Obter Refresh Token (rodar uma vez no PC)

```bash
cd tools
pip install -r requirements.txt
python get_refresh_token.py
```

O script abre o navegador para login, recebe o callback e imprime o `refresh_token`.

### 3. Gravar Credenciais na ESP32

**Opção A: Via Serial (recomendado)**

Após flashear o firmware, abra o Monitor Serial (115200 baud) e envie:
```
SET_CREDS:seu_client_id:seu_client_secret:seu_refresh_token
```

**Opção B: Via secrets.h (na compilação)**

1. Copie `include/secrets.h.example` para `include/secrets.h`
2. Preencha com suas credenciais
3. Compile e faça upload

### 4. Compilar e Fazer Upload

```bash
pio run -t upload
pio device monitor
```

### 5. Configurar Wi-Fi

Na primeira execução (ou se resetar Wi-Fi):
1. O ESP32 cria um Access Point: **SpotifyDisplay-Setup**
2. Conecte pelo celular/PC nesse AP
3. Um portal cativo abre — selecione sua rede e digite a senha
4. O ESP32 salva e reinicia conectado

## Comandos Serial

| Comando | Descrição |
|---------|-----------|
| `SET_CREDS:id:secret:token` | Grava credenciais na NVS |
| `RESET_WIFI` | Apaga SSID/senha salvos (reabre portal) |
| `STATUS` | Mostra estado atual, RAM livre, etc. |

## Simulador no PC

Teste o layout do display sem hardware:

```bash
cd tools
pip install -r requirements.txt
python display_simulator.py
```

Abre uma janela replicando o display 240x240 com dados reais do Spotify.

## Licença

MIT
