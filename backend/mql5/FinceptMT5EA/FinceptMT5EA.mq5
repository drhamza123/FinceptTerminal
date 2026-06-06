//+------------------------------------------------------------------+
//|                                             FinceptMT5EA.mq5      |
//|                     Ultra-Low Latency ZMQ Execution Bridge        |
//|  Listens on tcp://127.0.0.1:5555 for pipe-delimited orders        |
//|  Format: ACTION|SYMBOL|VOLUME|PRICE|SL|TP|COMMENT                 |
//+------------------------------------------------------------------+
#property strict
#property copyright "FinceptTerminal"
#property version   "1.00"
#property description "ZMQ bridge — receives orders from Python backend"
#property description "Expects FinceptTerminal backend on port 5555"

// Download from: https://github.com/dmitrievmv/MQL5-ZeroMQ
#include <ZeroMQ/ZeroMQ.mqh>

input string InpZmqEndpoint = "tcp://127.0.0.1:5555";
input int    InpMagicNumber = 888999;
input int    InpDeviation   = 10;

CZmqContext   gContext;
CZmqSocket    gResponder;
int           gTimerMs = 10;

int OnInit() {
    gContext.Initialize();
    gResponder.Initialize(gContext, ZMQ_REP);

    if (!gResponder.Bind(InpZmqEndpoint)) {
        Print("[FinceptEA] Failed to bind to ", InpZmqEndpoint);
        return INIT_FAILED;
    }

    EventSetMillisecondTimer(gTimerMs);
    Print("[FinceptEA] Listening on ", InpZmqEndpoint);
    return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {
    EventKillTimer();
    gResponder.Close();
    gContext.Terminate();
}

void OnTimer() {
    CZmqMsg request;
    if (!gResponder.Receive(request, 5)) return;

    string reqStr = request.GetString();
    string parts[];
    int count = StringSplit(reqStr, '|', parts);

    if (count < 7) {
        gResponder.Send(CZmqMsg("ERR|-1|INVALID_FORMAT"));
        return;
    }

    string action  = parts[0];
    string symbol  = parts[1];
    double volume  = StringToDouble(parts[2]);
    double price   = StringToDouble(parts[3]);
    double sl      = StringToDouble(parts[4]);
    double tp      = StringToDouble(parts[5]);
    string comment = parts[6];

    MqlTradeRequest req = {};
    MqlTradeResult  res = {};

    req.action   = TRADE_ACTION_DEAL;
    req.symbol   = symbol;
    req.volume   = (float)volume;
    req.type     = (action == "BUY") ? ORDER_TYPE_BUY : ORDER_TYPE_SELL;
    req.price    = (price > 0) ? price :
                   (action == "BUY") ? SymbolInfoDouble(symbol, SYMBOL_ASK)
                                     : SymbolInfoDouble(symbol, SYMBOL_BID);
    req.sl       = sl;
    req.tp       = tp;
    req.deviation = InpDeviation;
    req.magic    = InpMagicNumber;
    req.comment  = comment;

    bool sent = OrderSend(req, res);
    string response;

    if (sent && res.retcode == TRADE_RETCODE_DONE)
        response = "OK|" + IntegerToString(res.order) + "|0";
    else
        response = "ERR|" + IntegerToString(res.retcode) + "|" + res.comment;

    gResponder.Send(CZmqMsg(response));
}
//+------------------------------------------------------------------+
