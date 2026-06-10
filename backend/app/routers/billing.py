"""Subscription & billing — plans, checkout, credits."""
import sqlite3
import os
import secrets
from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, HTTPException, Header
from fastapi.responses import HTMLResponse

router = APIRouter(tags=["billing"])

PLANS = [
    {
        "plan_id": "free",
        "name": "Free",
        "description": "Basic access — 100 free credits to start",
        "price_usd": 0.0,
        "currency": "USD",
        "credits": 100,
        "support_type": "community",
        "validity_days": 30,
        "features": ["100 credits", "Basic market data", "AI chat (10 queries/day)"],
        "is_free": True,
        "display_order": 1,
    },
    {
        "plan_id": "starter",
        "name": "Starter",
        "description": "For active traders — 500 credits/month",
        "price_usd": 30.0,
        "currency": "USD",
        "credits": 500,
        "support_type": "email",
        "validity_days": 30,
        "features": ["500 credits/month", "Real-time market data", "AI chat (unlimited)", "MT5 trading", "Email support"],
        "is_free": False,
        "display_order": 2,
    },
    {
        "plan_id": "pro",
        "name": "Pro",
        "description": "For professionals — 2000 credits/month",
        "price_usd": 50.0,
        "currency": "USD",
        "credits": 2000,
        "support_type": "priority",
        "validity_days": 30,
        "features": ["2000 credits/month", "Real-time market data", "AI chat (unlimited)",
                     "MT5 trading", "Backtesting", "Options chain", "Multi-chart",
                     "Priority support"],
        "is_free": False,
        "display_order": 3,
    },
]


@router.get("/admin", response_class=HTMLResponse)
def admin_panel():
    return HTMLResponse(ADMIN_HTML)


