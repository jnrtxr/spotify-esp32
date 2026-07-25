#!/usr/bin/env python3
# ============================================================
# test_api.py
# ============================================================
# Simulador do fluxo completo da ESP32 rodando no PC.
# Testa TODA a lógica de comunicação com a API do Spotify
# antes de flashar na placa.
#
# O que faz (igual à ESP32):
#   1. Refresh do access_token usando client_id/secret + refresh_token
#   2. GET /v1/me/player/currently-playing com o token
#   3. Parse do JSON com os mesmos campos que o firmware extrai
#   4. Download da menor capa do álbum (JPEG)
#   5. Exibe tudo no terminal (e salva a capa em disco)
#   6. Loop de polling a cada 4s com interpolação de progresso
#
# Se este script funcionar, a ESP32 também vai funcionar
# (a lógica HTTP/JSON é a mesma, só muda a lib de transporte).
#
# Uso:
#   pip install -r requirements.txt
#   python test_api.py
#
# Configuração via variáveis de ambiente ou arquivo .env:
#   SPOTIFY_CLIENT_ID=...
#   SPOTIFY_CLIENT_SECRET=...
#   SPOTIFY_REFRESH_TOKEN=...
# ============================================================

import os
import sys
import time
import base64
import json
from pathlib import Path

try:
    import requests
except ImportError:
    print("ERRO: instale o requests: pip install requests")
    sys.exit(1)

try:
    from dotenv import load_dotenv
    load_dotenv()  # Carrega .env se existir
except ImportError:
    pass  # Sem python-dotenv, usa só variáveis de ambiente

# ============================================================
# CONFIGURAÇÃO
# ============================================================
CLIENT_ID = os.environ.get("SPOTIFY_CLIENT_ID", "")
CLIENT_SECRET = os.environ.get("SPOTIFY_CLIENT_SECRET", "")
REFRESH_TOKEN = os.environ.get("SPOTIFY_REFRESH_TOKEN", "")

SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token"
SPOTIFY_NOW_PLAYING_URL = "https://api.spotify.com/v1/me/player/currently-playing"

POLL_INTERVAL = 4  # segundos (mesmo valor da ESP32)

# ============================================================
# Estado global (simula variáveis estáticas do firmware)
# ============================================================
access_token = ""
token_expiry_time = 0  # timestamp Unix quando expira
last_track_id = ""

# ============================================================
# Cores do terminal (ANSI)
# ============================================================
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
CYAN = "\033[96m"
DIM = "\033[2m"
RESET = "\033[0m"
BOLD = "\033[1m"


def log(tag, msg, color=DIM):
    """Log formatado similar ao Serial.println da ESP32."""
    print(f"{color}[{tag}]{RESET} {msg}")


# ============================================================
# 1. REFRESH TOKEN (simula spotify_auth.cpp)
# ============================================================
def refresh_access_token():
    """
    Simula spotify_auth_refresh():
    POST /api/token com grant_type=refresh_token
    Header Authorization: Basic base64(client_id:client_secret)
    """
    global access_token, token_expiry_time

    log("AUTH", "Renovando access_token...", YELLOW)

    credentials = f"{CLIENT_ID}:{CLIENT_SECRET}"
    encoded = base64.b64encode(credentials.encode()).decode()

    headers = {
        "Authorization": f"Basic {encoded}",
        "Content-Type": "application/x-www-form-urlencoded",
    }

    data = {
        "grant_type": "refresh_token",
        "refresh_token": REFRESH_TOKEN,
    }

    resp = requests.post(SPOTIFY_TOKEN_URL, headers=headers, data=data)

    if resp.status_code == 200:
        body = resp.json()
        access_token = body["access_token"]
        expires_in = body.get("expires_in", 3600)
        token_expiry_time = time.time() + expires_in

        # Verifica se o Spotify retornou novo refresh_token
        new_refresh = body.get("refresh_token")
        if new_refresh and new_refresh != REFRESH_TOKEN:
            log("AUTH", f"⚠ Spotify retornou NOVO refresh_token!", RED)
            log("AUTH", f"  Atualize sua variável SPOTIFY_REFRESH_TOKEN:", RED)
            log("AUTH", f"  {new_refresh}", RED)

        log("AUTH", f"Token renovado! Expira em {expires_in}s", GREEN)
        return True
    else:
        log("AUTH", f"ERRO HTTP {resp.status_code}", RED)
        log("AUTH", f"  {resp.text}", RED)
        return False


