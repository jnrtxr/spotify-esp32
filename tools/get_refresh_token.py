#!/usr/bin/env python3
# ============================================================
# get_refresh_token.py
# ============================================================
# Script auxiliar para obter o refresh_token do Spotify.
# Roda UMA VEZ no PC para fazer o login OAuth (Authorization Code Flow).
#
# O ESP32 não consegue fazer o login interativo (precisa de navegador),
# então usamos este script para:
#   1. Abrir o navegador no login do Spotify
#   2. Receber o callback com o authorization code
#   3. Trocar o code por access_token + refresh_token
#   4. Imprimir o refresh_token para você copiar/gravar na ESP32
#
# Requisitos:
#   pip install requests
#
# Uso:
#   python get_refresh_token.py
#
# Configuração:
#   1. Crie um app em https://developer.spotify.com/dashboard
#   2. Na aba Settings, adicione http://127.0.0.1:8888/callback
#      como Redirect URI (Spotify não aceita mais "localhost")
#   3. Copie Client ID e Client Secret
#   4. Cole abaixo ou passe como variáveis de ambiente
# ============================================================

import os
import sys
import webbrowser
import urllib.parse
import http.server
import threading
import requests
import base64

# ============================================================
# CONFIGURAÇÃO - Preencha ou use variáveis de ambiente
# ============================================================
CLIENT_ID = os.environ.get("SPOTIFY_CLIENT_ID", "SEU_CLIENT_ID_AQUI")
CLIENT_SECRET = os.environ.get("SPOTIFY_CLIENT_SECRET", "SEU_CLIENT_SECRET_AQUI")
REDIRECT_URI = "http://127.0.0.1:8888/callback"

# Escopos necessários (somente leitura do player)
SCOPES = "user-read-currently-playing user-read-playback-state"

# Porta do servidor local para receber o callback
PORT = 8888

# ============================================================
# Variável global para capturar o authorization code
# ============================================================
auth_code = None


class CallbackHandler(http.server.BaseHTTPRequestHandler):
    """Servidor HTTP temporário que captura o callback do Spotify."""

    def do_GET(self):
        global auth_code

        # Parseia a query string
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)

        if "code" in params:
            auth_code = params["code"][0]
            # Responde ao navegador
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            response = """
            <html><body style="font-family:Arial;text-align:center;padding:50px;background:#191414;color:#1DB954">
            <h1>✓ Login realizado com sucesso!</h1>
            <p style="color:#fff">Volte ao terminal para copiar o refresh_token.</p>
            <p style="color:#fff">Pode fechar esta aba.</p>
            </body></html>
            """
            self.wfile.write(response.encode())
        elif "error" in params:
            error = params["error"][0]
            self.send_response(400)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            response = f"""
            <html><body style="font-family:Arial;text-align:center;padding:50px;background:#191414;color:#ff4444">
            <h1>✗ Erro no login</h1>
            <p style="color:#fff">{error}</p>
            </body></html>
            """
            self.wfile.write(response.encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        """Silencia logs do servidor HTTP."""
        pass


def get_authorization_url():
    """Monta a URL de autorização do Spotify."""
    params = {
        "client_id": CLIENT_ID,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
        "show_dialog": "true"  # Sempre mostra tela de login
    }
    base_url = "https://accounts.spotify.com/authorize"
    return f"{base_url}?{urllib.parse.urlencode(params)}"


def exchange_code_for_tokens(code):
    """Troca o authorization code por tokens."""
    url = "https://accounts.spotify.com/api/token"

    # Autenticação Basic (client_id:client_secret em Base64)
    credentials = f"{CLIENT_ID}:{CLIENT_SECRET}"
    encoded = base64.b64encode(credentials.encode()).decode()

    headers = {
        "Authorization": f"Basic {encoded}",
        "Content-Type": "application/x-www-form-urlencoded"
    }

    data = {
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": REDIRECT_URI
    }

    response = requests.post(url, headers=headers, data=data)

    if response.status_code == 200:
        return response.json()
    else:
        print(f"\nErro ao trocar code por tokens: {response.status_code}")
        print(response.json())
        return None


def main():
    global auth_code

    print("=" * 60)
    print("  Spotify OAuth - Obter Refresh Token")
    print("=" * 60)
    print()

    # Valida configuração
    if CLIENT_ID == "SEU_CLIENT_ID_AQUI" or CLIENT_SECRET == "SEU_CLIENT_SECRET_AQUI":
        print("ERRO: Configure CLIENT_ID e CLIENT_SECRET!")
        print()
        print("Opções:")
        print("  1. Edite este arquivo e preencha as variáveis")
        print("  2. Use variáveis de ambiente:")
        print("     export SPOTIFY_CLIENT_ID=seu_id")
        print("     export SPOTIFY_CLIENT_SECRET=seu_secret")
        print()
        print("Obtenha em: https://developer.spotify.com/dashboard")
        sys.exit(1)

    print(f"Client ID: {CLIENT_ID[:8]}...{CLIENT_ID[-4:]}")
    print(f"Redirect URI: {REDIRECT_URI}")
    print(f"Scopes: {SCOPES}")
    print()

    # Inicia servidor HTTP local para receber o callback
    server = http.server.HTTPServer(("127.0.0.1", PORT), CallbackHandler)
    server_thread = threading.Thread(target=server.handle_request)
    server_thread.daemon = True
    server_thread.start()

    # Monta e abre URL de autorização no navegador
    auth_url = get_authorization_url()
    print("Abrindo navegador para login no Spotify...")
    print(f"(Se não abrir automaticamente, acesse: {auth_url})")
    print()
    webbrowser.open(auth_url)

    # Aguarda o callback
    print("Aguardando callback do Spotify...")
    server_thread.join(timeout=120)  # Timeout de 2 minutos

    if not auth_code:
        print("\nTimeout! Nenhum callback recebido em 2 minutos.")
        print("Verifique se o Redirect URI está configurado corretamente.")
        sys.exit(1)

    print(f"Authorization code recebido: {auth_code[:20]}...")
    print()

    # Troca o code por tokens
    print("Trocando code por tokens...")
    tokens = exchange_code_for_tokens(auth_code)

    if not tokens:
        print("Falha ao obter tokens!")
        sys.exit(1)

    # Exibe resultados
    refresh_token = tokens.get("refresh_token", "")
    access_token = tokens.get("access_token", "")
    expires_in = tokens.get("expires_in", 0)

    print()
    print("=" * 60)
    print("  SUCESSO! Tokens obtidos:")
    print("=" * 60)
    print()
    print(f"Access Token (expira em {expires_in}s):")
    print(f"  {access_token[:40]}...")
    print()
    print(f"REFRESH TOKEN (copie este valor!):")
    print(f"  {refresh_token}")
    print()
    print("=" * 60)
    print()
    print("Para gravar na ESP32 via Serial, envie:")
    print(f"  SET_CREDS:{CLIENT_ID}:{CLIENT_SECRET}:{refresh_token}")
    print()
    print("Ou copie para o secrets.h:")
    print(f'  #define SPOTIFY_CLIENT_ID     "{CLIENT_ID}"')
    print(f'  #define SPOTIFY_CLIENT_SECRET "{CLIENT_SECRET}"')
    print(f'  #define SPOTIFY_REFRESH_TOKEN "{refresh_token}"')
    print()
    print("=" * 60)


if __name__ == "__main__":
    main()
