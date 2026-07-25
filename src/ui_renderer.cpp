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
    state.max_visible_w = DISPLAY_WIDTH - (TEXT_X_MARGIN * 2);

    // Calcula largura do texto em pixels
    state.text_width = tft->textWidth(text);
    state.scrolling = (state.text_width > state.max_visible_w);
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
    // Limpa área do título se mudou
    if (track->track_name != s_last_drawn_title) {
        tft->fillRect(0, TEXT_Y_TITLE, DISPLAY_WIDTH, 22, COLOR_BG);
        s_last_drawn_title = track->track_name;

        tft->setTextFont(2); // Fonte média
        tft->setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG);
        initMarquee(s_marquee_title, track->track_name, TEXT_Y_TITLE);

        // Se não precisa de scroll, desenha uma vez
        if (!s_marquee_title.scrolling) {
            // Centraliza texto curto
            tft->setTextDatum(TC_DATUM);
            tft->drawString(track->track_name, DISPLAY_WIDTH / 2, TEXT_Y_TITLE);
            tft->setTextDatum(TL_DATUM);
        }
    }

    // --- Artista ---
    if (track->artist_name != s_last_drawn_artist) {
        tft->fillRect(0, TEXT_Y_ARTIST, DISPLAY_WIDTH, 20, COLOR_BG);
        s_last_drawn_artist = track->artist_name;

        tft->setTextFont(1); // Fonte pequena
        tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
        initMarquee(s_marquee_artist, track->artist_name, TEXT_Y_ARTIST);

        if (!s_marquee_artist.scrolling) {
            tft->setTextDatum(TC_DATUM);
            tft->drawString(track->artist_name, DISPLAY_WIDTH / 2, TEXT_Y_ARTIST);
            tft->setTextDatum(TL_DATUM);
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

    // Define escala para que a imagem caiba em COVER_SIZE x COVER_SIZE
    // Escalas disponíveis no TJpg_Decoder: 1, 2, 4, 8
    uint8_t scale = 1;
    if (img_w > COVER_SIZE * 4 || img_h > COVER_SIZE * 4) scale = 8;
    else if (img_w > COVER_SIZE * 2 || img_h > COVER_SIZE * 2) scale = 4;
    else if (img_w > COVER_SIZE || img_h > COVER_SIZE) scale = 2;

    TJpgDec.setJpgScale(scale);

    // Calcula posição para centralizar a capa na área reservada
    uint16_t scaled_w = img_w / scale;
    uint16_t scaled_h = img_h / scale;
    int16_t x_pos = COVER_X + (COVER_SIZE - scaled_w) / 2;
    int16_t y_pos = COVER_Y + (COVER_SIZE - scaled_h) / 2;

    // Decodifica diretamente para o display (sem framebuffer)
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

    // --- Marquee do título ---
    if (s_marquee_title.scrolling) {
        // Pausa no início e no final
        if (s_marquee_title.pause_counter > 0) {
            s_marquee_title.pause_counter--;
        } else {
            s_marquee_title.offset += MARQUEE_SPEED;

            // Se passou do fim do texto, volta ao início com pausa
            if (s_marquee_title.offset > (s_marquee_title.text_width - s_marquee_title.max_visible_w)) {
                s_marquee_title.offset = 0;
                s_marquee_title.pause_counter = MARQUEE_PAUSE_FRAMES;
            }
        }

        // Redesenha com clipping usando sprite (eficiente)
        tft->setTextFont(2);
        tft->setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG);
        tft->fillRect(TEXT_X_MARGIN, TEXT_Y_TITLE, s_marquee_title.max_visible_w, 22, COLOR_BG);

        // Cria viewport via setViewport (se disponível) ou desenha com offset
        tft->setTextDatum(TL_DATUM);
        // Desenha texto com offset negativo (clipped naturalmente pelas bordas)
        tft->drawString(
            s_marquee_title.text,
            TEXT_X_MARGIN - s_marquee_title.offset,
            TEXT_Y_TITLE
        );
    }

    // --- Marquee do artista ---
    if (s_marquee_artist.scrolling) {
        if (s_marquee_artist.pause_counter > 0) {
            s_marquee_artist.pause_counter--;
        } else {
            s_marquee_artist.offset += MARQUEE_SPEED;

            if (s_marquee_artist.offset > (s_marquee_artist.text_width - s_marquee_artist.max_visible_w)) {
                s_marquee_artist.offset = 0;
                s_marquee_artist.pause_counter = MARQUEE_PAUSE_FRAMES;
            }
        }

        tft->setTextFont(1);
        tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
        tft->fillRect(TEXT_X_MARGIN, TEXT_Y_ARTIST, s_marquee_artist.max_visible_w, 20, COLOR_BG);

        tft->setTextDatum(TL_DATUM);
        tft->drawString(
            s_marquee_artist.text,
            TEXT_X_MARGIN - s_marquee_artist.offset,
            TEXT_Y_ARTIST
        );
    }
}

// ============================================================
// ui_renderer_draw_idle()
// Exibe tela de "Nada tocando" (player inativo)
// ============================================================
void ui_renderer_draw_idle() {
    if (!tft) return;

    display_clear();

    // Ícone de nota musical (simples)
    tft->setTextFont(4);
    tft->setTextColor(COLOR_SPOTIFY_GREEN, COLOR_BG);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Spotify", DISPLAY_WIDTH / 2, 100);

    tft->setTextFont(2);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft->drawString("Nada tocando", DISPLAY_WIDTH / 2, 140);
    tft->drawString("Abra o Spotify e", DISPLAY_WIDTH / 2, 165);
    tft->drawString("toque algo!", DISPLAY_WIDTH / 2, 185);

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
    tft->drawString("Erro", DISPLAY_WIDTH / 2, 100);

    tft->setTextFont(1);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft->drawString(message, DISPLAY_WIDTH / 2, 130);
    tft->drawString("Tentando novamente...", DISPLAY_WIDTH / 2, 155);

    tft->setTextDatum(TL_DATUM);
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
    tft->drawString("Spotify Display", DISPLAY_WIDTH / 2, 90);

    tft->setTextFont(1);
    tft->setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft->drawString("Conectando ao Wi-Fi...", DISPLAY_WIDTH / 2, 130);
    tft->drawString("Configure via portal:", DISPLAY_WIDTH / 2, 155);
    tft->drawString("SpotifyDisplay-Setup", DISPLAY_WIDTH / 2, 175);

    tft->setTextDatum(TL_DATUM);
}
