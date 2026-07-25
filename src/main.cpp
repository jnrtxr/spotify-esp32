// ============================================================
// main.cpp
// ============================================================
// Ponto de entrada do firmware ESP32 Spotify Now Playing.
// Orquestra todos os módulos usando uma máquina de estados.
//
// Estados:
//   BOOT        → Inicializa periféricos
//   WIFI_SETUP  → Conecta via WiFiManager (portal cativo)
//   AUTH_INIT   → Carrega credenciais e obtém primeiro token
//   POLLING     → Loop principal: poll + render + interpolação
//   ERROR       → Exibe erro, tenta recuperar
//
// O loop principal avança a barra de progresso localmente usando
// millis(), sem gastar requisição HTTP. Só resincroniza no poll.
// ============================================================

#include <Arduino.h>
#include <WiFi.h>

// Módulos do projeto
#include "display_driver.h"
#include "wifi_manager_setup.h"
#include "spotify_auth.h"
#include "spotify_poller.h"
#include "json_parser.h"
#include "ui_renderer.h"

// ============================================================
// Máquina de estados principal
// ============================================================
enum AppState {
    STATE_BOOT,
    STATE_WIFI_SETUP,
    STATE_AUTH_INIT,
    STATE_POLLING,
    STATE_ERROR
};

static AppState s_state = STATE_BOOT;

// ---- Timers ----
static unsigned long s_last_poll_time = 0;         // Quando foi o último poll
static unsigned long s_last_marquee_time = 0;      // Timer do marquee
static unsigned long s_last_progress_time = 0;     // Timer da barra de progresso
static unsigned long s_progress_sync_millis = 0;   // millis() no momento do último sync

// ---- Estado de reprodução local ----
static uint32_t s_local_progress_ms = 0;   // Progresso interpolado localmente
static uint32_t s_duration_ms = 0;         // Duração do track atual
static bool s_is_playing = false;          // Se está tocando (para interpolação)
static bool s_has_track = false;           // Se há algo para mostrar

// ---- Controle de erro ----
static int s_error_count = 0;
static const int MAX_ERRORS_BEFORE_RECONNECT = 5;

// ---- Intervalo de atualização da UI ----
#define MARQUEE_UPDATE_MS   50    // Atualiza marquee a cada 50ms (~20fps)
#define PROGRESS_UPDATE_MS  1000  // Atualiza barra a cada 1 segundo

// ============================================================
// Funções auxiliares de configuração Serial
// ============================================================

/// Verifica se há comandos via Serial para gravar credenciais.
/// Formato: SET_CREDS:client_id:client_secret:refresh_token
void check_serial_commands() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.startsWith("SET_CREDS:")) {
            // Remove prefixo
            input = input.substring(10);

            // Separa por ':'
            int sep1 = input.indexOf(':');
            int sep2 = input.indexOf(':', sep1 + 1);

            if (sep1 > 0 && sep2 > sep1) {
                String client_id = input.substring(0, sep1);
                String client_secret = input.substring(sep1 + 1, sep2);
                String refresh_token = input.substring(sep2 + 1);

                Serial.println("[MAIN] Salvando credenciais na NVS...");
                spotify_auth_save_credentials(
                    client_id.c_str(),
                    client_secret.c_str(),
                    refresh_token.c_str()
                );
                Serial.println("[MAIN] Credenciais salvas! Reiniciando...");
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("[MAIN] Formato invalido!");
                Serial.println("[MAIN] Use: SET_CREDS:client_id:client_secret:refresh_token");
            }
        }
        else if (input == "RESET_WIFI") {
            Serial.println("[MAIN] Resetando credenciais Wi-Fi...");
            wifi_reset_credentials();
            Serial.println("[MAIN] Reiniciando...");
            delay(1000);
            ESP.restart();
        }
        else if (input == "STATUS") {
            Serial.printf("[MAIN] Estado: %d\n", s_state);
            Serial.printf("[MAIN] Wi-Fi: %s\n", wifi_is_connected() ? "OK" : "Desconectado");
            Serial.printf("[MAIN] Token: %s\n", spotify_auth_token_expired() ? "Expirado" : "Válido");
            Serial.printf("[MAIN] Free heap: %u bytes\n", ESP.getFreeHeap());
            Serial.printf("[MAIN] Free PSRAM: %u bytes\n", ESP.getFreePsram());
        }
    }
}

