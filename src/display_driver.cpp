// ============================================================
// display_driver.cpp
// ============================================================
// Implementação da camada de display.
// Configuração específica do ST7789 240x240.
// Para trocar para GC9A01 (redondo 240x240):
//   - Altere build_flags no platformio.ini:
//       remova -DST7789_DRIVER=1
//       adicione -DGC9A01_DRIVER=1
//   - Ajuste pinos se necessário
//   - O resto do código permanece inalterado!
// ============================================================

#include "display_driver.h"

// Instância global do TFT_eSPI (configurado via build_flags)
static TFT_eSPI tft = TFT_eSPI(DISPLAY_WIDTH, DISPLAY_HEIGHT);

// ============================================================
// display_init()
// Inicializa o hardware do display e configura o decodificador JPEG
// ============================================================
void display_init() {
    // Inicializa o controlador TFT
    tft.init();

    // Rotação: 0=normal, 1=90°, 2=180°, 3=270°
    // Definido em build_flags como TFT_ROTATION, padrão 0
    #ifdef TFT_ROTATION
        tft.setRotation(TFT_ROTATION);
    #else
        tft.setRotation(0);
    #endif

    // Preenche tela com cor de fundo
    tft.fillScreen(COLOR_BG);

    // Configura backlight (pino definido em build_flags como TFT_BL)
    #ifdef TFT_BL
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH); // Liga backlight
    #endif

    // Configura o TJpg_Decoder
    // Escala JPEG: 1 = sem escala, 2 = metade, 4 = quarto, 8 = oitavo
    TJpgDec.setJpgScale(1);

    // Usa swap de bytes (necessário para displays SPI com TFT_eSPI)
    TJpgDec.setSwapBytes(true);

    // Registra callback para renderizar blocos JPEG direto no display
    TJpgDec.setCallback(display_jpeg_output);

    Serial.println("[DISPLAY] Inicializado com sucesso");
}

// ============================================================
// display_get_tft()
// Retorna ponteiro para a instância TFT para uso por outros módulos
// ============================================================
TFT_eSPI* display_get_tft() {
    return &tft;
}

// ============================================================
// display_backlight()
// Controla o backlight do display
// ============================================================
void display_backlight(bool on) {
    #ifdef TFT_BL
        digitalWrite(TFT_BL, on ? HIGH : LOW);
    #endif
}

// ============================================================
// display_clear()
// Limpa a tela inteira com a cor de fundo
// ============================================================
void display_clear() {
    tft.fillScreen(COLOR_BG);
}

// ============================================================
// display_jpeg_output()
// Callback chamado pelo TJpg_Decoder para cada bloco MCU decodificado.
// Renderiza o bloco diretamente no display via pushImage.
// Isso evita manter um framebuffer completo na RAM.
// ============================================================
bool display_jpeg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // Verifica se o bloco está dentro dos limites do display
    if (y >= DISPLAY_HEIGHT) return false; // Para de decodificar se saiu da tela

    // Empurra o bloco de pixels direto para o display
    tft.pushImage(x, y, w, h, bitmap);

    // Retorna true para continuar decodificando o próximo bloco
    return true;
}
