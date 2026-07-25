// ============================================================
// spotify_auth.h
// ============================================================
// Gerenciamento de autenticação OAuth2 do Spotify.
// - Armazena client_id, client_secret e refresh_token na NVS
// - Renova o access_token automaticamente quando expira
// - Não faz o login inicial (isso é feito pelo script Python)
// ============================================================

#ifndef SPOTIFY_AUTH_H
#define SPOTIFY_AUTH_H

#include <Arduino.h>

// URL do endpoint de token do Spotify
#define SPOTIFY_TOKEN_URL "https://accounts.spotify.com/api/token"

// Margem de segurança: renova o token 60s antes de expirar
#define TOKEN_EXPIRY_MARGIN_MS 60000

/// Inicializa o módulo de autenticação.
/// Carrega credenciais da NVS. Se não existirem, tenta de secrets.h.
/// Retorna true se as credenciais estão disponíveis.
bool spotify_auth_init();

/// Verifica se há credenciais válidas na NVS
bool spotify_auth_has_credentials();

/// Salva credenciais na NVS (usado pelo portal de configuração ou Serial)
void spotify_auth_save_credentials(const char* client_id,
                                     const char* client_secret,
                                     const char* refresh_token);

/// Retorna o access_token atual (pode estar expirado!)
String spotify_auth_get_access_token();

/// Verifica se o token precisa ser renovado
bool spotify_auth_token_expired();

/// Renova o access_token usando o refresh_token.
/// Retorna true se renovou com sucesso.
bool spotify_auth_refresh();

/// Garante que o access_token é válido (renova se necessário).
/// Retorna true se o token está pronto para uso.
bool spotify_auth_ensure_valid_token();

#endif // SPOTIFY_AUTH_H
