// ============================================================
// display_driver.h
// ============================================================
// Camada de abstração do display.
// Toda a configuração específica do controlador (ST7789, GC9A01, etc.)
// fica isolada aqui. Para trocar de display, basta:
//   1. Alterar os build_flags no platformio.ini (driver + pinos)
//   2. Ajustar as constantes neste header se necessário
// A lógica principal (ui_renderer) usa apenas a API pública abaixo.
// ============================================================

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ---- Dimensões do display ----
// Altere aqui se trocar para um display de tamanho diferente
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// ---- Cores customizadas (RGB565) ----
#define COLOR_BG         TFT_BLACK
#define COLOR_TEXT_PRIMARY   TFT_WHITE
#define COLOR_TEXT_SECONDARY 0xBDF7  // Cinza claro
#define COLOR_PROGRESS_BG    0x2104  // Cinza escuro
#define COLOR_PROGRESS_FG    0x07E0  // Verde Spotify
#define COLOR_SPOTIFY_GREEN  0x07E0

// ---- API pública ----

/// Inicializa o display (SPI, backlight, rotação, TJpg_Decoder)
void display_init();

/// Retorna ponteiro para a instância TFT (para uso pelo ui_renderer)
TFT_eSPI* display_get_tft();

/// Liga/desliga o backlight
void display_backlight(bool on);

/// Limpa a tela inteira com a cor de fundo padrão
void display_clear();

/// Callback interno do TJpg_Decoder para renderizar blocos JPEG no display.
/// Registrado automaticamente em display_init(). Não chamar diretamente.
bool display_jpeg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

#endif // DISPLAY_DRIVER_H
