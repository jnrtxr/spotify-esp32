// ============================================================
// spotify_poller.cpp
// ============================================================
// Implementação do polling da API do Spotify.
// Faz GET em /v1/me/player/currently-playing e gerencia
// download de capas de álbum via PSRAM.
// ============================================================

#include "spotify_poller.h"
#include "spotify_auth.h"
#include "json_parser.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---- Estado interno ----
static unsigned long s_next_poll_interval = POLL_INTERVAL_MS;
static String s_last_track_id = "";
static bool s_track_changed = false;

// ============================================================
// spotify_poller_init()
// Inicializa variáveis do módulo
// ============================================================
void spotify_poller_init() {
    s_last_track_id = "";
    s_track_changed = false;
    s_next_poll_interval = POLL_INTERVAL_MS;
    Serial.println("[POLLER] Inicializado");
}

// ============================================================
// spotify_poller_poll()
// Faz GET na API do Spotify para obter a música atual.
// Lida com os vários códigos de resposta HTTP.
// ============================================================
PollResult spotify_poller_poll() {
    // Garante token válido antes de fazer a requisição
    if (!spotify_auth_ensure_valid_token()) {
        Serial.println("[POLLER] Falha ao obter token válido");
        return POLL_TOKEN_EXPIRED;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Sem validação de certificado (simplificação)

    HTTPClient http;
    http.begin(client, SPOTIFY_NOW_PLAYING_URL);

    // Header de autorização com Bearer token
    String authHeader = "Bearer " + spotify_auth_get_access_token();
    http.addHeader("Authorization", authHeader);

    // Aceita JSON
    http.addHeader("Accept", "application/json");

    // Faz a requisição GET
    int httpCode = http.GET();

    // ---- Trata os códigos de resposta ----

    // 204 No Content = nada tocando
    if (httpCode == 204) {
        Serial.println("[POLLER] 204 - Nada tocando");
        http.end();
        s_next_poll_interval = POLL_INTERVAL_MS;
        return POLL_NO_CONTENT;
    }

    // 401 Unauthorized = token expirado
    if (httpCode == 401) {
        Serial.println("[POLLER] 401 - Token expirado");
        http.end();
        s_next_poll_interval = 1000; // Tenta renovar rápido
        return POLL_TOKEN_EXPIRED;
    }

    // 429 Too Many Requests = rate limit
    if (httpCode == 429) {
        // Respeita o header Retry-After (em segundos)
        String retryAfter = http.header("Retry-After");
        int waitSec = retryAfter.toInt();
        if (waitSec <= 0) waitSec = 30; // Padrão seguro
        s_next_poll_interval = waitSec * 1000UL;

        Serial.printf("[POLLER] 429 - Rate limited. Aguardando %d segundos\n", waitSec);
        http.end();
        return POLL_RATE_LIMITED;
    }

    // 200 OK = resposta com dados
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();

        // Parseia a resposta JSON
        bool parseOk = json_parser_parse(payload);
        if (!parseOk) {
            Serial.println("[POLLER] Erro ao parsear resposta JSON");
            s_next_poll_interval = POLL_INTERVAL_MS;
            return POLL_ERROR_PARSE;
        }

        // Verifica se o track mudou
        TrackInfo* track = json_parser_get_track();
        if (track->track_id != s_last_track_id) {
            s_last_track_id = track->track_id;
            s_track_changed = true;
            Serial.printf("[POLLER] Novo track: %s - %s\n",
                          track->artist_name.c_str(),
                          track->track_name.c_str());
        }

        s_next_poll_interval = POLL_INTERVAL_MS;

        // Retorna estado baseado em is_playing
        return track->is_playing ? POLL_OK_PLAYING : POLL_OK_PAUSED;
    }

    // Erros de conexão (httpCode negativo)
    if (httpCode < 0) {
        Serial.printf("[POLLER] Erro de rede: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        s_next_poll_interval = 5000; // Espera um pouco mais
        return POLL_ERROR_NETWORK;
    }

    // Qualquer outro código HTTP
    Serial.printf("[POLLER] Erro HTTP inesperado: %d\n", httpCode);
    http.end();
    s_next_poll_interval = POLL_INTERVAL_MS;
    return POLL_ERROR_OTHER;
}

// ============================================================
// spotify_poller_get_next_interval()
// Retorna o intervalo até o próximo poll (respeita rate limit)
// ============================================================
unsigned long spotify_poller_get_next_interval() {
    return s_next_poll_interval;
}

// ============================================================
// spotify_poller_track_changed()
// Indica se o track mudou desde a última verificação.
// Consumido (resetado) após leitura.
// ============================================================
bool spotify_poller_track_changed() {
    bool changed = s_track_changed;
    s_track_changed = false; // Reseta flag
    return changed;
}

// ============================================================
// spotify_poller_download_cover()
// Baixa a imagem JPEG da capa do álbum para um buffer na PSRAM.
// Usa ps_malloc() para alocar em PSRAM (evita fragmentar a RAM principal).
// Retorna o tamanho dos dados baixados, ou 0 em caso de erro.
// O chamador deve liberar o buffer com spotify_poller_free_cover().
// ============================================================
uint32_t spotify_poller_download_cover(const String& url, uint8_t** out_buffer) {
    if (url.isEmpty()) {
        Serial.println("[POLLER] URL da capa vazia");
        return 0;
    }

    Serial.printf("[POLLER] Baixando capa: %s\n", url.c_str());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Accept", "image/jpeg");

    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("[POLLER] Erro ao baixar capa: HTTP %d\n", httpCode);
        http.end();
        return 0;
    }

    // Obtém o tamanho do conteúdo
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        // Se o servidor não informou o tamanho, usa chunked
        contentLength = COVER_BUFFER_MAX_SIZE;
    }

    if (contentLength > COVER_BUFFER_MAX_SIZE) {
        Serial.printf("[POLLER] Capa muito grande: %d bytes\n", contentLength);
        http.end();
        return 0;
    }

    // Aloca buffer na PSRAM
    uint8_t* buffer = (uint8_t*)ps_malloc(contentLength);
    if (!buffer) {
        Serial.println("[POLLER] Falha ao alocar PSRAM para capa");
        http.end();
        return 0;
    }

    // Lê os dados do stream
    WiFiClient* stream = http.getStreamPtr();
    uint32_t bytesRead = 0;
    uint32_t timeout = millis() + 10000; // Timeout de 10s

    while (http.connected() && (bytesRead < (uint32_t)contentLength)) {
        // Verifica timeout
        if (millis() > timeout) {
            Serial.println("[POLLER] Timeout ao baixar capa");
            free(buffer);
            http.end();
            return 0;
        }

        size_t available = stream->available();
        if (available) {
            size_t toRead = min(available, (size_t)(contentLength - bytesRead));
            size_t read = stream->readBytes(buffer + bytesRead, toRead);
            bytesRead += read;
        } else {
            delay(1); // Yield
        }
    }

    http.end();

    if (bytesRead == 0) {
        free(buffer);
        return 0;
    }

    Serial.printf("[POLLER] Capa baixada: %u bytes\n", bytesRead);
    *out_buffer = buffer;
    return bytesRead;
}

// ============================================================
// spotify_poller_free_cover()
// Libera o buffer de PSRAM usado pela capa
// ============================================================
void spotify_poller_free_cover(uint8_t* buffer) {
    if (buffer) {
        free(buffer);
    }
}
