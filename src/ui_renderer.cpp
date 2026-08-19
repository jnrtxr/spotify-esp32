// ============================================================
// ui_renderer.cpp
// ============================================================
// Implementação da renderização da UI no display ST7789.
// Inclui efeito marquee para textos longos e barra de progresso
// animada localmente (sem requisição HTTP).
// ============================================================

#include "ui_renderer.h"
#include "display_driver.h"
#include <TJpg_Decoder.h>

// Ponteiro para a instância TFT
static TFT_eSPI* tft = nullptr;

// ---- Estado do marquee ----
struct MarqueeState {
    String text;
    int16_t text_width;     // Largura total do texto em pixels
    int16_t offset;         // Offset atual de scroll
    int16_t max_visible_w;  // Largura visível (DISPLAY_WIDTH - margens)
    int16_t pause_counter;  // Contador de pausa no início/fim
    bool scrolling;         // Se o texto precisa de scroll
};

static MarqueeState s_marquee_title;
static MarqueeState s_marquee_artist;

// ---- Estado da barra de progresso ----
static uint32_t s_last_progress_ms = 0;
static uint32_t s_last_duration_ms = 0;

// ---- Último track desenhado (para evitar redesenho desnecessário) ----
static String s_last_drawn_title = "";
static String s_last_drawn_artist = "";

// ---- Funções auxiliares internas ----

/// Formata milissegundos em "m:ss"
static String formatTime(uint32_t ms) {
    uint32_t totalSec = ms / 1000;
    uint32_t min = totalSec / 60;
    uint32_t sec = totalSec % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u:%02u", min, sec);
    return String(buf);
}

/// Inicializa um marquee state para um texto
static void initMarquee(MarqueeState& state, const String& text, int16_t y_unused) {
    state.text = text;
    state.offset = 0;
    state.pause_counter = MARQUEE_PAUSE_FRAMES;
    state.max_visible_w = DISPLAY_WIDTH - TEXT_X_MARGIN - 5;

    // Calcula largura do texto em pixels (usa fonte 2)
    tft->setTextFont(2);
    state.text_width = tft->textWidth(text);
    // Ativa marquee se o texto não cabe na área disponível
    state.scrolling = (state.text_width > state.max_visible_w);
    
    if (state.scrolling) {
        Serial.printf("[UI] Marquee ativado para: %s (%dpx > %dpx)\n", 
                      text.c_str(), state.text_width, state.max_visible_w);
    }
}

// ============================================================
// ui_renderer_init()
// Configura fontes e prepara a tela
// ============================================================
void ui_renderer_init() {
    tft = display_get_tft();

    // Configura fonte padrão
    tft->setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG);
    tft->setTextDatum(TL_DATUM); // Top-Left como datum padrão

    // Limpa tela
    display_clear();

    // Inicializa marquee vazio
    s_marquee_title.text = "";
    s_marquee_artist.text = "";

    Serial.println("[UI] Inicializado");
}

