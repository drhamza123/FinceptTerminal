//+------------------------------------------------------------------+
//| GoldBacktestEA.mq5 - EMA Crossover Backtest for XAUUSD           |
//| Purpose: Mirror the Python vectorized backtester logic so results |
//|          can be compared side-by-side with MT5's Strategy Tester. |
//|                                                                   |
//| Strategy: EMA(9) / EMA(21) crossover, ATR-based SL/TP             |
//| Risk: Fixed % per trade, SL = 1.5×ATR, TP = 3.0×ATR              |
//| Friction: Spread deducted via MT5's built-in modeling             |
//+------------------------------------------------------------------+
#property copyright "FinceptTerminal"
#property version   "1.00"
#property description "Gold EMA Crossover Backtest — compare with Python backtest.py"
#property description "Fast EMA / Slow EMA crossover on XAUUSD H1"
#property description "ATR-based stop loss and take profit"
#property strict

#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>
#include <Trade\AccountInfo.mqh>
#include <Trade\SymbolInfo.mqh>

//+------------------------------------------------------------------+
//| Input parameters                                                 |
//+------------------------------------------------------------------+
input group "=== Strategy ==="
input int      InpFastMA         = 9;              // Fast EMA period
input int      InpSlowMA         = 21;             // Slow EMA period
input double   InpRiskPercent    = 2.0;            // Risk % per trade
input double   InpATRMultiplierSL = 1.5;           // SL = ATR × multiplier
input double   InpATRMultiplierTP = 3.0;           // TP = ATR × multiplier
input int      InpATRPeriod      = 14;             // ATR period

input group "=== Trade Settings ==="
input double   InpMinLot         = 0.01;           // Minimum lot
input double   InpMaxLot         = 10.0;           // Maximum lot
input int      InpMagicNumber    = 20240601;       // EA magic number
input int      InpSlippage       = 30;             // Slippage (points)

input group "=== Display ==="
input bool     InpVerbose        = true;           // Print trade details

//+------------------------------------------------------------------+
//| Global variables                                                 |
//+------------------------------------------------------------------+
CTrade         m_trade;
CPositionInfo  m_position;
CAccountInfo   m_account;
CSymbolInfo    m_symbol;

int            h_ema_fast, h_ema_slow, h_atr;
double         ema_fast_buf[], ema_slow_buf[], atr_buf[];
datetime       last_bar_time = 0;
ulong          total_trades = 0;
double         gross_pnl = 0.0;
double         total_commission = 0.0;
double         initial_balance = 0.0;
int            drawdown_bar = 0;

