#!/usr/bin/env python3
"""Gold Mean Reversion Strategy — deployed on VPS Trade Server"""
import socket, json, statistics, urllib.request, time

def ts_cmd(cmd):
    s = socket.socket()
    s.settimeout(10)
    s.connect(('127.0.0.1', 5559))
    s.sendall((cmd + '\n').encode())
    r = s.recv(65536).decode()
    s.close()
    return json.loads(r) if r.startswith('{') else {'raw': r}

# Get gold price and SMA from backend
url = 'http://127.0.0.1:8155/market/ohlc?symbol=GC%3DF&timeframe=1d&count=30'
resp = urllib.request.urlopen(url, timeout=10)
data = json.loads(resp.read()).get('data', [])

if len(data) >= 20:
    closes = [c['close'] for c in data]
    price = closes[-1]
    sma20 = sum(closes[-20:]) / 20
    deviation = (price - sma20) / sma20 * 100
    
    # Push price to trade server
    ts_cmd(f'PRICE|GC=F|{price}|{price*1.001}')
    
    print(f'Gold: ${price:.2f}')
    print(f'SMA20: ${sma20:.2f}')
    print(f'Deviation: {deviation:+.2f}%')
    
    if deviation < -2.0:
        sl = round(price * 0.985, 2)
        tp = round(price * 1.03, 2)
        result = ts_cmd(f'PLACE|GC=F|BUY|0.1|MARKET|0|{sl}|{tp}|0')
        print(f'\n>>> BUY SIGNAL <<<')
        print(f'Entry: ${price:.2f}, SL: ${sl}, TP: ${tp}')
        print(f'Result: {result}')
    elif deviation > 2.0:
        sl = round(price * 1.015, 2)
        tp = round(price * 0.97, 2)
        result = ts_cmd(f'PLACE|GC=F|SELL|0.1|MARKET|0|{sl}|{tp}|0')
        print(f'\n>>> SELL SIGNAL <<<')
        print(f'Entry: ${price:.2f}, SL: ${sl}, TP: ${tp}')
        print(f'Result: {result}')
    else:
        print(f'\nNo signal — deviation within normal range')
else:
    print(f'Not enough data: {len(data)} bars')

# Show final status
status = ts_cmd('STATUS')
print(f'\nAccount Balance: ${status.get("balance", 0):,.2f}')
print(f'Open Positions: {status.get("positions", 0)}')
print(f'Orders: {status.get("orders", 0)}')

# Show positions
pos = ts_cmd('POSITIONS')
for p in pos.get('positions', []):
    print(f'  {p["symbol"]:8s} {p["side"]:4s} {p["volume"]:.2f} @ ${p["open_price"]:.2f} P&L=${p.get("profit",0):+.2f}')
