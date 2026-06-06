//+------------------------------------------------------------------+
//|                                          GoldTraderEA.mq5        |
//|  Gold (XAUUSD) strategy — EMA crossover + ATR stops + trend filter
//+------------------------------------------------------------------+
#property strict
#property copyright "FinceptTerminal"
#property version   "1.00"
#property description "Gold Trader: EMA(20/50) + ATR(14) + 200 SMA trend filter"

input int    InpFastMA     = 20;
input int    InpSlowMA     = 50;
input int    InpTrendMA    = 200;
input double InpRiskPct    = 2.0;
input double InpATRSL      = 2.0;
input double InpATRTP      = 4.0;

int h_fast, h_slow, h_trend, h_atr;
double bf[], bs[], bt[], atr[];

int OnInit() {
    h_fast  = iMA(_Symbol, PERIOD_CURRENT, InpFastMA, 0, MODE_EMA, PRICE_CLOSE);
    h_slow  = iMA(_Symbol, PERIOD_CURRENT, InpSlowMA, 0, MODE_EMA, PRICE_CLOSE);
    h_trend = iMA(_Symbol, PERIOD_CURRENT, InpTrendMA, 0, MODE_SMA, PRICE_CLOSE);
    h_atr   = iATR(_Symbol, PERIOD_CURRENT, 14);
    if (h_fast == INVALID_HANDLE || h_slow == INVALID_HANDLE || h_trend == INVALID_HANDLE || h_atr == INVALID_HANDLE)
        return INIT_FAILED;
    ArraySetAsSeries(bf,true); ArraySetAsSeries(bs,true);
    ArraySetAsSeries(bt,true); ArraySetAsSeries(atr,true);
    return INIT_SUCCEEDED;
}

void OnTick() {
    CopyBuffer(h_fast,0,0,5,bf); CopyBuffer(h_slow,0,0,5,bs);
    CopyBuffer(h_trend,0,0,5,bt); CopyBuffer(h_atr,0,0,2,atr);
    if (bf[1] <= bs[1] && bf[0] > bs[0] && bf[0] > bt[0]) {
        double sl = SymbolInfoDouble(_Symbol, SYMBOL_BID) - atr[0] * InpATRSL;
        double tp = SymbolInfoDouble(_Symbol, SYMBOL_ASK) + atr[0] * InpATRTP;
        Print("GOLD BUY signal @ ", SymbolInfoDouble(_Symbol, SYMBOL_BID));
    }
    if (bf[1] >= bs[1] && bf[0] < bs[0] && bf[0] < bt[0]) {
        Print("GOLD SELL signal @ ", SymbolInfoDouble(_Symbol, SYMBOL_BID));
    }
}
//+------------------------------------------------------------------+
