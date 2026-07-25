// ============================================================
// spotify_auth.cpp
// ============================================================
// Implementação do gerenciamento de tokens OAuth2 do Spotify.
// Usa a biblioteca Preferences (NVS) para persistir credenciais.
// ============================================================

#include "spotify_auth.h"
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>  // Biblioteca nativa do ESP32

// Namespace da NVS para credenciais Spotify
#define NVS_NAMESPACE "spotify"

// Chaves NVS
#define NVS_KEY_CLIENT_ID     "client_id"
#define NVS_KEY_CLIENT_SECRET "client_secret"
#define NVS_KEY_REFRESH_TOKEN "refresh_token"

// Variáveis internas do módulo
static String s_client_id;
static String s_client_secret;
static String s_refresh_token;
static String s_access_token;
static unsigned long s_token_expiry_ms = 0;  // millis() quando expira

// Certificado raiz do Spotify (Let's Encrypt / DigiCert)
// Necessário para HTTPS. Alternativamente, use setInsecure().
static const char* SPOTIFY_ROOT_CA = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
    "NFtY2PwByVS5uCbMiogZiUwwg7zLYIbyDwfS/VhRPczOBYMLU2JOljc+UqN4HeJY\n"
    "LlbiCbIcFE1bnYRFdWXBTDIUBRkHYmvPbGd/wl9cjHHLYCQ0MHFGA1UdEQQOMIHL\n"
    "LubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "-----END CERTIFICATE-----\n";

// ============================================================
// spotify_auth_init()
// Carrega credenciais da NVS. Se não existirem, tenta compilar
// com secrets.h (para primeira gravação).
// ============================================================
bool spotify_auth_init() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true); // Modo somente leitura

    s_client_id     = prefs.getString(NVS_KEY_CLIENT_ID, "");
    s_client_secret = prefs.getString(NVS_KEY_CLIENT_SECRET, "");
    s_refresh_token = prefs.getString(NVS_KEY_REFRESH_TOKEN, "");

    prefs.end();

    // Se a NVS está vazia, tenta usar valores de secrets.h (se existir)
    #if __has_include("secrets.h")
        #include "secrets.h"
        if (s_client_id.isEmpty()) {
            Serial.println("[AUTH] NVS vazia, usando valores de secrets.h");
            spotify_auth_save_credentials(
                SPOTIFY_CLIENT_ID,
                SPOTIFY_CLIENT_SECRET,
                SPOTIFY_REFRESH_TOKEN
            );
        }
    #endif

    if (s_client_id.isEmpty() || s_client_secret.isEmpty() || s_refresh_token.isEmpty()) {
        Serial.println("[AUTH] ERRO: Credenciais não encontradas!");
        Serial.println("[AUTH] Use o script Python para obter o refresh_token");
        Serial.println("[AUTH] e grave via Serial ou portal de configuração.");
        return false;
    }

    Serial.println("[AUTH] Credenciais carregadas da NVS");
    return true;
}

// ============================================================
// spotify_auth_has_credentials()
// Verifica se todas as credenciais necessárias estão presentes
// ============================================================
bool spotify_auth_has_credentials() {
    return !s_client_id.isEmpty() &&
           !s_client_secret.isEmpty() &&
           !s_refresh_token.isEmpty();
}

// ============================================================
// spotify_auth_save_credentials()
// Persiste credenciais na NVS (flash, sobrevive a reboot)
// ============================================================
void spotify_auth_save_credentials(const char* client_id,
                                     const char* client_secret,
                                     const char* refresh_token) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false); // Modo leitura/escrita

    prefs.putString(NVS_KEY_CLIENT_ID, client_id);
    prefs.putString(NVS_KEY_CLIENT_SECRET, client_secret);
    prefs.putString(NVS_KEY_REFRESH_TOKEN, refresh_token);

    prefs.end();

    // Atualiza variáveis em memória
    s_client_id     = client_id;
    s_client_secret = client_secret;
    s_refresh_token = refresh_token;

    Serial.println("[AUTH] Credenciais salvas na NVS");
}

// ============================================================
// spotify_auth_get_access_token()
// Retorna o token atual (pode estar expirado)
// ============================================================
String spotify_auth_get_access_token() {
    return s_access_token;
}

// ============================================================
// spotify_auth_token_expired()
// Verifica se o token precisa ser renovado
// ============================================================
bool spotify_auth_token_expired() {
    // Se não temos token, está "expirado"
    if (s_access_token.isEmpty()) return true;

    // Verifica com margem de segurança
    return millis() >= (s_token_expiry_ms - TOKEN_EXPIRY_MARGIN_MS);
}

// ============================================================
// spotify_auth_refresh()
// Faz POST em /api/token para obter novo access_token
// usando o refresh_token salvo.
// ============================================================
bool spotify_auth_refresh() {
    Serial.println("[AUTH] Renovando access_token...");

    WiFiClientSecure client;
    // Usa setInsecure() para evitar problemas com certificados expirados.
    // Em produção, considere fixar o certificado raiz.
    client.setInsecure();

    HTTPClient http;
    http.begin(client, SPOTIFY_TOKEN_URL);

    // Header de autenticação Basic (client_id:client_secret em Base64)
    String credentials = s_client_id + ":" + s_client_secret;
    String encoded = base64::encode(credentials);
    http.addHeader("Authorization", "Basic " + encoded);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Corpo da requisição
    String body = "grant_type=refresh_token&refresh_token=" + s_refresh_token;

    int httpCode = http.POST(body);

    if (httpCode == 200) {
        String payload = http.getString();

        // Parse da resposta JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("[AUTH] Erro ao parsear JSON: ");
            Serial.println(error.c_str());
            http.end();
            return false;
        }

        // Extrai o novo access_token
        s_access_token = doc["access_token"].as<String>();

        // Calcula quando o token expira (em millis)
        int expires_in = doc["expires_in"] | 3600; // Padrão 1 hora
        s_token_expiry_ms = millis() + (expires_in * 1000UL);

        // Se o Spotify retornou um novo refresh_token, atualiza
        if (doc["refresh_token"].is<const char*>()) {
            String new_refresh = doc["refresh_token"].as<String>();
            if (new_refresh.length() > 0 && new_refresh != s_refresh_token) {
                Serial.println("[AUTH] Novo refresh_token recebido, atualizando NVS");
                s_refresh_token = new_refresh;
                // Salva o novo refresh_token na NVS
                Preferences prefs;
                prefs.begin(NVS_NAMESPACE, false);
                prefs.putString(NVS_KEY_REFRESH_TOKEN, s_refresh_token.c_str());
                prefs.end();
            }
        }

        Serial.println("[AUTH] Token renovado com sucesso!");
        Serial.printf("[AUTH] Expira em %d segundos\n", expires_in);

        http.end();
        return true;
    } else {
        Serial.printf("[AUTH] Erro HTTP %d ao renovar token\n", httpCode);
        if (httpCode > 0) {
            Serial.println(http.getString());
        }
        http.end();
        return false;
    }
}

// ============================================================
// spotify_auth_ensure_valid_token()
// Garante que temos um access_token válido antes de fazer requests.
// Renova automaticamente se expirado.
// ============================================================
bool spotify_auth_ensure_valid_token() {
    if (!spotify_auth_has_credentials()) {
        return false;
    }

    if (spotify_auth_token_expired()) {
        return spotify_auth_refresh();
    }

    return true; // Token ainda válido
}
