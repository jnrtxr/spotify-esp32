// ============================================================
// ui_renderer.h
// ============================================================
// Módulo de renderização da interface no display.
// Gerencia layout, efeito marquee no texto, barra de progresso
// e exibição da capa do álbum.
//
// Layout (240x240):
// ┌────────────────────────┐
// │                        │
// │     ┌──────────┐       │
// │     │  CAPA    │       │  ← Capa 120x120 centralizada (topo)
// │     │  120x120 │       │
// │     └──────────┘       │
// │                        │
// │  Título da Música      │  ← Marquee se não couber
// │  Nome do Artista       │
// │                        │
// │  ▓▓▓▓▓▓▓░░░░░░░░░░░░  │  ← Barra de progresso
// │  1:23         3:45     │  ← Tempo decorrido / total
// └────────────────────────┘
// ============================================================

#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <Arduino.h>
#include "json_parser.h"

// ---- Dimensões do layout ----
#define COVER_SIZE     120    // Capa 120x120 no display
#define COVER_X        60     // Posição X da capa (centralizada em 240)
#define COVER_Y        10     // Posição Y da capa

#define TEXT_Y_TITLE   140    // Y do título
#define TEXT_Y_ARTIST  165    // Y do artista
#define TEXT_X_MARGIN  8      // Margem lateral dos textos

#define PROGRESS_BAR_Y      195   // Y da barra de progresso
#define PROGRESS_BAR_HEIGHT 6     // Altura da barra
#define PROGRESS_BAR_X      10    // X inicial
#define PROGRESS_BAR_W      220   // Largura total

#define TIME_Y              210   // Y dos textos de tempo

// Velocidade do marquee (pixels por frame)
#define MARQUEE_SPEED  2
// Pausa no início/fim do marquee (em frames)
#define MARQUEE_PAUSE_FRAMES 30

/// Inicializa o módulo de UI (configura fontes, limpa tela)
void ui_renderer_init();

/// Desenha a tela completa com os dados do track
void ui_renderer_draw_track(TrackInfo* track);

/// Atualiza apenas a barra de progresso (chamado entre polls)
void ui_renderer_update_progress(uint32_t progress_ms, uint32_t duration_ms, bool is_playing);

/// Desenha a capa do álbum a partir de um buffer JPEG
void ui_renderer_draw_cover(uint8_t* jpeg_data, uint32_t jpeg_size);

/// Atualiza o efeito marquee (chamar no loop principal)
void ui_renderer_update_marquee();

/// Exibe tela de "Nada tocando"
void ui_renderer_draw_idle();

/// Exibe tela de "Pausado" (overlay sobre a tela atual)
void ui_renderer_draw_paused_indicator();

/// Exibe tela de erro (com mensagem)
void ui_renderer_draw_error(const char* message);

/// Exibe tela de "Conectando..."
void ui_renderer_draw_connecting();

#endif // UI_RENDERER_H