// ============================================================
// ui_renderer_draw_track()
// Desenha as informações do track (texto e barra)
// Não redesenha a capa (isso é feito separadamente)
// ============================================================
void ui_renderer_draw_track(TrackInfo* track) {
    if (!track || !tft) return;

    // --- Título ---
    if (track->track_name != s_last_drawn_title) {
        s_last_drawn_title = track->track_name;
        tft->setTextFont(2);
        initMarquee(s_marquee_title, track->track_name, TEXT_Y_TITLE);

        // Desenha primeira vez usando sprite (clipping perfeito)
        int16_t area_w = DISPLAY_WIDTH - TEXT_X_MARGIN - 5;
        TFT_eSprite spr = TFT_eSprite(tft);
        spr.createSprite(area_w, 20);
        spr.fillSprite(TFT_BLACK);
        spr.setTextFont(2);
        spr.setTextColor(TFT_WHITE, TFT_BLACK);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(track->track_name, 0, 2);
        spr.pushSprite(TEXT_X_MARGIN, TEXT_Y_TITLE);
        spr.deleteSprite();
    }

    // --- Artista ---
    if (track->artist_name != s_last_drawn_artist) {
        s_last_drawn_artist = track->artist_name;
        tft->setTextFont(2);
        initMarquee(s_marquee_artist, track->artist_name, TEXT_Y_ARTIST);

        // Sincroniza pausa com o título (ambos começam juntos)
        s_marquee_artist.pause_counter = s_marquee_title.pause_counter;
        s_marquee_artist.offset = 0;

        // Desenha primeira vez usando sprite (clipping perfeito)
        int16_t area_w = DISPLAY_WIDTH - TEXT_X_MARGIN - 5;
        TFT_eSprite spr = TFT_eSprite(tft);
        spr.createSprite(area_w, 20);
        spr.fillSprite(TFT_BLACK);
        spr.setTextFont(2);
        spr.setTextColor(0x7BEF, TFT_BLACK);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(track->artist_name, 0, 2);
        spr.pushSprite(TEXT_X_MARGIN, TEXT_Y_ARTIST);
        spr.deleteSprite();
    }

    // --- Ícone play/pause (desenha sempre) ---
    {
        int icon_x = TEXT_X_MARGIN + (DISPLAY_WIDTH - TEXT_X_MARGIN) / 2;
        int icon_y = TEXT_Y_ARTIST + 50;
        // Limpa área do ícone
        tft->fillRect(icon_x - 12, icon_y - 12, 30, 24, TFT_BLACK);
        if (!track->is_playing) {
            // Triângulo de play ▶ (música pausada = mostrar play)
            tft->fillTriangle(icon_x - 8, icon_y - 10,
                              icon_x - 8, icon_y + 10,
                              icon_x + 10, icon_y,
                              TFT_WHITE);
        } else {
            // Barras de pause ⏸ (música tocando = mostrar pause)
            tft->fillRect(icon_x - 8, icon_y - 9, 6, 18, TFT_WHITE);
            tft->fillRect(icon_x + 2, icon_y - 9, 6, 18, TFT_WHITE);
        }
    }

    // --- Barra de progresso ---
    s_last_progress_ms = track->progress_ms;
    s_last_duration_ms = track->duration_ms;
    ui_renderer_update_progress(track->progress_ms, track->duration_ms, track->is_playing);
}

// ============================================================
// ui_renderer_update_progress()
// Atualiza barra de progresso e tempos.
// Chamado frequentemente pelo loop principal (interpolação local)
// ============================================================
void ui_renderer_update_progress(uint32_t progress_ms, uint32_t duration_ms, bool is_playing) {
    if (!tft || duration_ms == 0) return;

    // Calcula proporção
    float ratio = (float)progress_ms / (float)duration_ms;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < 0.0f) ratio = 0.0f;

    int filled_w = (int)(PROGRESS_BAR_W * ratio);

    // Desenha fundo da barra
    tft->fillRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_W, PROGRESS_BAR_HEIGHT, COLOR_PROGRESS_BG);

    // Desenha parte preenchida
    if (filled_w > 0) {
        tft->fillRect(PROGRESS_BAR_X, PROGRESS_BAR_Y, filled_w, PROGRESS_BAR_HEIGHT, COLOR_PROGRESS_FG);
    }

    // Bolinha indicadora de posição
    int dot_x = PROGRESS_BAR_X + filled_w;
    int dot_y = PROGRESS_BAR_Y + (PROGRESS_BAR_HEIGHT / 2);
    tft->fillCircle(dot_x, dot_y, 4, COLOR_PROGRESS_FG);

    // --- Tempos ---
    tft->setTextFont(1);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);

    // Tempo decorrido (esquerda)
    String elapsed = formatTime(progress_ms);
    tft->fillRect(PROGRESS_BAR_X, TIME_Y, 50, 10, COLOR_BG);
    tft->setTextDatum(TL_DATUM);
    tft->drawString(elapsed, PROGRESS_BAR_X, TIME_Y);

    // Tempo total (direita)
    String total = formatTime(duration_ms);
    tft->fillRect(PROGRESS_BAR_X + PROGRESS_BAR_W - 50, TIME_Y, 50, 10, COLOR_BG);
    tft->setTextDatum(TR_DATUM);
    tft->drawString(total, PROGRESS_BAR_X + PROGRESS_BAR_W, TIME_Y);

    tft->setTextDatum(TL_DATUM);
}

