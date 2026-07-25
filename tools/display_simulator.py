#!/usr/bin/env python3
# ============================================================
# display_simulator.py
# ============================================================
# Simulador visual do display ST7789 240x240 usando Tkinter.
# Mostra EXATAMENTE como vai ficar na placa: capa, título com
# marquee, artista, barra de progresso animada.
#
# Usa apenas tkinter (incluso no Python) + Pillow + requests.
# Não precisa de pygame!
#
# Requisitos:
#   pip install requests Pillow python-dotenv
#
# Uso:
#   python display_simulator.py
#
# Configuração:
#   Variáveis de ambiente ou arquivo .env com:
#     SPOTIFY_CLIENT_ID=...
#     SPOTIFY_CLIENT_SECRET=...
#     SPOTIFY_REFRESH_TOKEN=...
# ============================================================

import os
import sys
import time
import base64
import threading
import io
import tkinter as tk

try:
    import requests
except ImportError:
    print("ERRO: instale o requests: pip install requests")
    sys.exit(1)

try:
    from PIL import Image, ImageTk, ImageDraw, ImageFont
except ImportError:
    print("ERRO: instale o Pillow: pip install Pillow")
    sys.exit(1)

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

# ============================================================
# CONFIGURAÇÃO
# ============================================================
CLIENT_ID = os.environ.get("SPOTIFY_CLIENT_ID", "")
CLIENT_SECRET = os.environ.get("SPOTIFY_CLIENT_SECRET", "")
REFRESH_TOKEN = os.environ.get("SPOTIFY_REFRESH_TOKEN", "")

SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token"
SPOTIFY_NOW_PLAYING_URL = "https://api.spotify.com/v1/me/player/currently-playing"

# ============================================================
# DIMENSÕES DO DISPLAY (igual ao firmware)
# ============================================================
DISPLAY_W = 240
DISPLAY_H = 240
SCALE = 3  # Escala da janela (3x para melhor visualização)

# Layout (mesmos valores de ui_renderer.h)
COVER_SIZE = 120
COVER_X = 60
COVER_Y = 10

TEXT_Y_TITLE = 140
TEXT_Y_ARTIST = 165
TEXT_X_MARGIN = 8

PROGRESS_BAR_Y = 195
PROGRESS_BAR_H = 6
PROGRESS_BAR_X = 10
PROGRESS_BAR_W = 220

TIME_Y = 210

# Cores
COLOR_BG = "#000000"
COLOR_TEXT_PRIMARY = "#FFFFFF"
COLOR_TEXT_SECONDARY = "#B4B4B4"
COLOR_PROGRESS_BG = "#282828"
COLOR_PROGRESS_FG = "#1ED760"  # Verde Spotify
COLOR_SPOTIFY_GREEN = "#1ED760"

# Marquee
MARQUEE_SPEED = 1
MARQUEE_PAUSE_FRAMES = 60

# Polling
POLL_INTERVAL = 4.0

# ============================================================
# Estado global
# ============================================================
access_token = ""
token_expiry = 0
current_track = None
last_track_id = ""
cover_image = None  # PIL Image da capa

# Marquee
marquee_title_offset = 0
marquee_title_pause = MARQUEE_PAUSE_FRAMES
marquee_artist_offset = 0
marquee_artist_pause = MARQUEE_PAUSE_FRAMES

# Progresso
progress_sync_time = 0
progress_sync_ms = 0


# ============================================================
# SPOTIFY AUTH
# ============================================================
def refresh_access_token():
    global access_token, token_expiry

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

    try:
        resp = requests.post(SPOTIFY_TOKEN_URL, headers=headers, data=data, timeout=10)
        if resp.status_code == 200:
            body = resp.json()
            access_token = body["access_token"]
            token_expiry = time.time() + body.get("expires_in", 3600)
            print("[AUTH] Token renovado!")
            return True
        else:
            print(f"[AUTH] Erro {resp.status_code}: {resp.text[:200]}")
            return False
    except Exception as e:
        print(f"[AUTH] Exceção: {e}")
        return False