// CSV log file handle (only for Strategy Tester)
int            csv_handle = INVALID_HANDLE;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
   initial_balance = AccountInfoDouble(ACCOUNT_BALANCE);

   // Symbol setup
   m_symbol.Name(Symbol());
   m_symbol.Refresh();

   m_trade.SetExpertMagicNumber(InpMagicNumber);
   m_trade.SetDeviationInPoints(InpSlippage);

   if (!m_trade.SetTypeFillingBySymbol(Symbol()))
      m_trade.SetTypeFilling(ORDER_FILLING_FOK);

   // Create indicators
   h_ema_fast = iMA(Symbol(), Period(), InpFastMA, 0, MODE_EMA, PRICE_CLOSE);
   h_ema_slow = iMA(Symbol(), Period(), InpSlowMA, 0, MODE_EMA, PRICE_CLOSE);
   h_atr      = iATR(Symbol(), Period(), InpATRPeriod);

   if (h_ema_fast == INVALID_HANDLE || h_ema_slow == INVALID_HANDLE || h_atr == INVALID_HANDLE)
   {
      Print("ERROR: Failed to create indicators");
      return INIT_FAILED;
   }

   ArraySetAsSeries(ema_fast_buf, true);
   ArraySetAsSeries(ema_slow_buf, true);
   ArraySetAsSeries(atr_buf, true);

   if (InpVerbose)
   {
      Print("=== GoldBacktestEA initialized ===");
      Print("Symbol: ", Symbol(), " | TF: ", EnumToString(Period()));
      Print("Strategy: EMA(", InpFastMA, "/", InpSlowMA, ") crossover");
      Print("Risk: ", InpRiskPercent, "% | SL: ", InpATRMultiplierSL, "×ATR | TP: ", InpATRMultiplierTP, "×ATR");
      Print("Magic: ", InpMagicNumber);
      Print("Initial Balance: ", DoubleToString(initial_balance, 2));
   }

   // Open CSV log in tester
   if (MQLInfoInteger(MQL_TESTER))
   {
      csv_handle = FileOpen("GoldBacktestEA_" + Symbol() + "_" +
                            EnumToString(Period()) + ".csv",
                            FILE_WRITE | FILE_CSV | FILE_ANSI, ",");
      if (csv_handle != INVALID_HANDLE)
      {
         FileWrite(csv_handle, "Time", "Side", "Entry", "Exit",
                   "Lot", "SL", "TP", "P&L", "Commission", "Swap",
                   "Reason", "Balance");
      }
   }

   last_bar_time = 0;
   total_trades = 0;
   gross_pnl = 0.0;
   total_commission = 0.0;

   return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   if (csv_handle != INVALID_HANDLE)
      FileClose(csv_handle);

   if (InpVerbose)
   {
      Print("=== GoldBacktestEA summary ===");
      Print("Total trades: ", total_trades);
      Print("Gross P&L: ", DoubleToString(gross_pnl, 2));
      Print("Total Commission: ", DoubleToString(total_commission, 2));
      double final_balance = AccountInfoDouble(ACCOUNT_BALANCE);
      Print("Final Balance: ", DoubleToString(final_balance, 2),
            " (", DoubleToString((final_balance - initial_balance) / initial_balance * 100, 2), "%)");
   }
}