ADMIN_HTML = """<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Fincept Admin</title>
<style>
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family:system-ui,sans-serif; background:#0a0a0f; color:#e5e5e5; padding:20px; }
h1 { color:#f59e0b; font-size:22px; margin-bottom:20px; }
h2 { color:#e5e5e5; font-size:16px; margin:20px 0 10px; border-bottom:1px solid #1a1a2e; padding-bottom:6px; }
.card { background:#111118; border:1px solid #1a1a2e; border-radius:6px; padding:15px; margin-bottom:15px; }
table { width:100%; border-collapse:collapse; font-size:13px; }
th { background:#1a1a2e; color:#808090; padding:8px 10px; text-align:left; font-weight:600; }
td { padding:6px 10px; border-bottom:1px solid #1a1a2e; }
tr:hover td { background:#1a1a2e; }
.badge { display:inline-block; padding:2px 8px; border-radius:3px; font-size:11px; font-weight:600; }
.free { background:#1a3a1a; color:#4ade80; }
.starter { background:#1a2a3a; color:#60a5fa; }
.pro { background:#3a1a1a; color:#f87171; }
.premium { background:#3a2a1a; color:#fbbf24; }
input, select { background:#0a0a0f; border:1px solid #1a1a2e; color:#e5e5e5; padding:6px 10px; border-radius:4px; font-size:13px; }
input:focus { outline:none; border-color:#f59e0b; }
button { background:#f59e0b; color:#0a0a0f; border:none; padding:6px 14px; border-radius:4px; font-weight:600; cursor:pointer; font-size:13px; }
button:hover { background:#d97706; }
button.danger { background:#dc2626; }
button.danger:hover { background:#b91c1c; }
.btn-sm { padding:3px 10px; font-size:11px; }
.form-row { display:flex; gap:8px; align-items:end; flex-wrap:wrap; margin-top:8px; }
.form-row label { font-size:11px; color:#808090; display:block; margin-bottom:2px; }
.msg { padding:8px 12px; border-radius:4px; margin-bottom:10px; display:none; font-size:13px; }
.msg.ok { background:#1a3a1a; color:#4ade80; display:block; }
.msg.err { background:#3a1a1a; color:#f87171; display:block; }
.stats { display:flex; gap:15px; flex-wrap:wrap; }
.stat-box { background:#111118; border:1px solid #1a1a2e; border-radius:6px; padding:12px 20px; text-align:center; min-width:100px; }
.stat-box .num { font-size:24px; font-weight:700; color:#f59e0b; }
.stat-box .lbl { font-size:11px; color:#808090; margin-top:2px; }
</style></head>
<body>
<h1>⚡ Fincept Admin Panel</h1>
<div id="msg" class="msg"></div>
<div class="stats" id="stats"></div>
<h2>📋 Users</h2>
<div class="card"><table><thead><tr>
<th>ID</th><th>Email</th><th>Username</th><th>Plan</th><th>Credits</th><th>Verified</th><th>Created</th><th>Actions</th>
</tr></thead><tbody id="user-table"></tbody></table></div>
<div style="display:flex; gap:20px; flex-wrap:wrap;">
<div class="card" style="flex:1; min-width:300px;">
<h2>➕ Add User</h2>
<div class="form-row"><label>Email <input id="add-email" placeholder="user@email.com"></label>
<label>Password <input id="add-pass" value="Fincept123!"></label>
<label>Plan <select id="add-plan"><option value="free">Free</option><option value="starter">Starter ($30)</option><option value="pro">Pro ($50)</option></select></label>
<label>Credits <input id="add-credits" value="100" style="width:70px"></label>
<button onclick="addUser()">Add User</button></div></div>
<div class="card" style="flex:1; min-width:300px;">
<h2>💰 Add Credits</h2>
<div class="form-row"><label>Email <input id="cred-email" placeholder="user@email.com"></label>
<label>Credits <input id="cred-amount" value="500" style="width:70px"></label>
<button onclick="addCredits()">Add Credits</button></div></div></div>
<h2>📦 Plans</h2>
<div class="card"><table><thead><tr><th>Plan</th><th>Price</th><th>Credits</th><th>Features</th></tr></thead>
<tbody id="plans-table"></tbody></table></div>
<script>
const API = window.location.origin;
function msg(text, ok=true) {
  const m = document.getElementById('msg');
  m.textContent = text; m.className = 'msg ' + (ok?'ok':'err');
  setTimeout(() => m.style.display='none', 5000);
}
async function loadUsers() {
  const r = await fetch(API + '/admin/users');
  const d = await r.json();
  if (!d.success) return;
  const t = document.getElementById('user-table');
  t.innerHTML = '';
  d.data.forEach(u => {
    t.innerHTML += '<tr><td>'+u.id+'</td><td>'+u.email+'</td><td>'+u.username+'</td>'+
      '<td><span class="badge '+u.plan+'">'+u.plan+'</span></td><td>'+u.credits+'</td><td>'+(u.verified?'✅':'❌')+'</td>'+
      '<td style="font-size:11px;color:#808090">'+(u.created||'')+'</td>'+
      '<td><button class="btn-sm danger" onclick="deleteUser('+u.id+')">✕</button></td></tr>';
  });
  document.getElementById('stats').innerHTML =
    '<div class="stat-box"><div class="num">'+d.data.length+'</div><div class="lbl">Total Users</div></div>'+
    '<div class="stat-box"><div class="num">'+d.data.filter(u=>u.plan==='pro').length+'</div><div class="lbl">Pro Users</div></div>'+
    '<div class="stat-box"><div class="num">'+d.data.reduce((s,u)=>s+u.credits,0)+'</div><div class="lbl">Total Credits</div></div>';
}
async function loadPlans() {
  const r = await fetch(API + '/cashfree/plans');
  const d = await r.json();
  document.getElementById('plans-table').innerHTML = d.data.map(p =>
    '<tr><td><b>'+p.name+'</b></td><td>$'+p.price_usd+'</td><td>'+p.credits+'</td><td style="font-size:11px;color:#808090">'+p.features.slice(0,3).join(', ')+'</td></tr>'
  ).join('');
}
async function addUser() {
  const body = {email:document.getElementById('add-email').value,password:document.getElementById('add-pass').value,plan:document.getElementById('add-plan').value,credits:parseInt(document.getElementById('add-credits').value)};
  if(!body.email) return msg('Email required', false);
  const r = await fetch(API + '/admin/users/add', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  const d = await r.json();
  if(d.success) { msg('Created!'); loadUsers(); } else msg(d.detail?.message||'Failed', false);
}
async function deleteUser(id) {
  if(!confirm('Delete user #'+id+'?')) return;
  const r = await fetch(API + '/admin/users/'+id, {method:'DELETE'});
  const d = await r.json();
  if(d.success) { msg('Deleted'); loadUsers(); } else msg('Failed', false);
}
async function addCredits() {
  const body = {email:document.getElementById('cred-email').value,credits:parseInt(document.getElementById('cred-amount').value),plan_id:'pro'};
  if(!body.email) return msg('Email required', false);
  const r = await fetch(API + '/user/credits/add', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  const d = await r.json();
  if(d.success) { msg('Added!'); loadUsers(); } else msg(d.detail?.message||'Failed', false);
}
loadUsers(); loadPlans();
</script></body></html>"""