def ensure_token():
    if not access_token or time.time() >= (token_expiry - 60):
        return refresh_access_token()
    return True


# ============================================================
# SPOTIFY POLLER
# ============================================================
def poll_spotify():
    global current_track, last_track_id, cover_image
    global progress_sync_time, progress_sync_ms
    global marquee_title_offset, marquee_title_pause
    global marquee_artist_offset, marquee_artist_pause

    if not ensure_token():
        return

    headers = {
        "Authorization": f"Bearer {access_token}",
        "Accept": "application/json",
    }

    try:
        resp = requests.get(SPOTIFY_NOW_PLAYING_URL, headers=headers, timeout=10)
    except Exception as e:
        print(f"[POLLER] Erro: {e}")
        return

    if resp.status_code == 204:
        current_track = None
        print("[POLLER] Nada tocando")
        return

    if resp.status_code == 401:
        print("[POLLER] 401 - Renovando token...")
        refresh_access_token()
        return

    if resp.status_code == 429:
        retry = int(resp.headers.get("Retry-After", 30))
        print(f"[POLLER] 429 - Rate limited, espera {retry}s")
        return

    if resp.status_code != 200:
        print(f"[POLLER] HTTP {resp.status_code}")
        return

    data = resp.json()
    item = data.get("item")
    if not item:
        current_track = None
        return

    track = {
        "is_playing": data.get("is_playing", False),
        "progress_ms": data.get("progress_ms", 0),
        "duration_ms": item.get("duration_ms", 0),
        "track_id": item.get("id", ""),
        "track_name": item.get("name", ""),
        "artist_name": ", ".join(a["name"] for a in item.get("artists", [])),
        "cover_url": "",
    }

    # Imagem ~300px (mesmo do firmware ESP32 — escolhe a mais próxima de 300px)
    images = item.get("album", {}).get("images", [])
    best_url = ""
    best_diff = 99999
    for img in images:
        h = img.get("height", 0)
        diff = abs(h - 300)
        if diff < best_diff:
            best_diff = diff
            best_url = img.get("url", "")
    track["cover_url"] = best_url

    # Sincroniza progresso
    progress_sync_time = time.time()
    progress_sync_ms = track["progress_ms"]

    # Se track mudou, baixa capa
    if track["track_id"] != last_track_id:
        last_track_id = track["track_id"]
        print(f"[POLLER] Novo: {track['artist_name']} - {track['track_name']}")

        # Reseta marquee
        marquee_title_offset = 0
        marquee_title_pause = MARQUEE_PAUSE_FRAMES
        marquee_artist_offset = 0
        marquee_artist_pause = MARQUEE_PAUSE_FRAMES

        # Baixa capa
        if track["cover_url"]:
            try:
                r = requests.get(track["cover_url"], timeout=10)
                if r.status_code == 200:
                    img = Image.open(io.BytesIO(r.content))
                    cover_image = img.resize((COVER_SIZE, COVER_SIZE), Image.LANCZOS)
                    print(f"[COVER] OK ({len(r.content)} bytes)")
            except Exception as e:
                print(f"[COVER] Erro: {e}")

    current_track = track


# ============================================================
# RENDERIZAÇÃO COM PIL (simula display 240x240)
# ============================================================
def format_time(ms):
    total_sec = max(0, ms) // 1000
    m = total_sec // 60
    s = total_sec % 60
    return f"{m}:{s:02d}"