//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick()
{
   // Only trade on new bar
   datetime current_bar_time = iTime(Symbol(), Period(), 0);
   if (current_bar_time == last_bar_time)
      return;
   last_bar_time = current_bar_time;

   // Copy indicator values
   if (CopyBuffer(h_ema_fast, 0, 0, InpSlowMA + 5, ema_fast_buf) < InpSlowMA + 5) return;
   if (CopyBuffer(h_ema_slow, 0, 0, InpSlowMA + 5, ema_slow_buf) < InpSlowMA + 5) return;
   if (CopyBuffer(h_atr, 0, 0, 2, atr_buf) < 2) return;

   double ema_fast_0  = ema_fast_buf[0];
   double ema_slow_0  = ema_slow_buf[0];
   double ema_fast_1  = ema_fast_buf[1];
   double ema_slow_1  = ema_slow_buf[1];
   double atr_val     = atr_buf[0];

   if (ema_fast_0 == EMPTY_VALUE || ema_slow_0 == EMPTY_VALUE || atr_val == EMPTY_VALUE)
      return;

   double ask = SymbolInfoDouble(Symbol(), SYMBOL_ASK);
   double bid = SymbolInfoDouble(Symbol(), SYMBOL_BID);
   double point = SymbolInfoDouble(Symbol(), SYMBOL_POINT);
   double tick_size = SymbolInfoDouble(Symbol(), SYMBOL_TRADE_TICK_SIZE);
   double tick_value = SymbolInfoDouble(Symbol(), SYMBOL_TRADE_TICK_VALUE);
   double lot_step = SymbolInfoDouble(Symbol(), SYMBOL_VOLUME_STEP);

   // Position management
   bool has_buy = false, has_sell = false;
   for (int i = PositionsTotal() - 1; i >= 0; i--)
   {
      if (m_position.SelectByIndex(i) && m_position.Magic() == InpMagicNumber)
      {
         if (m_position.PositionType() == POSITION_TYPE_BUY) has_buy = true;
         if (m_position.PositionType() == POSITION_TYPE_SELL) has_sell = true;
      }
   }

   // --- Signal Logic (same as Python backtester) ---
   // Cross above: fast_1 <= slow_1 AND fast_0 > slow_0 → BUY
   // Cross below: fast_1 >= slow_1 AND fast_0 < slow_0 → SELL
   bool buy_signal  = (ema_fast_1 <= ema_slow_1 + point) && (ema_fast_0 > ema_slow_0);
   bool sell_signal = (ema_fast_1 >= ema_slow_1 - point) && (ema_fast_0 < ema_slow_0);

   // --- Close opposite positions on signal ---
   double curr_balance = AccountInfoDouble(ACCOUNT_BALANCE);

   if (buy_signal && has_sell)
   {
      CloseAllPositions("Reverse Buy");
      has_sell = false;
   }

   if (sell_signal && has_buy)
   {
      CloseAllPositions("Reverse Sell");
      has_buy = false;
   }

   // --- Open new position (max 1 per side, 2 total) ---
   if (buy_signal && !has_buy)
   {
      double sl_dist = atr_val * InpATRMultiplierSL;
      double tp_dist = atr_val * InpATRMultiplierTP;
      double sl = NormalizeDouble(ask - sl_dist, (int)SymbolInfoInteger(Symbol(), SYMBOL_DIGITS));
      double tp = NormalizeDouble(ask + tp_dist, (int)SymbolInfoInteger(Symbol(), SYMBOL_DIGITS));
      double lot = CalcLotSize(curr_balance, sl_dist);

      if (lot >= InpMinLot)
      {
         if (m_trade.Buy(lot, Symbol(), ask, sl, tp, "GoldBacktestEA"))
         {
            total_trades++;
            if (InpVerbose)
               Print("BUY ", lot, " @ ", ask, " SL:", sl, " TP:", tp);
         }
      }
   }

   if (sell_signal && !has_sell)
   {
      double sl_dist = atr_val * InpATRMultiplierSL;
      double tp_dist = atr_val * InpATRMultiplierTP;
      double sl = NormalizeDouble(bid + sl_dist, (int)SymbolInfoInteger(Symbol(), SYMBOL_DIGITS));
      double tp = NormalizeDouble(bid - tp_dist, (int)SymbolInfoInteger(Symbol(), SYMBOL_DIGITS));
      double lot = CalcLotSize(curr_balance, sl_dist);

      if (lot >= InpMinLot)
      {
         if (m_trade.Sell(lot, Symbol(), bid, sl, tp, "GoldBacktestEA"))
         {
            total_trades++;
            if (InpVerbose)
               Print("SELL ", lot, " @ ", bid, " SL:", sl, " TP:", tp);
         }
      }
   }
}

