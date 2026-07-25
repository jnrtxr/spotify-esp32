// ============================================================
// json_parser.h
// ============================================================
// Módulo de parsing JSON para a resposta da API do Spotify.
// Usa ArduinoJson com filtro de campos para economizar RAM,
// extraindo apenas os campos necessários para exibição.
// ============================================================

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <Arduino.h>

// Estrutura com as informações do track atual
struct TrackInfo {
    String track_name;      // Nome da música
    String artist_name;     // Nome do(s) artista(s) concatenados
    String track_id;        // ID único do track (para detectar mudança)
    String cover_url;       // URL da menor imagem da capa
    uint32_t progress_ms;   // Posição atual de reprodução (ms)
    uint32_t duration_ms;   // Duração total da música (ms)
    bool is_playing;        // true = tocando, false = pausado
};

/// Parseia a resposta JSON da API /v1/me/player/currently-playing.
/// Extrai apenas os campos necessários usando filtro ArduinoJson.
/// Retorna true se o parse foi bem sucedido.
bool json_parser_parse(const String& json_payload);

/// Retorna ponteiro para o TrackInfo parseado.
/// Válido apenas após uma chamada bem sucedida de json_parser_parse().
TrackInfo* json_parser_get_track();

/// Reseta os dados do track (usado quando não há nada tocando)
void json_parser_reset();

#endif // JSON_PARSER_H
