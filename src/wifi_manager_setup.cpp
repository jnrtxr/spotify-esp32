// ============================================================
// wifi_manager_setup.cpp
// ============================================================
// Implementação da configuração Wi-Fi via portal cativo.
// Usa a biblioteca WiFiManager de tzapu/tablatronix.
// ============================================================

#include "wifi_manager_setup.h"
#include <WiFi.h>

// Instância do WiFiManager
static WiFiManager wm;

// ============================================================
// wifi_setup_init()
// Tenta conectar com credenciais salvas. Se falhar, abre o portal.
// ============================================================
bool wifi_setup_init() {
    // Define timeout do portal cativo
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT);

    // Desabilita debug verboso (descomente para depurar)
    // wm.setDebugOutput(true);

    // Tema escuro para o portal (mais bonito)
    wm.setDarkMode(true);

    // Título customizado no portal
    wm.setTitle("Spotify Display");

    Serial.println("[WIFI] Tentando conectar...");

    // autoConnect tenta credenciais salvas; se falhar, abre o AP
    // Bloqueia a execução até conectar ou até timeout
    bool connected = wm.autoConnect(AP_NAME, AP_PASSWORD);

    if (connected) {
        Serial.println("[WIFI] Conectado!");
        Serial.print("[WIFI] IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WIFI] Falha ao conectar. Portal expirou.");
    }

    return connected;
}

// ============================================================
// wifi_is_connected()
// Verifica estado atual da conexão
// ============================================================
bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

// ============================================================
// wifi_reconnect()
// Tenta reconectar ao último AP conhecido
// ============================================================
bool wifi_reconnect() {
    Serial.println("[WIFI] Tentando reconectar...");

    WiFi.disconnect();
    delay(1000);
    WiFi.reconnect();

    // Espera até 10 segundos pela reconexão
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WIFI] Reconectado!");
        return true;
    }

    Serial.println("[WIFI] Falha na reconexão.");
    return false;
}

// ============================================================
// wifi_reset_credentials()
// Apaga SSID/senha salvos. Na próxima reinicialização,
// o portal cativo será aberto novamente.
// ============================================================
void wifi_reset_credentials() {
    Serial.println("[WIFI] Resetando credenciais salvas...");
    wm.resetSettings();
}
