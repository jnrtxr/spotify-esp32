// ============================================================
// json_parser.cpp
// ============================================================
// Implementação do parser JSON com filtros ArduinoJson.
// O filtro indica ao parser quais campos devem ser armazenados,
// reduzindo drasticamente o uso de RAM ao ignorar campos
// desnecessários (como available_markets, linked_from, etc.)
// ============================================================

#include "json_parser.h"
#include <ArduinoJson.h>

// Estado interno: dados do track atual
static TrackInfo s_track;

// ============================================================
// json_parser_parse()
// Faz parsing da resposta JSON da API do Spotify usando filtro.
//
// A resposta completa do Spotify pode ter > 10 KB, mas com o
// filtro, só processamos os campos que realmente usamos (~500 bytes).
//
// Campos extraídos:
//   - is_playing (bool)
//   - progress_ms (int)
//   - item.id (track ID)
//   - item.name (nome da música)
//   - item.duration_ms (duração)
//   - item.artists[].name (lista de artistas)
//   - item.album.images[] (lista de capas)
// ============================================================
bool json_parser_parse(const String& json_payload) {
    // ---- Definição do filtro ----
    // Apenas os campos listados serão extraídos do JSON.
    // Economiza RAM e acelera o parsing.
    JsonDocument filter;

    filter["is_playing"] = true;
    filter["progress_ms"] = true;

    JsonObject filterItem = filter["item"].to<JsonObject>();
    filterItem["id"] = true;
    filterItem["name"] = true;
    filterItem["duration_ms"] = true;

    // Artistas: só o nome de cada um
    filterItem["artists"][0]["name"] = true;

    // Álbum: só a lista de imagens (url e dimensão)
    JsonObject filterAlbum = filterItem["album"].to<JsonObject>();
    filterAlbum["images"][0]["url"] = true;
    filterAlbum["images"][0]["height"] = true;
    filterAlbum["images"][0]["width"] = true;

    // ---- Parsing com filtro ----
    JsonDocument doc;
    DeserializationError error = deserializeJson(
        doc,
        json_payload,
        DeserializationOption::Filter(filter)
    );

    if (error) {
        Serial.printf("[PARSER] Erro ArduinoJson: %s\n", error.c_str());
        return false;
    }

    // ---- Extração dos campos ----

    // Estado de reprodução
    s_track.is_playing = doc["is_playing"] | false;
    s_track.progress_ms = doc["progress_ms"] | 0;

    // Dados do item (track)
    JsonObject item = doc["item"];
    if (item.isNull()) {
        Serial.println("[PARSER] Campo 'item' ausente no JSON");
        return false;
    }

    s_track.track_id = item["id"].as<String>();
    s_track.track_name = item["name"].as<String>();
    s_track.duration_ms = item["duration_ms"] | 0;

    // ---- Artista(s) ----
    // Concatena nomes separados por ", "
    JsonArray artists = item["artists"];
    s_track.artist_name = "";
    for (size_t i = 0; i < artists.size(); i++) {
        if (i > 0) s_track.artist_name += ", ";
        s_track.artist_name += artists[i]["name"].as<String>();
    }

    // ---- Capa do álbum ----
    // Seleciona a imagem de ~300px (melhor qualidade para o display 120x120).
    // Com PSRAM, a imagem de 300x300 (~15-25 KB JPEG) cabe sem problemas.
    // O TJpg_Decoder reduz com scale=2 (300→150) ficando nítido no display.
    // Fallback: se não achar ~300, pega a maior disponível.
    JsonArray images = item["album"]["images"];
    String bestUrl = "";
    int bestDiff = 99999;

    for (size_t i = 0; i < images.size(); i++) {
        int h = images[i]["height"] | 0;
        String url = images[i]["url"].as<String>();
        // Preferência por imagem próxima de 300px
        int diff = abs(h - 300);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestUrl = url;
        }
    }
    s_track.cover_url = bestUrl;

    Serial.printf("[PARSER] Track: %s | Artista: %s | Progresso: %u/%u ms\n",
                  s_track.track_name.c_str(),
                  s_track.artist_name.c_str(),
                  s_track.progress_ms,
                  s_track.duration_ms);

    return true;
}

// ============================================================
// json_parser_get_track()
// Retorna ponteiro para os dados do track parseado
// ============================================================
TrackInfo* json_parser_get_track() {
    return &s_track;
}

// ============================================================
// json_parser_reset()
// Limpa os dados armazenados (usado quando não há nada tocando)
// ============================================================
void json_parser_reset() {
    s_track.track_name = "";
    s_track.artist_name = "";
    s_track.track_id = "";
    s_track.cover_url = "";
    s_track.progress_ms = 0;
    s_track.duration_ms = 0;
    s_track.is_playing = false;
}
