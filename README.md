# ESP32 Spotify Now Playing

Real-time Spotify "Now Playing" display using an ESP32-2432S028R (Cheap Yellow Display) with album art, scrolling text, progress bar, and RGB LED status indicator.

> **[Leia em Português](#português)** abaixo.

## Hardware

- **ESP32-2432S028R** (Cheap Yellow Display / CYD)
- Built-in **ILI9341 2.8" TFT LCD** 320x240 with resistive touch
- Built-in **RGB LED** for playback status

### Display Configuration (CYD pinout)

| Function | GPIO |
|----------|------|
| MOSI | 13 |
| MISO | 12 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RST | -1 (tied to EN) |
| Backlight | 21 |

### RGB LED

| Color | Meaning | GPIO |
|-------|---------|------|
| Green | Playing | 4 (R), 16 (G), 17 (B) |
| Blue | Paused | |
| Red | Error | |
| Off | Nothing playing | |

## Features

- Wi-Fi via captive portal (WiFiManager) — no hardcoded credentials
- Spotify API polling every ~4 seconds
- Landscape layout: album cover (150x150) on the left, track info on the right
- Scrolling marquee text for long song titles and artist names (synchronized)
- Play/pause icon indicator
- Progress bar with elapsed/total time
- RGB LED changes color based on playback state
- Automatic token renewal (~1h)
- Credentials stored in NVS (survives reboot)
- Handles: nothing playing, paused, network error, rate limit (429)
- Auto-recovers when Spotify is reopened (same track detection)

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

### 3. Flash Firmware

```bash
pip install platformio
python -m platformio run -t upload
```

### 4. Send Credentials via Serial

```bash
python -m platformio device monitor
```

Then type:
```
SET_CREDS:your_client_id:your_client_secret:your_refresh_token
```

### 5. Configure Wi-Fi

On first boot:
1. Connect to **SpotifyDisplay-Setup** AP from your phone
2. Select your Wi-Fi and enter password
3. ESP32 saves and reboots connected

## Serial Commands

| Command | Description |
|---------|-------------|
| `SET_CREDS:id:secret:token` | Save credentials to NVS |
| `RESET_WIFI` | Erase saved Wi-Fi (reopens portal) |
| `STATUS` | Show current state, free RAM |

## PC Simulator

Preview the display layout without hardware:

```bash
cd tools
pip install -r requirements.txt
python display_simulator.py
```

## Project Structure

```
├── platformio.ini              # PlatformIO config (ESP32-2432S028R / CYD)
├── include/
│   ├── secrets.h.example       # Credentials template
│   ├── display_driver.h        # Display abstraction
│   ├── wifi_manager_setup.h    # Wi-Fi captive portal
│   ├── spotify_auth.h          # OAuth management
│   ├── spotify_poller.h        # API polling
│   ├── json_parser.h           # JSON parser with filters
│   └── ui_renderer.h           # UI rendering + marquee
├── src/
│   ├── main.cpp                # State machine + RGB LED
│   ├── display_driver.cpp
│   ├── wifi_manager_setup.cpp
│   ├── spotify_auth.cpp
│   ├── spotify_poller.cpp
│   ├── json_parser.cpp
│   └── ui_renderer.cpp
└── tools/
    ├── get_refresh_token.py    # OAuth helper (run on PC)
    ├── test_api.py             # API test (terminal)
    └── display_simulator.py    # Visual simulator (tkinter)
```

## Libraries

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — SPI display driver
- [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) — Lightweight JPEG decoder
- [ArduinoJson](https://arduinojson.org/) — JSON parser with filters
- [WiFiManager](https://github.com/tzapu/WiFiManager) — Wi-Fi captive portal

---

# Português

## ESP32 Spotify Now Playing

Display em tempo real da música tocando no Spotify usando ESP32-2432S028R (Cheap Yellow Display) com capa do álbum, texto com scroll, barra de progresso e LED RGB indicador de estado.

## Hardware

- **ESP32-2432S028R** (Cheap Yellow Display / CYD)
- Display **ILI9341 2.8" TFT LCD** 320x240 embutido com touch resistivo
- **LED RGB** embutido para indicar estado de reprodução

## Funcionalidades

- Wi-Fi via portal cativo (WiFiManager) — sem credenciais hardcoded
- Polling da API do Spotify a cada ~4 segundos
- Layout paisagem: capa (150x150) à esquerda, info à direita
- Texto com marquee (scroll) para títulos e artistas longos (sincronizados)
- Ícone de play/pause
- Barra de progresso com tempo decorrido/total
- LED RGB muda de cor conforme estado (verde=tocando, azul=pausado, vermelho=erro)
- Renovação automática do token (~1h)
- Credenciais na NVS (sobrevive reboot)
- Trata: nada tocando, pausado, erro de rede, rate limit (429)
- Recupera automaticamente quando o Spotify é reaberto (mesma música)

## Setup

### 1. Criar App no Spotify Developer

1. Acesse [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard)
2. Crie um app
3. Em **Settings** → **Redirect URIs**, adicione: `http://127.0.0.1:8888/callback`
4. Copie **Client ID** e **Client Secret**

### 2. Obter Refresh Token (rodar uma vez no PC)

```bash
cd tools
pip install -r requirements.txt
python get_refresh_token.py
```

### 3. Gravar Firmware

```bash
pip install platformio
python -m platformio run -t upload
```

### 4. Enviar Credenciais via Serial

```bash
python -m platformio device monitor
```

Digite:
```
SET_CREDS:seu_client_id:seu_client_secret:seu_refresh_token
```

### 5. Configurar Wi-Fi

Na primeira execução:
1. Conecte no AP **SpotifyDisplay-Setup** pelo celular
2. Selecione sua rede e digite a senha
3. ESP32 salva e reinicia conectado

## Comandos Serial

| Comando | Descrição |
|---------|-----------|
| `SET_CREDS:id:secret:token` | Grava credenciais na NVS |
| `RESET_WIFI` | Apaga Wi-Fi salvo (reabre portal) |
| `STATUS` | Mostra estado atual, RAM livre |

## Simulador no PC

```bash
cd tools
pip install -r requirements.txt
python display_simulator.py
```

## Licença

MIT
