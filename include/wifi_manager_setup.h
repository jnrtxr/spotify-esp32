// ============================================================
// wifi_manager_setup.h
// ============================================================
// Módulo de conexão Wi-Fi usando WiFiManager.
// Na primeira execução, abre um portal cativo (Access Point)
// onde o usuário configura SSID e senha pelo celular/PC.
// Nas vezes seguintes, conecta automaticamente com as
// credenciais salvas na flash.
// ============================================================

#ifndef WIFI_MANAGER_SETUP_H
#define WIFI_MANAGER_SETUP_H

#include <WiFiManager.h>

// Nome do Access Point criado para configuração
#define AP_NAME "SpotifyDisplay-Setup"

// Senha do AP (deixe vazio "" para aberto)
#define AP_PASSWORD ""

// Timeout do portal em segundos (0 = sem timeout)
#define PORTAL_TIMEOUT 180

/// Inicializa Wi-Fi via WiFiManager.
/// Bloqueia até conectar ou até o timeout do portal.
/// Retorna true se conectou com sucesso.
bool wifi_setup_init();

/// Verifica se o Wi-Fi está conectado
bool wifi_is_connected();

/// Tenta reconectar ao Wi-Fi (usado após perda de conexão)
/// Retorna true se reconectou
bool wifi_reconnect();

/// Reseta as credenciais salvas (útil para reconfigurar)
void wifi_reset_credentials();

#endif // WIFI_MANAGER_SETUP_H