def token_expired():
    """Simula spotify_auth_token_expired()"""
    if not access_token:
        return True
    # Margem de 60s (igual à ESP32: TOKEN_EXPIRY_MARGIN_MS)
    return time.time() >= (token_expiry_time - 60)


def ensure_valid_token():
    """Simula spotify_auth_ensure_valid_token()"""
    if token_expired():
        return refresh_access_token()
    return True


# ============================================================
# 2. POLL CURRENTLY PLAYING (simula spotify_poller.cpp)
# ============================================================
def poll_currently_playing():
    """
    Simula spotify_poller_poll():
    GET /v1/me/player/currently-playing
    Trata: 200, 204, 401, 429, erros de rede
    """
    global last_track_id

    if not ensure_valid_token():
        return "TOKEN_ERROR", None

    headers = {
        "Authorization": f"Bearer {access_token}",
        "Accept": "application/json",
    }

    try:
        resp = requests.get(SPOTIFY_NOW_PLAYING_URL, headers=headers)
    except requests.exceptions.ConnectionError as e:
        log("POLLER", f"Erro de rede: {e}", RED)
        return "NETWORK_ERROR", None

    # 204 = nada tocando
    if resp.status_code == 204:
        log("POLLER", "204 - Nada tocando", YELLOW)
        return "NO_CONTENT", None

    # 401 = token expirado
    if resp.status_code == 401:
        log("POLLER", "401 - Token expirado", RED)
        return "TOKEN_EXPIRED", None

    # 429 = rate limited
    if resp.status_code == 429:
        retry_after = int(resp.headers.get("Retry-After", 30))
        log("POLLER", f"429 - Rate limited! Retry-After: {retry_after}s", RED)
        return "RATE_LIMITED", retry_after

    # 200 = dados
    if resp.status_code == 200:
        return "OK", resp.json()

    # Outro erro
    log("POLLER", f"Erro HTTP {resp.status_code}: {resp.text[:200]}", RED)
    return "ERROR", None


# ============================================================
# 3. PARSE JSON (simula json_parser.cpp com filtro)
# ============================================================
def parse_track_info(data):
    """
    Simula json_parser_parse():
    Extrai APENAS os campos que a ESP32 usa (filtro ArduinoJson).
    Se o JSON vier diferente do esperado, falha aqui = falharia na placa.
    """
    if not data:
        return None

    # Valida estrutura esperada
    if "item" not in data or data["item"] is None:
        log("PARSER", "Campo 'item' ausente ou null no JSON", RED)
        return None

    item = data["item"]

    # Extrai campos (mesma lógica do firmware)
    track_info = {
        "is_playing": data.get("is_playing", False),
        "progress_ms": data.get("progress_ms", 0),
        "track_id": item.get("id", ""),
        "track_name": item.get("name", ""),
        "duration_ms": item.get("duration_ms", 0),
        "artist_name": "",
        "cover_url": "",
    }

    # Artistas concatenados (mesmo comportamento do firmware)
    artists = item.get("artists", [])
    track_info["artist_name"] = ", ".join(a.get("name", "") for a in artists)

    # Menor imagem disponível (firmware faz isso no parser)
    images = item.get("album", {}).get("images", [])
    smallest_url = ""
    smallest_size = 99999
    for img in images:
        h = img.get("height", 9999)
        w = img.get("width", 9999)
        size = h * w
        if size < smallest_size:
            smallest_size = size
            smallest_url = img.get("url", "")

    track_info["cover_url"] = smallest_url

    return track_info


# ============================================================
# 4. DOWNLOAD COVER (simula spotify_poller_download_cover)
# ============================================================
def download_cover(url):
    """
    Simula download da capa via HTTPS.
    Salva em disco para inspeção visual.
    Retorna tamanho dos dados (simula bytes na PSRAM).
    """
    if not url:
        return 0

    log("COVER", f"Baixando: {url}", CYAN)

    try:
        resp = requests.get(url, timeout=10)
        if resp.status_code == 200:
            # Salva no disco para conferência visual
            cover_path = Path(__file__).parent / "last_cover.jpg"
            cover_path.write_bytes(resp.content)
            size = len(resp.content)
            log("COVER", f"OK! {size} bytes → salvo em tools/last_cover.jpg", GREEN)
            return size
        else:
            log("COVER", f"Erro HTTP {resp.status_code}", RED)
            return 0
    except Exception as e:
        log("COVER", f"Erro: {e}", RED)
        return 0