// ============================================================
// ui_renderer_draw_cover()
// Decodifica e renderiza a capa JPEG no display.
// Usa TJpg_Decoder que chama display_jpeg_output() por bloco.
// ============================================================
void ui_renderer_draw_cover(uint8_t* jpeg_data, uint32_t jpeg_size) {
    if (!jpeg_data || jpeg_size == 0) return;

    // Limpa área da capa
    tft->fillRect(COVER_X, COVER_Y, COVER_SIZE, COVER_SIZE, COLOR_BG);

    // Obtém dimensões da imagem sem decodificar
    uint16_t img_w = 0, img_h = 0;
    TJpgDec.getJpgSize(&img_w, &img_h, jpeg_data, jpeg_size);

    Serial.printf("[UI] Capa JPEG: %ux%u pixels\n", img_w, img_h);

    // Para imagem 300x300: scale=2 → 150x150 (perfeito para COVER_SIZE 150)
    // Para imagem 640x640: scale=4 → 160x160
    // Para imagem 64x64: scale=1 → 64x64
    uint8_t scale = 1;
    if (img_w >= 500) scale = 4;
    else if (img_w >= 200) scale = 2;
    else if (img_w > COVER_SIZE) scale = 2;

    TJpgDec.setJpgScale(scale);

    // Centraliza a capa na área reservada
    uint16_t scaled_w = img_w / scale;
    uint16_t scaled_h = img_h / scale;
    int16_t x_pos = COVER_X + (COVER_SIZE - scaled_w) / 2;
    int16_t y_pos = COVER_Y + (COVER_SIZE - scaled_h) / 2;
    if (x_pos < COVER_X) x_pos = COVER_X;
    if (y_pos < COVER_Y) y_pos = COVER_Y;

    TJpgDec.drawJpg(x_pos, y_pos, jpeg_data, jpeg_size);

    // Restaura escala padrão
    TJpgDec.setJpgScale(1);
}

// ============================================================
// ui_renderer_update_marquee()
// Atualiza animação de scroll dos textos longos.
// Chamar a cada ~30-50ms no loop principal.
// ============================================================
void ui_renderer_update_marquee() {
    if (!tft) return;

    int16_t area_x = TEXT_X_MARGIN;
    int16_t area_w = DISPLAY_WIDTH - TEXT_X_MARGIN - 5;

    // --- Marquee do título ---
    if (s_marquee_title.scrolling) {
        if (s_marquee_title.pause_counter > 0) {
            s_marquee_title.pause_counter--;
        } else {
            s_marquee_title.offset += MARQUEE_SPEED;
            if (s_marquee_title.offset > (s_marquee_title.text_width - area_w + 20)) {
                s_marquee_title.offset = 0;
                s_marquee_title.pause_counter = MARQUEE_PAUSE_FRAMES;
                // Sincroniza: quando título volta, artista volta também
                s_marquee_artist.offset = 0;
                s_marquee_artist.pause_counter = MARQUEE_PAUSE_FRAMES;
            }
        }

        // Usa sprite como buffer para clipping perfeito
        TFT_eSprite spr = TFT_eSprite(tft);
        spr.createSprite(area_w, 20);
        spr.fillSprite(TFT_BLACK);
        spr.setTextFont(2);
        spr.setTextColor(TFT_WHITE, TFT_BLACK);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(s_marquee_title.text, -s_marquee_title.offset, 2);
        spr.pushSprite(area_x, TEXT_Y_TITLE);
        spr.deleteSprite();
    }

    // --- Marquee do artista ---
    if (s_marquee_artist.scrolling) {
        if (s_marquee_artist.pause_counter > 0) {
            s_marquee_artist.pause_counter--;
        } else {
            s_marquee_artist.offset += MARQUEE_SPEED;
            if (s_marquee_artist.offset > (s_marquee_artist.text_width - area_w + 20)) {
                s_marquee_artist.offset = 0;
                s_marquee_artist.pause_counter = MARQUEE_PAUSE_FRAMES;
                // Sincroniza: quando artista volta, título volta também
                s_marquee_title.offset = 0;
                s_marquee_title.pause_counter = MARQUEE_PAUSE_FRAMES;
            }
        }

        TFT_eSprite spr = TFT_eSprite(tft);
        spr.createSprite(area_w, 20);
        spr.fillSprite(TFT_BLACK);
        spr.setTextFont(2);
        spr.setTextColor(0x7BEF, TFT_BLACK);
        spr.setTextDatum(TL_DATUM);
        spr.drawString(s_marquee_artist.text, -s_marquee_artist.offset, 2);
        spr.pushSprite(area_x, TEXT_Y_ARTIST);
        spr.deleteSprite();
    }
}

