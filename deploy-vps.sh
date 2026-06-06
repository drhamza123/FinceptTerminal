#!/bin/bash
set -euo pipefail

# ═══════════════════════════════════════════════════════════════
#  AI Stock Guardian — VPS Deploy Script
#  Usage: bash deploy-vps.sh <your-domain.com>
#  Tested on: Ubuntu 22.04 / 24.04
# ═══════════════════════════════════════════════════════════════

DOMAIN="${1:-}"
if [ -z "$DOMAIN" ]; then
    echo "Usage: bash deploy-vps.sh <your-domain.com>"
    echo "Example: bash deploy-vps.sh guardian.example.com"
    exit 1
fi

# ── Colors ──────────────────────────────────────────────────────
GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
ok()   { echo -e "  ${GREEN}OK${NC}"; }
fail() { echo -e "  ${RED}FAILED: $1${NC}"; exit 1; }

# ── Check root ──────────────────────────────────────────────────
if [ "$(id -u)" -ne 0 ]; then fail "Run as root: sudo bash deploy-vps.sh $DOMAIN"; fi

echo ""
echo "════════════════════════════════════════════════════"
echo "  AI Stock Guardian — Deploy to $DOMAIN"
echo "════════════════════════════════════════════════════"
echo ""

# ── 1. System packages ──────────────────────────────────────────
echo "[1/7] Installing system packages..."
apt-get update -qq
apt-get install -y -qq python3.11 python3.11-venv python3-pip nginx certbot python3-certbot-nginx curl git >/dev/null 2>&1
ok

# ── 2. Create user ──────────────────────────────────────────────
echo "[2/7] Creating guardian user..."
id -u guardian &>/dev/null || useradd -m -s /bin/bash guardian
ok

# ── 3. Deploy backend ───────────────────────────────────────────
echo "[3/7] Deploying backend..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="/opt/guardian"
mkdir -p "$APP_DIR"
cp -r "$SCRIPT_DIR/backend/"* "$APP_DIR/"
chown -R guardian:guardian "$APP_DIR"

# Python venv
su - guardian -c "python3.11 -m venv $APP_DIR/venv" >/dev/null 2>&1
su - guardian -c "$APP_DIR/venv/bin/pip install -q -r $APP_DIR/requirements.txt" >/dev/null 2>&1
ok

# ── 4. .env ─────────────────────────────────────────────────────
echo "[4/7] Configuring environment..."
if [ ! -f "$APP_DIR/.env" ]; then
    cat > "$APP_DIR/.env" << 'ENVEOF'
LLM_PROVIDER_API_KEY=
LLM_PROVIDER_BASE_URL=https://api.openai.com/v1
LLM_DEFAULT_MODEL=gpt-4o-mini
DATABASE_URL=sqlite+aiosqlite:///./data/guardian.db
JWT_SECRET_KEY=
FRED_API_KEY=
ENVEOF
    # Generate random JWT secret
    JWT_SECRET=$(openssl rand -hex 32)
    sed -i "s/JWT_SECRET_KEY=/JWT_SECRET_KEY=$JWT_SECRET/" "$APP_DIR/.env"
    chown guardian:guardian "$APP_DIR/.env"
    chmod 600 "$APP_DIR/.env"
    echo "  Created $APP_DIR/.env — EDIT it to add your API keys:"
    echo "    LLM_PROVIDER_API_KEY  (for AI chat)"
    echo "    FRED_API_KEY          (for macro data)"
else
    echo "  $APP_DIR/.env already exists — leaving it."
fi
ok

# ── 5. Systemd service ──────────────────────────────────────────
echo "[5/7] Creating systemd service..."
cat > /etc/systemd/system/guardian.service << 'UNIT'
[Unit]
Description=AI Stock Guardian API
After=network.target

[Service]
Type=simple
User=guardian
Group=guardian
WorkingDirectory=/opt/guardian
ExecStart=/opt/guardian/venv/bin/python3 /opt/guardian/run.py
Restart=always
RestartSec=5
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable guardian >/dev/null 2>&1
systemctl start guardian
sleep 3
systemctl is-active --quiet guardian || fail "guardian service failed to start. Check: journalctl -u guardian -n 30"
ok

# ── 6. Nginx reverse proxy ──────────────────────────────────────
echo "[6/7] Configuring nginx..."
cat > /etc/nginx/sites-available/guardian << 'NGINX'
server {
    listen 80;
    server_name __DOMAIN__;

    client_max_body_size 10M;

    location / {
        proxy_pass http://127.0.0.1:8150;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_read_timeout 86400;
    }
}
NGINX

sed -i "s/__DOMAIN__/$DOMAIN/g" /etc/nginx/sites-available/guardian
rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/guardian /etc/nginx/sites-enabled/
nginx -t >/dev/null 2>&1 || fail "nginx config test failed"
systemctl reload nginx
ok

# ── 7. SSL via Let's Encrypt ────────────────────────────────────
echo "[7/7] Obtaining SSL certificate..."
certbot --nginx -d "$DOMAIN" --non-interactive --agree-tos --email "admin@$DOMAIN" --redirect 2>&1 || {
    echo "  SSL failed (maybe DNS not propagated?). Serving HTTP only."
    echo "  Run later: certbot --nginx -d $DOMAIN"
}
ok

# ── Done ────────────────────────────────────────────────────────
echo ""
echo "════════════════════════════════════════════════════"
echo "  Deployment Complete!"
echo ""
echo "  URL:      https://$DOMAIN"
echo "  Health:   https://$DOMAIN/health"
echo "  Docs:     https://$DOMAIN/docs"
echo ""
echo "  Config:   $APP_DIR/.env"
echo "  Logs:     journalctl -u guardian -f"
echo "  Restart:  systemctl restart guardian"
echo "════════════════════════════════════════════════════"
echo ""
echo "Next steps:"
echo "  1. Edit $APP_DIR/.env — add your LLM/FRED API keys"
echo "  2. Restart: systemctl restart guardian"
echo "  3. On each user's Mac:"
echo "     defaults write com.guardian.aistockguardian api/base_url \"https://$DOMAIN\""