// ============================================================
// handle_state_boot()
// Inicializa todos os periféricos e módulos
// ============================================================
void handle_state_boot() {
    Serial.println("\n====================================");
    Serial.println("  ESP32 Spotify Now Playing");
    Serial.println("====================================");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.println();

    // Inicializa display
    display_init();
    ui_renderer_init();

    // Mostra tela de "Conectando..."
    ui_renderer_draw_connecting();

    // Inicializa poller
    spotify_poller_init();

    // Próximo estado
    s_state = STATE_WIFI_SETUP;
}

// ============================================================
// handle_state_wifi_setup()
// Conecta via WiFiManager (portal cativo na primeira vez)
// ============================================================
void handle_state_wifi_setup() {
    bool connected = wifi_setup_init();

    if (connected) {
        Serial.println("[MAIN] Wi-Fi OK! Iniciando autenticação...");
        s_state = STATE_AUTH_INIT;
    } else {
        Serial.println("[MAIN] Wi-Fi falhou. Reiniciando portal em 5s...");
        ui_renderer_draw_error("Wi-Fi desconectado");
        delay(5000);
        ESP.restart(); // Reinicia para reabrir o portal
    }
}

// ============================================================
// handle_state_auth_init()
// Carrega credenciais e tenta obter o primeiro access_token
// ============================================================
void handle_state_auth_init() {
    // Carrega credenciais da NVS
    bool hasCredentials = spotify_auth_init();

    if (!hasCredentials) {
        Serial.println("[MAIN] Sem credenciais! Envie via Serial:");
        Serial.println("[MAIN] SET_CREDS:client_id:client_secret:refresh_token");
        ui_renderer_draw_error("Credenciais ausentes");
        ui_renderer_draw_connecting(); // Hint about setup

        // Fica em loop esperando comandos Serial
        while (!spotify_auth_has_credentials()) {
            check_serial_commands();
            delay(100);
        }
    }

    // Tenta obter o primeiro access_token
    Serial.println("[MAIN] Obtendo access_token...");
    bool tokenOk = spotify_auth_refresh();

    if (tokenOk) {
        Serial.println("[MAIN] Autenticação OK! Iniciando polling...");
        s_state = STATE_POLLING;
        s_last_poll_time = 0; // Força primeiro poll imediato
        s_error_count = 0;
    } else {
        Serial.println("[MAIN] Falha na autenticação. Verifique credenciais.");
        ui_renderer_draw_error("Auth falhou");
        delay(5000);
        s_state = STATE_ERROR;
    }
}