def _db():
    db_path = os.path.join(os.path.dirname(__file__), "..", "..", "data", "guardian.db")
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    return sqlite3.connect(db_path)


@router.get("/cashfree/plans")
def get_plans():
    return {"success": True, "message": "OK", "data": PLANS}


@router.post("/user/generate-checkout-token")
def generate_checkout(body: dict):
    plan_id = body.get("plan_id", "")
    plan = next((p for p in PLANS if p["plan_id"] == plan_id), None)
    if not plan:
        raise HTTPException(status_code=400, detail={"success": False, "message": "Invalid plan"})
    # Generate a mock checkout URL — replace with Stripe/Cashfree when live
    token = secrets.token_hex(16)
    checkout_url = f"https://fincept.ai/checkout/{token}?plan={plan_id}&amount={plan['price_usd']}"
    return {"success": True, "message": "Checkout URL generated",
            "data": {"checkout_url": checkout_url, "token": token, "plan": plan}}


@router.post("/user/credits/add")
def add_credits(body: dict = {}):
    """Add credits to a user (admin or after payment webhook)."""
    email = body.get("email", "")
    credits = int(body.get("credits", 0))
    plan_id = body.get("plan_id", "")
    if not email or credits <= 0:
        raise HTTPException(status_code=400, detail={"success": False, "message": "email and credits required"})
    db = _db()
    plan = next((p for p in PLANS if p["plan_id"] == plan_id), None)
    account_type = plan["plan_id"] if plan else "free"
    db.execute("UPDATE users SET credit_balance = credit_balance + ?, account_type = ?, credits_expire_at = ? WHERE email = ?",
               (credits, account_type, (datetime.now(timezone.utc) + timedelta(days=30)).isoformat(), email))
    db.commit()
    row = db.execute("SELECT email, credit_balance, account_type FROM users WHERE email = ?", (email,)).fetchone()
    db.close()
    if not row:
        raise HTTPException(status_code=404, detail={"success": False, "message": "User not found"})
    return {"success": True, "message": f"Added {credits} credits", "data": {"email": row[0], "balance": row[1], "plan": row[2]}}


@router.post("/user/credits/deduct")
def deduct_credits(body: dict = {}):
    """Deduct credits (called when user uses AI, downloads data, etc.)."""
    email = body.get("email", "")
    amount = int(body.get("amount", 1))
    if not email:
        raise HTTPException(status_code=400, detail={"success": False, "message": "email required"})
    db = _db()
    row = db.execute("SELECT credit_balance FROM users WHERE email = ?", (email,)).fetchone()
    if not row:
        db.close()
        raise HTTPException(status_code=404, detail={"success": False, "message": "User not found"})
    balance = row[0]
    if balance < amount:
        db.close()
        raise HTTPException(status_code=402, detail={"success": False, "message": f"Insufficient credits. Need {amount}, have {balance}"})
    db.execute("UPDATE users SET credit_balance = credit_balance - ? WHERE email = ?", (amount, email))
    db.commit()
    new_balance = db.execute("SELECT credit_balance FROM users WHERE email = ?", (email,)).fetchone()[0]
    db.close()
    return {"success": True, "message": f"Deducted {amount} credits", "data": {"email": email, "remaining": new_balance}}