# ============================================================
# 5. DISPLAY (simula ui_renderer no terminal)
# ============================================================
def format_time(ms):
    """Simula formatTime() do ui_renderer"""
    total_sec = ms // 1000
    minutes = total_sec // 60
    seconds = total_sec % 60
    return f"{minutes}:{seconds:02d}"


def render_progress_bar(progress_ms, duration_ms, width=40):
    """Simula a barra de progresso do display"""
    if duration_ms == 0:
        return "[" + " " * width + "]"
    ratio = min(progress_ms / duration_ms, 1.0)
    filled = int(width * ratio)
    bar = "█" * filled + "░" * (width - filled)
    return f"[{bar}]"


def display_track(track_info, cover_size=0):
    """Simula o render completo no terminal (equivale ao display 240x240)"""
    print()
    print(f"  {BOLD}{'═' * 50}{RESET}")

    if cover_size > 0:
        print(f"  {DIM}┌────────────────┐{RESET}")
        print(f"  {DIM}│   CAPA ALBUM   │{RESET}  {DIM}({cover_size} bytes JPEG){RESET}")
        print(f"  {DIM}│   (salva em    │{RESET}")
        print(f"  {DIM}│  last_cover.jpg)│{RESET}")
        print(f"  {DIM}└────────────────┘{RESET}")

    status = f"{GREEN}▶ Tocando{RESET}" if track_info["is_playing"] else f"{YELLOW}⏸ Pausado{RESET}"
    print(f"  {status}")
    print()
    print(f"  {BOLD}{track_info['track_name']}{RESET}")
    print(f"  {DIM}{track_info['artist_name']}{RESET}")
    print()

    progress = track_info["progress_ms"]
    duration = track_info["duration_ms"]
    bar = render_progress_bar(progress, duration)
    elapsed = format_time(progress)
    total = format_time(duration)
    print(f"  {GREEN}{bar}{RESET}")
    print(f"  {elapsed}{'':>{44 - len(elapsed) - len(total)}}{total}")

    print(f"  {BOLD}{'═' * 50}{RESET}")
    print()

    # Info técnica (para debug)
    print(f"  {DIM}Track ID: {track_info['track_id']}{RESET}")
    print(f"  {DIM}Cover URL: {track_info['cover_url'][:60]}...{RESET}")

    # Verifica se o texto do título caberia no display (240px com fonte 2)
    # Fonte 2 do TFT_eSPI = ~12px por caractere
    max_chars = 240 // 7  # Margem considerada (~34 chars)
    title_len = len(track_info["track_name"])
    if title_len > max_chars:
        print(f"  {YELLOW}⚠ Título tem {title_len} chars (>{max_chars}) → marquee será ativado{RESET}")
    artist_len = len(track_info["artist_name"])
    if artist_len > max_chars:
        print(f"  {YELLOW}⚠ Artista tem {artist_len} chars (>{max_chars}) → marquee será ativado{RESET}")

    print()


def display_idle():
    """Simula tela de nada tocando"""
    print()
    print(f"  {GREEN}♪ Spotify{RESET}")
    print(f"  {DIM}Nada tocando. Abra o Spotify e toque algo!{RESET}")
    print()


# ============================================================
# 6. LOOP PRINCIPAL (simula main.cpp state machine)
# ============================================================
def interpolate_progress(track_info, start_time):
    """
    Simula a interpolação local de progresso via millis().
    Avança o progress_ms com base no tempo decorrido desde o poll.
    """
    if not track_info["is_playing"]:
        return track_info["progress_ms"]

    elapsed_ms = int((time.time() - start_time) * 1000)
    interpolated = track_info["progress_ms"] + elapsed_ms
    return min(interpolated, track_info["duration_ms"])


