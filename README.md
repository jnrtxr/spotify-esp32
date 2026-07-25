# ESP32 Spotify Now Playing

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

- Conexão Wi-Fi via portal cativo (WiFiManager) - sem SSID/senha hardcoded
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
3. Em **Settings** → **Redirect URIs**, adicione: `http://localhost:8888/callback`
4. Anote o **Client ID** e **Client Secret**

### 2. Obter Refresh Token (rodar uma vez no PC)

```bash
cd tools
pip install requests
python get_refresh_token.py
```

O script abre o navegador para login, recebe o callback e imprime o `refresh_token`.

Alternativamente, configure via variáveis de ambiente:
```bash
export SPOTIFY_CLIENT_ID=seu_client_id
export SPOTIFY_CLIENT_SECRET=seu_client_secret
python get_refresh_token.py
```

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

As credenciais são salvas na NVS na primeira execução. Depois disso, `secrets.h` não é mais necessário.

### 4. Compilar e Fazer Upload

```bash
# Com PlatformIO CLI
pio run -t upload

# Para abrir o monitor serial
pio device monitor
```

### 5. Configurar Wi-Fi

Na primeira execução (ou se resetar Wi-Fi):
1. O ESP32 cria um Access Point: **SpotifyDisplay-Setup**
2. Conecte pelo celular/PC nesse AP
3. Selecione sua rede Wi-Fi e digite a senha
4. O ESP32 salva e reinicia conectado

## Comandos Serial

| Comando | Descrição |
|---------|-----------|
| `SET_CREDS:id:secret:token` | Grava credenciais na NVS |
| `RESET_WIFI` | Apaga SSID/senha salvos (reabre portal) |
| `STATUS` | Mostra estado atual, RAM livre, etc. |

## Estrutura do Projeto

```
ProjetoSpotify/
├── platformio.ini              # Configuração do PlatformIO
├── include/
│   ├── secrets.h.example       # Template de credenciais
│   ├── display_driver.h        # Abstração do display
│   ├── wifi_manager_setup.h    # Portal cativo Wi-Fi
│   ├── spotify_auth.h          # Gerenciamento OAuth
│   ├── spotify_poller.h        # Polling da API
│   ├── json_parser.h           # Parser JSON com filtros
│   └── ui_renderer.h           # Renderização da UI
├── src/
│   ├── main.cpp                # Máquina de estados principal
│   ├── display_driver.cpp
│   ├── wifi_manager_setup.cpp
│   ├── spotify_auth.cpp
│   ├── spotify_poller.cpp
│   ├── json_parser.cpp
│   └── ui_renderer.cpp
└── tools/
    └── get_refresh_token.py    # Script OAuth para PC
```

## Trocar para Display GC9A01 (Redondo)

1. Em `platformio.ini`, altere os build_flags:
   ```ini
   ; Remova:
   -DST7789_DRIVER=1
   ; Adicione:
   -DGC9A01_DRIVER=1
   ```
2. Ajuste pinos se necessário
3. O resto do código permanece inalterado

## Bibliotecas Utilizadas

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - Driver SPI para displays
- [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) - Decodificador JPEG leve
- [ArduinoJson](https://arduinojson.org/) - Parser JSON com filtros
- [WiFiManager](https://github.com/tzapu/WiFiManager) - Portal cativo Wi-Fi

## Licença

MIT
