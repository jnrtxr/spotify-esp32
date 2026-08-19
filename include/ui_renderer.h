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
// Layout estilo "Spotify Car Thing" - paisagem 320x240
// Capa à esquerda, texto à direita, barra embaixo
//
// Capa: 140x140 em X=10, Y=15 (imagem 300/2=150, clipped para 140)
// Textos: começam em X=160 (10+140+10 gap)
// Barra: embaixo, toda a largura

#define COVER_SIZE     150    // Capa grande
#define COVER_X        5      // Margem esquerda
#define COVER_Y        10     // Margem superior

#define TEXT_Y_TITLE   30     // Título à direita da capa
#define TEXT_Y_ARTIST  70     // Artista abaixo do título
#define TEXT_X_MARGIN  165    // Após a capa (5 + 150 + 10)

#define PROGRESS_BAR_Y      195   // Barra embaixo
#define PROGRESS_BAR_HEIGHT 5
#define PROGRESS_BAR_X      10
#define PROGRESS_BAR_W      300

#define TIME_Y              210

// Velocidade do marquee (pixels por frame)
#define MARQUEE_SPEED  1
// Pausa no início/fim do marquee (em frames)
#define MARQUEE_PAUSE_FRAMES 60

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

/// Reseta o cache de textos (forçar redesenho na próxima chamada)
void ui_renderer_reset_cache();

#endif // UI_RENDERER_H