def render_frame():
    """Gera um frame PIL Image na resolução escalada (com anti-alias)."""
    global marquee_title_offset, marquee_title_pause
    global marquee_artist_offset, marquee_artist_pause

    # Renderiza direto na resolução final (SCALE x) para fontes nítidas
    S = SCALE
    W = DISPLAY_W * S
    H = DISPLAY_H * S

    frame = Image.new("RGB", (W, H), COLOR_BG)
    draw = ImageDraw.Draw(frame)

    # Fontes escaladas (maiores = mais nítidas)
    try:
        font_title = ImageFont.truetype("segoeui.ttf", 15 * S)
        font_artist = ImageFont.truetype("segoeui.ttf", 12 * S)
        font_time = ImageFont.truetype("segoeui.ttf", 10 * S)
    except (OSError, IOError):
        try:
            font_title = ImageFont.truetype("arial.ttf", 15 * S)
            font_artist = ImageFont.truetype("arial.ttf", 12 * S)
            font_time = ImageFont.truetype("arial.ttf", 10 * S)
        except (OSError, IOError):
            font_title = ImageFont.load_default()
            font_artist = ImageFont.load_default()
            font_time = ImageFont.load_default()

    if current_track is None:
        # === Tela "Nada tocando" ===
        draw.text((W // 2, 95 * S), "Spotify",
                  fill=COLOR_SPOTIFY_GREEN, font=font_title, anchor="mm")
        draw.text((W // 2, 130 * S), "Nada tocando",
                  fill=COLOR_TEXT_SECONDARY, font=font_artist, anchor="mm")
        draw.text((W // 2, 155 * S), "Abra o Spotify e",
                  fill=COLOR_TEXT_SECONDARY, font=font_time, anchor="mm")
        draw.text((W // 2, 172 * S), "toque algo!",
                  fill=COLOR_TEXT_SECONDARY, font=font_time, anchor="mm")
        return frame

    # === Capa do álbum (redimensionada com anti-alias) ===
    if cover_image:
        cover_scaled = cover_image.resize((COVER_SIZE * S, COVER_SIZE * S), Image.LANCZOS)
        frame.paste(cover_scaled, (COVER_X * S, COVER_Y * S))
    else:
        draw.rectangle([COVER_X * S, COVER_Y * S,
                        (COVER_X + COVER_SIZE) * S, (COVER_Y + COVER_SIZE) * S],
                       fill=COLOR_PROGRESS_BG)
        draw.text(((COVER_X + COVER_SIZE // 2) * S, (COVER_Y + COVER_SIZE // 2) * S),
                  "...", fill=COLOR_TEXT_SECONDARY, font=font_title, anchor="mm")

    # === Indicador de pausado ===
    if not current_track["is_playing"]:
        cx = (COVER_X + COVER_SIZE // 2) * S
        cy = (COVER_Y + COVER_SIZE // 2) * S
        r = 18 * S
        draw.ellipse([cx - r, cy - r, cx + r, cy + r],
                     fill="#000000", outline=COLOR_TEXT_SECONDARY, width=2)
        # Barras de pausa
        bw = 5 * S
        bh = 20 * S
        draw.rectangle([cx - 8 * S, cy - 10 * S, cx - 8 * S + bw, cy - 10 * S + bh],
                       fill=COLOR_TEXT_PRIMARY)
        draw.rectangle([cx + 3 * S, cy - 10 * S, cx + 3 * S + bw, cy - 10 * S + bh],
                       fill=COLOR_TEXT_PRIMARY)

    # === Título (com marquee) ===
    title = current_track["track_name"]
    title_bbox = draw.textbbox((0, 0), title, font=font_title)
    title_w = title_bbox[2] - title_bbox[0]
    max_text_w = (DISPLAY_W - (TEXT_X_MARGIN * 2)) * S

    if title_w > max_text_w:
        # Marquee animado
        if marquee_title_pause > 0:
            marquee_title_pause -= 1
        else:
            marquee_title_offset += MARQUEE_SPEED * S
            if marquee_title_offset > (title_w - max_text_w):
                marquee_title_offset = 0
                marquee_title_pause = MARQUEE_PAUSE_FRAMES

        # Cria sub-imagem para clip
        txt_img = Image.new("RGB", (title_w + 10, 22 * S), COLOR_BG)
        txt_draw = ImageDraw.Draw(txt_img)
        txt_draw.text((0, 0), title, fill=COLOR_TEXT_PRIMARY, font=font_title)
        crop = txt_img.crop((marquee_title_offset, 0,
                             marquee_title_offset + max_text_w, 22 * S))
        frame.paste(crop, (TEXT_X_MARGIN * S, TEXT_Y_TITLE * S))
    else:
        draw.text((W // 2, (TEXT_Y_TITLE + 8) * S), title,
                  fill=COLOR_TEXT_PRIMARY, font=font_title, anchor="mm")

    # === Artista (com marquee) ===
    artist = current_track["artist_name"]
    artist_bbox = draw.textbbox((0, 0), artist, font=font_artist)
    artist_w = artist_bbox[2] - artist_bbox[0]

    if artist_w > max_text_w:
        if marquee_artist_pause > 0:
            marquee_artist_pause -= 1
        else:
            marquee_artist_offset += MARQUEE_SPEED * S
            if marquee_artist_offset > (artist_w - max_text_w):
                marquee_artist_offset = 0
                marquee_artist_pause = MARQUEE_PAUSE_FRAMES

        txt_img = Image.new("RGB", (artist_w + 10, 18 * S), COLOR_BG)
        txt_draw = ImageDraw.Draw(txt_img)
        txt_draw.text((0, 0), artist, fill=COLOR_TEXT_SECONDARY, font=font_artist)
        crop = txt_img.crop((marquee_artist_offset, 0,
                             marquee_artist_offset + max_text_w, 18 * S))
        frame.paste(crop, (TEXT_X_MARGIN * S, TEXT_Y_ARTIST * S))
    else:
        draw.text((W // 2, (TEXT_Y_ARTIST + 6) * S), artist,
                  fill=COLOR_TEXT_SECONDARY, font=font_artist, anchor="mm")

    # === Barra de progresso ===
    duration = current_track["duration_ms"]
    if duration > 0:
        if current_track["is_playing"]:
            elapsed_ms = (time.time() - progress_sync_time) * 1000
            progress = min(progress_sync_ms + elapsed_ms, duration)
        else:
            progress = progress_sync_ms

        ratio = progress / duration

        # Fundo
        draw.rounded_rectangle(
            [PROGRESS_BAR_X * S, PROGRESS_BAR_Y * S,
             (PROGRESS_BAR_X + PROGRESS_BAR_W) * S, (PROGRESS_BAR_Y + PROGRESS_BAR_H) * S],
            radius=3 * S, fill=COLOR_PROGRESS_BG
        )

        # Preenchido
        filled_w = int(PROGRESS_BAR_W * ratio)
        if filled_w > 2:
            draw.rounded_rectangle(
                [PROGRESS_BAR_X * S, PROGRESS_BAR_Y * S,
                 (PROGRESS_BAR_X + filled_w) * S, (PROGRESS_BAR_Y + PROGRESS_BAR_H) * S],
                radius=3 * S, fill=COLOR_PROGRESS_FG
            )

        # Bolinha
        dot_x = (PROGRESS_BAR_X + filled_w) * S
        dot_y = (PROGRESS_BAR_Y + PROGRESS_BAR_H // 2) * S
        dot_r = 5 * S
        draw.ellipse([dot_x - dot_r, dot_y - dot_r, dot_x + dot_r, dot_y + dot_r],
                     fill=COLOR_PROGRESS_FG)

        # Tempo decorrido (esquerda)
        draw.text((PROGRESS_BAR_X * S, TIME_Y * S), format_time(int(progress)),
                  fill=COLOR_TEXT_SECONDARY, font=font_time)

        # Tempo total (direita)
        draw.text(((PROGRESS_BAR_X + PROGRESS_BAR_W) * S, TIME_Y * S),
                  format_time(duration),
                  fill=COLOR_TEXT_SECONDARY, font=font_time, anchor="ra")

    return frame


# ============================================================
# MAIN - TKINTER WINDOW
# ============================================================
class DisplaySimulator:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32 Spotify Display - Simulador 240x240")
        self.root.resizable(False, False)
        self.root.configure(bg="#333333")

        # Canvas no tamanho escalado
        self.canvas_w = DISPLAY_W * SCALE
        self.canvas_h = DISPLAY_H * SCALE
        self.canvas = tk.Canvas(root, width=self.canvas_w, height=self.canvas_h,
                                bg="black", highlightthickness=2,
                                highlightbackground="#555555")
        self.canvas.pack(padx=10, pady=10)

        # Label de status
        self.status_var = tk.StringVar(value="Iniciando...")
        self.status_label = tk.Label(root, textvariable=self.status_var,
                                     bg="#333333", fg="#AAAAAA",
                                     font=("Segoe UI", 9))
        self.status_label.pack(pady=(0, 5))

        # Label de controles
        ctrl_label = tk.Label(root,
                              text="[R] Refresh | [ESC] Sair | Polling a cada 4s",
                              bg="#333333", fg="#666666",
                              font=("Segoe UI", 8))
        ctrl_label.pack(pady=(0, 8))

        # Binds de teclado
        root.bind("<Escape>", lambda e: root.destroy())
        root.bind("r", lambda e: self.force_poll())
        root.bind("R", lambda e: self.force_poll())

        # Imagem do tkinter
        self.tk_image = None

        # Timer de poll
        self.last_poll_time = 0

        # Inicia loop de renderização
        self.update_frame()

        # Primeiro poll em thread
        self.poll_async()

    def force_poll(self):
        print("[MAIN] Refresh forçado (tecla R)")
        self.poll_async()

    def poll_async(self):
        """Faz poll em thread separada para não travar a UI."""
        self.status_var.set("Consultando Spotify...")

        def do_poll():
            poll_spotify()
            if current_track:
                self.status_var.set(
                    f"{'▶' if current_track['is_playing'] else '⏸'} "
                    f"{current_track['track_name'][:30]}"
                )
            else:
                self.status_var.set("Nada tocando")
            self.last_poll_time = time.time()

        threading.Thread(target=do_poll, daemon=True).start()

    def update_frame(self):
        """Atualiza o frame renderizado a 30fps."""
        # Verifica se precisa fazer novo poll
        if time.time() - self.last_poll_time >= POLL_INTERVAL:
            self.poll_async()

        # Renderiza frame já na resolução final (720x720)
        frame = render_frame()

        # Converte para tkinter (já no tamanho certo, sem resize)
        self.tk_image = ImageTk.PhotoImage(frame)
        self.canvas.create_image(0, 0, anchor=tk.NW, image=self.tk_image)

        # Agenda próximo frame (~30 fps)
        self.root.after(33, self.update_frame)


def main():
    # Valida credenciais
    if not CLIENT_ID or not CLIENT_SECRET or not REFRESH_TOKEN:
        print("=" * 50)
        print("ERRO: Configure as credenciais!")
        print()
        print("Use variáveis de ambiente ou crie tools/.env:")
        print("  SPOTIFY_CLIENT_ID=...")
        print("  SPOTIFY_CLIENT_SECRET=...")
        print("  SPOTIFY_REFRESH_TOKEN=...")
        print("=" * 50)
        sys.exit(1)

    # Obtém primeiro token
    print("[AUTH] Obtendo token inicial...")
    if not refresh_access_token():
        print("Falha na autenticação! Verifique credenciais.")
        sys.exit(1)

    print("[MAIN] Abrindo simulador do display...")
    print("[MAIN] Teclas: R=refresh, ESC=sair")
    print()

    # Cria janela tkinter
    root = tk.Tk()
    app = DisplaySimulator(root)
    root.mainloop()

    print("[MAIN] Simulador encerrado")


if __name__ == "__main__":
    main()
