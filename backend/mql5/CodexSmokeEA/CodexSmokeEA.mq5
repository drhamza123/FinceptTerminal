//+------------------------------------------------------------------+
//|                                               CodexSmokeEA.mq5   |
//|  Safe smoke-test EA for Fincept Execution + MetaTrader deploy.   |
//+------------------------------------------------------------------+
#property strict
#property copyright "FinceptTerminal"
#property version   "1.00"
#property description "No-trade smoke test: prints status when attached to MT5."

input string InpTestName = "Fincept Execution Smoke Test";

datetime g_last_print = 0;

int OnInit() {
   Print(InpTestName, " initialized on ", _Symbol, " / ", EnumToString(Period()));
   return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {
   Print(InpTestName, " stopped. reason=", reason);
}

void OnTick() {
   datetime now = TimeCurrent();
   if (now - g_last_print >= 30) {
      g_last_print = now;
      Print(InpTestName, " heartbeat: bid=", SymbolInfoDouble(_Symbol, SYMBOL_BID),
            " ask=", SymbolInfoDouble(_Symbol, SYMBOL_ASK));
   }
}
//+------------------------------------------------------------------+