//+------------------------------------------------------------------+
//| Trade transaction handler — log results                          |
//+------------------------------------------------------------------+
void OnTradeTransaction(const MqlTradeTransaction &trans,
                        const MqlTradeRequest &request,
                        const MqlTradeResult &result)
{
   if (trans.type != TRADE_TRANSACTION_DEAL_ADD)
      return;

   ulong deal_ticket = trans.deal;
   if (deal_ticket == 0) return;

   // Get deal details
   long deal_magic;
   if (!HistoryDealGetInteger(deal_ticket, DEAL_MAGIC, deal_magic))
      return;
   if (deal_magic != InpMagicNumber)
      return;

   double deal_profit = HistoryDealGetDouble(deal_ticket, DEAL_PROFIT);
   double deal_commission = HistoryDealGetDouble(deal_ticket, DEAL_COMMISSION);
   double deal_swap = HistoryDealGetDouble(deal_ticket, DEAL_SWAP);
   long deal_entry = HistoryDealGetInteger(deal_ticket, DEAL_ENTRY);
   long deal_type = HistoryDealGetInteger(deal_ticket, DEAL_TYPE);
   double deal_price = HistoryDealGetDouble(deal_ticket, DEAL_PRICE);
   double deal_volume = HistoryDealGetDouble(deal_ticket, DEAL_VOLUME);
   datetime deal_time = (datetime)HistoryDealGetInteger(deal_ticket, DEAL_TIME);

   string side_str = (deal_type == DEAL_TYPE_BUY) ? "BUY" : "SELL";
   string entry_str = (deal_entry == DEAL_ENTRY_IN) ? "In" :
                      (deal_entry == DEAL_ENTRY_OUT) ? "Out" : "";

   gross_pnl += deal_profit;
   total_commission += deal_commission;

   // Log to CSV
   if (csv_handle != INVALID_HANDLE && deal_entry == DEAL_ENTRY_OUT)
   {
      double entry_price = 0;
      // Find corresponding entry deal to get entry price
      HistorySelect(deal_time - 3600, deal_time + 3600);
      int total_deals = HistoryDealsTotal();
      for (int i = 0; i < total_deals; i++)
      {
         ulong ticket = HistoryDealGetTicket(i);
         if (HistoryDealGetString(ticket, DEAL_SYMBOL) == Symbol() &&
             HistoryDealGetInteger(ticket, DEAL_MAGIC) == InpMagicNumber &&
             HistoryDealGetInteger(ticket, DEAL_ENTRY) == DEAL_ENTRY_IN &&
             HistoryDealGetInteger(ticket, DEAL_TYPE) == deal_type)
         {
            entry_price = HistoryDealGetDouble(ticket, DEAL_PRICE);
            break;
         }
      }

      double balance = AccountInfoDouble(ACCOUNT_BALANCE);
      FileWrite(csv_handle,
                TimeToString(deal_time),
                side_str,
                DoubleToString(entry_price, (int)SymbolInfoInteger(Symbol(), SYMBOL_DIGITS)),
                DoubleToString(deal_price, (int)SymbolInfoInteger(Symbol(), SYMBOL_DIGITS)),
                DoubleToString(deal_volume, 2),
                "0", "0",
                DoubleToString(deal_profit, 2),
                DoubleToString(deal_commission, 2),
                DoubleToString(deal_swap, 2),
                "Close",
                DoubleToString(balance, 2));
   }
}

//+------------------------------------------------------------------+
//| Position close helper                                            |
//+------------------------------------------------------------------+
void CloseAllPositions(string reason)
{
   for (int i = PositionsTotal() - 1; i >= 0; i--)
   {
      if (m_position.SelectByIndex(i) && m_position.Magic() == InpMagicNumber)
      {
         if (m_trade.PositionClose(m_position.Ticket()))
         {
            if (InpVerbose)
               Print("Closed ", m_position.Symbol(), " (", reason, ")");
         }
      }
   }
   Sleep(100);  // Let MT5 process
}

//+------------------------------------------------------------------+
//| Lot size calculator (matches Python logic)                        |
//+------------------------------------------------------------------+
double CalcLotSize(double balance, double sl_dist_points)
{
   double risk_amount = balance * InpRiskPercent / 100.0;
   double tick_value = SymbolInfoDouble(Symbol(), SYMBOL_TRADE_TICK_VALUE);
   double tick_size  = SymbolInfoDouble(Symbol(), SYMBOL_TRADE_TICK_SIZE);
   double point      = SymbolInfoDouble(Symbol(), SYMBOL_POINT);

   if (tick_value <= 0 || tick_size <= 0 || sl_dist_points <= 0)
      return InpMinLot;

   // Risk per lot at sl_dist points:
   //   P&L = sl_dist_points * tick_value / tick_size * lot
   //   We want: risk_amount = sl_dist_points * tick_value / tick_size * lot
   //   => lot = risk_amount * tick_size / (sl_dist_points * tick_value)
   double lot = risk_amount * tick_size / (sl_dist_points * tick_value);
   lot = MathMax(InpMinLot, MathMin(InpMaxLot, lot));

   // Round to lot step
   double lot_step = SymbolInfoDouble(Symbol(), SYMBOL_VOLUME_STEP);
   if (lot_step > 0)
      lot = MathRound(lot / lot_step) * lot_step;

   return lot;
}

//+------------------------------------------------------------------+
//| Tester-only functions (not used live)                            |
//+------------------------------------------------------------------+
double OnTester()
{
   // Return Sharpe-like metric for MT5 Optimizer
   double profit = TesterStatistics(STAT_PROFIT);
   double dd     = TesterStatistics(STAT_EQUITY_DD);
   if (dd == 0) return profit;
   return profit / dd;
}
//+------------------------------------------------------------------+