@router.get("/user/credits")
def get_credits(x_api_key: str = Header(default=None)):
    if not x_api_key:
        raise HTTPException(status_code=401, detail={"success": False, "message": "API key required"})
    db = _db()
    row = db.execute("SELECT email, credit_balance, account_type FROM users WHERE api_key = ?", (x_api_key,)).fetchone()
    db.close()
    if not row:
        return {"success": True, "data": {"credits": 100, "plan": "free"}}
    return {"success": True, "data": {"credits": row[1], "plan": row[2]}}


@router.get("/user/subscriptions")
def get_subscriptions(x_api_key: str = Header(default=None), x_session_token: str = Header(default=None)):
    if not x_api_key:
        raise HTTPException(status_code=401, detail={"success": False, "message": "API key required"})
    db = _db()
    if x_session_token:
        row = db.execute("SELECT * FROM users WHERE api_key=? AND session_token=?", (x_api_key, x_session_token)).fetchone()
    else:
        row = db.execute("SELECT * FROM users WHERE api_key=?", (x_api_key,)).fetchone()
    db.close()
    if not row:
        return {"success": True, "message": "OK", "data": {"user_id": 0, "account_type": "free", "credit_balance": 0}}
    return {"success": True, "message": "OK", "data": {
        "user_id": row[0], "account_type": row[10], "credit_balance": row[11],
        "credits_expire_at": row[12], "support_type": "community", "last_credit_purchase_at": None, "created_at": row[14],
    }}


# ── Admin: List / Add / Delete Users ─────────────────────────────

@router.get("/admin/users")
def admin_list_users():
    db = _db()
    rows = db.execute("SELECT id, email, username, account_type, credit_balance, is_verified, created_at FROM users ORDER BY id").fetchall()
    db.close()
    return {"success": True, "data": [{
        "id": r[0], "email": r[1], "username": r[2],
        "plan": r[3], "credits": r[4], "verified": bool(r[5]), "created": r[6]
    } for r in rows]}


@router.post("/admin/users/add")
def admin_add_user(body: dict):
    import hashlib
    email = body.get("email", "")
    username = body.get("username", email.split("@")[0])
    password = body.get("password", "Fincept123!")
    plan = body.get("plan", "free")
    credits = int(body.get("credits", 100))
    if not email:
        raise HTTPException(status_code=400, detail={"success": False, "message": "email required"})
    db = _db()
    existing = db.execute("SELECT id FROM users WHERE email=?", (email,)).fetchone()
    if existing:
        db.close()
        raise HTTPException(status_code=409, detail={"success": False, "message": "User already exists"})
    from app.utils.security import hash_password, generate_api_key
    import uuid
    api_key = generate_api_key()
    user_uuid = str(uuid.uuid4())
    salt = secrets.token_hex(16)
    h = hashlib.sha256((salt + password).encode()).hexdigest()
    pw_hash = f"sha256${salt}${h}"
    db.execute(
        "INSERT INTO users (uuid, username, email, password_hash, api_key, phone, country_code, account_type, credit_balance, is_verified, is_admin, mfa_enabled, created_at) VALUES (?,?,?,?,?,?,?,?,?,1,0,0,datetime('now'))",
        (user_uuid, username, email, pw_hash, api_key, "", "US", plan, credits))
    db.commit()
    db.close()
    return {"success": True, "message": "User created", "data": {"email": email, "api_key": api_key, "password": password, "plan": plan, "credits": credits}}


@router.delete("/admin/users/{user_id}")
def admin_delete_user(user_id: int):
    db = _db()
    db.execute("DELETE FROM users WHERE id=?", (user_id,))
    db.commit()
    db.close()
    return {"success": True, "message": f"User {user_id} deleted"}


@router.post("/admin/users/{user_id}/credits")
def admin_set_credits(user_id: int, body: dict):
    credits = int(body.get("credits", 0))
    db = _db()
    db.execute("UPDATE users SET credit_balance=? WHERE id=?", (credits, user_id))
    db.commit()
    db.close()
    return {"success": True, "message": f"Credits set to {credits}"}