// ============================================================
// handle_state_polling()
// Loop principal: poll + render + interpolação local de progresso
// ============================================================
void handle_state_polling() {
    unsigned long now = millis();

    // ---- Verifica conexão Wi-Fi ----
    if (!wifi_is_connected()) {
        Serial.println("[MAIN] Wi-Fi perdido! Tentando reconectar...");
        ui_renderer_draw_error("Wi-Fi perdido...");

        if (!wifi_reconnect()) {
            s_error_count++;
            if (s_error_count > MAX_ERRORS_BEFORE_RECONNECT) {
                Serial.println("[MAIN] Muitos erros. Reiniciando...");
                ESP.restart();
            }
            delay(2000);
            return;
        }
        s_error_count = 0;
    }

    // ---- Poll da API (a cada intervalo) ----
    unsigned long poll_interval = spotify_poller_get_next_interval();
    if (now - s_last_poll_time >= poll_interval) {
        s_last_poll_time = now;

        PollResult result = spotify_poller_poll();

        switch (result) {
            case POLL_OK_PLAYING:
            case POLL_OK_PAUSED: {
                s_error_count = 0;
                TrackInfo* track = json_parser_get_track();

                // Atualiza estado local
                s_is_playing = track->is_playing;
                s_duration_ms = track->duration_ms;
                s_local_progress_ms = track->progress_ms;
                s_progress_sync_millis = millis(); // Marca momento do sync
                s_has_track = true;

                // Se o track mudou, baixa nova capa
                if (spotify_poller_track_changed()) {
                    // Redesenha tela completa
                    display_clear();

                    // Baixa e renderiza a capa
                    uint8_t* cover_buf = nullptr;
                    uint32_t cover_size = spotify_poller_download_cover(track->cover_url, &cover_buf);
                    if (cover_size > 0 && cover_buf) {
                        ui_renderer_draw_cover(cover_buf, cover_size);
                        spotify_poller_free_cover(cover_buf);
                    }

                    // Desenha informações do track
                    ui_renderer_draw_track(track);
                } else {
                    // Mesmo track: atualiza progresso e estado
                    ui_renderer_update_progress(track->progress_ms, track->duration_ms, track->is_playing);
                }

                // Indicador de pausa
                if (!track->is_playing) {
                    ui_renderer_draw_paused_indicator();
                }
                break;
            }

            case POLL_NO_CONTENT:
                // Nada tocando
                if (s_has_track) { // Só redesenha se mudou
                    ui_renderer_draw_idle();
                    s_has_track = false;
                    s_is_playing = false;
                }
                s_error_count = 0;
                break;

            case POLL_TOKEN_EXPIRED:
                // Token expirado, tenta renovar
                Serial.println("[MAIN] Token expirado, renovando...");
                if (!spotify_auth_refresh()) {
                    s_error_count++;
                    ui_renderer_draw_error("Token expirado");
                }
                break;

            case POLL_RATE_LIMITED:
                // Rate limited - o poller já ajustou o intervalo
                Serial.println("[MAIN] Rate limited, aguardando...");
                break;

            case POLL_ERROR_NETWORK:
                s_error_count++;
                if (s_error_count > MAX_ERRORS_BEFORE_RECONNECT) {
                    ui_renderer_draw_error("Rede instável");
                }
                break;

            case POLL_ERROR_PARSE:
            case POLL_ERROR_OTHER:
                s_error_count++;
                if (s_error_count > MAX_ERRORS_BEFORE_RECONNECT * 2) {
                    s_state = STATE_ERROR;
                }
                break;
        }
    }

    // ---- Interpolação local da barra de progresso ----
    // Avança o progresso usando millis() entre polls
    if (s_has_track && s_is_playing) {
        if (now - s_last_progress_time >= PROGRESS_UPDATE_MS) {
            s_last_progress_time = now;

            // Calcula progresso interpolado
            unsigned long elapsed_since_sync = now - s_progress_sync_millis;
            uint32_t interpolated = s_local_progress_ms + elapsed_since_sync;

            // Limita ao máximo da duração
            if (interpolated > s_duration_ms) {
                interpolated = s_duration_ms;
            }

            // Atualiza barra de progresso sem fazer HTTP
            ui_renderer_update_progress(interpolated, s_duration_ms, true);
        }
    }

    // ---- Atualiza animação marquee ----
    if (s_has_track && (now - s_last_marquee_time >= MARQUEE_UPDATE_MS)) {
        s_last_marquee_time = now;
        ui_renderer_update_marquee();
    }

    // ---- Verifica comandos Serial ----
    check_serial_commands();
}

// ============================================================
// handle_state_error()
// Estado de erro: exibe mensagem e tenta recuperar
// ============================================================
void handle_state_error() {
    Serial.println("[MAIN] Estado de ERRO. Tentando recuperar em 10s...");
    ui_renderer_draw_error("Erro persistente");

    delay(10000);

    // Tenta reconectar tudo
    if (wifi_is_connected() || wifi_reconnect()) {
        if (spotify_auth_refresh()) {
            s_error_count = 0;
            s_state = STATE_POLLING;
            s_last_poll_time = 0;
            return;
        }
    }

    // Se não conseguiu, reinicia
    Serial.println("[MAIN] Recuperação falhou. Reiniciando...");
    ESP.restart();
}

// ============================================================
// setup()
// Inicialização do Arduino (chamado uma vez)
// ============================================================
void setup() {
    // Inicializa Serial
    Serial.begin(115200);
    delay(500); // Aguarda estabilização

    Serial.println();
    Serial.println("[MAIN] Iniciando...");

    // Começa na máquina de estados
    s_state = STATE_BOOT;
}

// ============================================================
// loop()
// Loop principal do Arduino.
// Chama o handler do estado atual.
// ============================================================
void loop() {
    switch (s_state) {
        case STATE_BOOT:
            handle_state_boot();
            break;

        case STATE_WIFI_SETUP:
            handle_state_wifi_setup();
            break;

        case STATE_AUTH_INIT:
            handle_state_auth_init();
            break;

        case STATE_POLLING:
            handle_state_polling();
            break;

        case STATE_ERROR:
            handle_state_error();
            break;
    }

    // Yield para o watchdog e tarefas internas do ESP32
    yield();
}