// ============================================================
// ui_renderer_draw_idle()
// Exibe tela de "Nada tocando" (player inativo)
// ============================================================
void ui_renderer_draw_idle() {
    if (!tft) return;

    display_clear();

    tft->setTextFont(4);
    tft->setTextColor(COLOR_SPOTIFY_GREEN, COLOR_BG);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Spotify", DISPLAY_WIDTH / 2, 100);

    tft->setTextFont(2);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft->drawString("Nada tocando", DISPLAY_WIDTH / 2, 140);
    tft->drawString("Abra o Spotify e toque algo!", DISPLAY_WIDTH / 2, 170);

    tft->setTextDatum(TL_DATUM);

    // Reseta estado dos textos
    s_last_drawn_title = "";
    s_last_drawn_artist = "";
}

// ============================================================
// ui_renderer_draw_paused_indicator()
// Mostra indicador de "Pausado" sobre a tela atual
// ============================================================
void ui_renderer_draw_paused_indicator() {
    if (!tft) return;

    // Ícone de pausa no centro da capa
    int cx = COVER_X + COVER_SIZE / 2;
    int cy = COVER_Y + COVER_SIZE / 2;

    // Retângulo semi-transparente (fundo escuro)
    tft->fillCircle(cx, cy, 20, TFT_BLACK);
    tft->drawCircle(cx, cy, 20, COLOR_TEXT_SECONDARY);

    // Barras de pausa
    tft->fillRect(cx - 8, cy - 10, 5, 20, COLOR_TEXT_PRIMARY);
    tft->fillRect(cx + 3, cy - 10, 5, 20, COLOR_TEXT_PRIMARY);
}

// ============================================================
// ui_renderer_draw_error()
// Exibe tela de erro com mensagem
// ============================================================
void ui_renderer_draw_error(const char* message) {
    if (!tft) return;

    display_clear();

    tft->setTextFont(2);
    tft->setTextColor(TFT_RED, COLOR_BG);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Erro", DISPLAY_WIDTH / 2, 90);

    tft->setTextFont(1);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft->drawString(message, DISPLAY_WIDTH / 2, 120);
    tft->drawString("Tentando novamente...", DISPLAY_WIDTH / 2, 145);

    tft->setTextDatum(TL_DATUM);
}

// ============================================================
// ui_renderer_reset_cache()
// Reseta o cache de textos para forçar redesenho
// ============================================================
void ui_renderer_reset_cache() {
    s_last_drawn_title = "";
    s_last_drawn_artist = "";
}

// ============================================================
// ui_renderer_draw_connecting()
// Exibe tela de "Conectando ao Wi-Fi"
// ============================================================
void ui_renderer_draw_connecting() {
    if (!tft) return;

    display_clear();

    tft->setTextFont(2);
    tft->setTextColor(COLOR_SPOTIFY_GREEN, COLOR_BG);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Spotify Display", DISPLAY_WIDTH / 2, 80);

    tft->setTextFont(1);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft->drawString("Conectando ao Wi-Fi...", DISPLAY_WIDTH / 2, 120);
    tft->drawString("Configure via portal:", DISPLAY_WIDTH / 2, 145);
    tft->drawString("SpotifyDisplay-Setup", DISPLAY_WIDTH / 2, 165);

    tft->setTextDatum(TL_DATUM);
}
