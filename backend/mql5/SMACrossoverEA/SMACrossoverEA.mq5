//+------------------------------------------------------------------+
//|                                             SMACrossoverEA.mq5    |
//|  Simple SMA Crossover — live paper trade on FinceptTerminal chart |
//+------------------------------------------------------------------+
#property strict
#property copyright "FinceptTerminal"
#property version   "1.00"
#property description "SMA(50) / SMA(200) crossover on daily chart"
#property description "Paper trade: run on FinceptTerminal chart"

input int    InpFastMA    = 50;
input int    InpSlowMA    = 200;
input double InpLotSize   = 0.1;

int h_fast, h_slow;
double fast[], slow[];

int OnInit() {
    h_fast = iMA(Symbol(), PERIOD_D1, InpFastMA, 0, MODE_SMA, PRICE_CLOSE);
    h_slow = iMA(Symbol(), PERIOD_D1, InpSlowMA, 0, MODE_SMA, PRICE_CLOSE);
    if (h_fast == INVALID_HANDLE || h_slow == INVALID_HANDLE)
        return INIT_FAILED;
    ArraySetAsSeries(fast, true);
    ArraySetAsSeries(slow, true);
    return INIT_SUCCEEDED;
}

void OnTick() {
    CopyBuffer(h_fast, 0, 0, 3, fast);
    CopyBuffer(h_slow, 0, 0, 3, slow);
    if (fast[1] <= slow[1] && fast[0] > slow[0])
        Print("BUY signal on ", Symbol());
    if (fast[1] >= slow[1] && fast[0] < slow[0])
        Print("SELL signal on ", Symbol());
}
//+------------------------------------------------------------------+