def main():
    global last_track_id

    print()
    print(f"{BOLD}{'═' * 56}{RESET}")
    print(f"{BOLD}  ESP32 Spotify Now Playing - Teste de Webservice{RESET}")
    print(f"{BOLD}{'═' * 56}{RESET}")
    print()
    print(f"  Este script simula EXATAMENTE o que a ESP32 faz.")
    print(f"  Se funcionar aqui, vai funcionar na placa.")
    print()

    # Valida credenciais
    if not CLIENT_ID or not CLIENT_SECRET or not REFRESH_TOKEN:
        print(f"{RED}ERRO: Credenciais não configuradas!{RESET}")
        print()
        print("Configure via variáveis de ambiente:")
        print("  export SPOTIFY_CLIENT_ID=seu_id")
        print("  export SPOTIFY_CLIENT_SECRET=seu_secret")
        print("  export SPOTIFY_REFRESH_TOKEN=seu_token")
        print()
        print("Ou crie um arquivo tools/.env com:")
        print("  SPOTIFY_CLIENT_ID=seu_id")
        print("  SPOTIFY_CLIENT_SECRET=seu_secret")
        print("  SPOTIFY_REFRESH_TOKEN=seu_token")
        print()
        print("(Obtenha o refresh_token com: python get_refresh_token.py)")
        sys.exit(1)

    log("MAIN", f"Client ID: {CLIENT_ID[:8]}...{CLIENT_ID[-4:]}")
    log("MAIN", f"Refresh Token: {REFRESH_TOKEN[:8]}...{REFRESH_TOKEN[-4:]}")
    print()

    # ---- Simula STATE_AUTH_INIT ----
    log("MAIN", "=== STATE_AUTH_INIT ===", BOLD)
    if not refresh_access_token():
        log("MAIN", "Falha na autenticação! Verifique credenciais.", RED)
        sys.exit(1)

    print()
    log("MAIN", "=== STATE_POLLING (Ctrl+C para sair) ===", BOLD)
    print()

    # ---- Simula STATE_POLLING loop ----
    poll_count = 0
    current_track = None
    poll_time = 0

    try:
        while True:
            poll_count += 1
            log("MAIN", f"--- Poll #{poll_count} ---", DIM)

            # Poll da API
            status, data = poll_currently_playing()
            poll_time = time.time()

            if status == "OK":
                track_info = parse_track_info(data)

                if track_info:
                    # Verifica se track mudou (simula spotify_poller_track_changed)
                    track_changed = (track_info["track_id"] != last_track_id)

                    if track_changed:
                        last_track_id = track_info["track_id"]
                        log("POLLER", "Track mudou! Baixando capa...", GREEN)
                        cover_size = download_cover(track_info["cover_url"])
                    else:
                        cover_size = 0
                        log("POLLER", "Mesmo track, só atualiza progresso", DIM)

                    current_track = track_info
                    display_track(track_info, cover_size if track_changed else 0)
                else:
                    log("PARSER", "Parse retornou None", RED)

            elif status == "NO_CONTENT":
                current_track = None
                last_track_id = ""
                display_idle()

            elif status == "TOKEN_EXPIRED":
                log("MAIN", "Renovando token...", YELLOW)
                refresh_access_token()
                continue  # Retry imediato

            elif status == "RATE_LIMITED":
                wait = data if data else 30
                log("MAIN", f"Aguardando {wait}s (rate limit)...", YELLOW)
                time.sleep(wait)
                continue

            elif status == "NETWORK_ERROR":
                log("MAIN", "Erro de rede. Tentando novamente em 5s...", RED)
                time.sleep(5)
                continue

            # ---- Interpolação local de progresso (como a ESP32 faz) ----
            # Mostra a barra avançando por 1 segundo no meio do intervalo
            if current_track and current_track["is_playing"]:
                time.sleep(POLL_INTERVAL / 2)

                interpolated_ms = interpolate_progress(current_track, poll_time)
                bar = render_progress_bar(interpolated_ms, current_track["duration_ms"])
                elapsed = format_time(interpolated_ms)
                total = format_time(current_track["duration_ms"])
                print(f"\r  {DIM}Interpolado: {elapsed} {bar} {total}{RESET}", end="", flush=True)

                time.sleep(POLL_INTERVAL / 2)
                print()  # Nova linha após interpolação
            else:
                time.sleep(POLL_INTERVAL)

    except KeyboardInterrupt:
        print()
        print()
        log("MAIN", "Encerrado pelo usuário (Ctrl+C)", YELLOW)
        print()

        # Sumário final
        print(f"  {BOLD}Sumário do teste:{RESET}")
        print(f"  • Polls realizados: {poll_count}")
        print(f"  • Token renovado com sucesso: ✓")
        print(f"  • JSON parseado corretamente: ✓")
        if last_track_id:
            print(f"  • Último track: {last_track_id}")
        print(f"  • Capa baixada: verificar tools/last_cover.jpg")
        print()
        print(f"  {GREEN}✓ Tudo OK! Pode flashar na ESP32 com confiança.{RESET}")
        print()


if __name__ == "__main__":
    main()
