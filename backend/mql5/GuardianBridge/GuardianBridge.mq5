//+------------------------------------------------------------------+
//| GuardianBridge.mq5 — AI Stock Guardian MT5 Bridge EA            |
//| Connects to Python backend via TCP, executes remote commands.    |
//| Strategy-agnostic: runtime params modified via set_params.       |
//+------------------------------------------------------------------+
#property copyright "AI Stock Guardian"
#property version   "1.00"

#include <Trade\Trade.mqh>
CTrade trade;

// ── Input parameters (read-only at runtime) ──
input double   InpLotSize        = 0.1;        // Default Lot Size
input int      InpRiskPercent    = 2;           // Default Risk Per Trade (%)
input int      InpMagicNumber    = 2024001;     // Magic Number (EA identifier)
input double   InpStopLossPips   = 50.0;        // Default Stop Loss (pips)
input double   InpTakeProfitPips = 100.0;       // Default Take Profit (pips)
input string   InpServerHost     = "127.0.0.1"; // Bridge Host
input int      InpServerPort     = 5556;        // Bridge Port
input int      InpHeartbeatSec   = 1;           // Heartbeat Interval (sec)

// ── Runtime variables (modified by remote set_params) ──
double   gLotSize;
int      gRiskPercent;
double   gStopLossPips;
double   gTakeProfitPips;

int      gSocket = INVALID_HANDLE;
string   gMessageBuffer = "";
int      gReconnectAttempts = 0;
const int MAX_RECONNECT = 10;


//+------------------------------------------------------------------+
int OnInit() {
   gLotSize = InpLotSize;
   gRiskPercent = InpRiskPercent;
   gStopLossPips = InpStopLossPips;
   gTakeProfitPips = InpTakeProfitPips;
   Print("GuardianBridge starting, magic=", InpMagicNumber, " on ", _Symbol);
   if (!ConnectToServer()) Print("Initial connection failed — will retry");
   EventSetTimer(InpHeartbeatSec);
   return(INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
void OnDeinit(const int reason) {
   EventKillTimer();
   if (gSocket != INVALID_HANDLE) {
      SendJSON("{\"type\":\"goodbye\"}\n");
      SocketClose(gSocket);
      gSocket = INVALID_HANDLE;
   }
   Print("GuardianBridge shutting down, reason=", reason);
}

//+------------------------------------------------------------------+
void OnTick() {
   // STRICTLY NON-BLOCKING socket read
   if (gSocket != INVALID_HANDLE && SocketIsConnected(gSocket)) {
      if (SocketIsReadable(gSocket)) {
         uchar buffer[];
         ArrayResize(buffer, 4096);
         int received = SocketRead(gSocket, buffer, 4096, 0);
         if (received > 0) {
            gMessageBuffer += CharArrayToString(buffer, 0, received, CP_UTF8);
            int pos;
            while ((pos = StringFind(gMessageBuffer, "\n")) >= 0) {
               string jsonMsg = StringSubstr(gMessageBuffer, 0, pos);
               gMessageBuffer = StringSubstr(gMessageBuffer, pos + 1);
               ProcessCommand(jsonMsg);
            }
         }
      }
   }
}

//+------------------------------------------------------------------+
void OnTimer() {
   if (gSocket != INVALID_HANDLE && SocketIsConnected(gSocket)) {
      string heartbeat = StringFormat(
         "{\"type\":\"heartbeat\",\"ea_name\":\"%s\",\"magic\":%d,"
         "\"symbol\":\"%s\",\"tf\":\"%s\",\"status\":\"running\","
         "\"balance\":%.2f,\"equity\":%.2f}\n",
         MQLInfoString(MQL_PROGRAM_NAME), InpMagicNumber,
         _Symbol, EnumToString(Period()),
         AccountInfoDouble(ACCOUNT_BALANCE), AccountInfoDouble(ACCOUNT_EQUITY));
      SendJSON(heartbeat);
   } else if (gReconnectAttempts < MAX_RECONNECT) {
      gReconnectAttempts++;
      Print("Reconnect attempt ", gReconnectAttempts, "/", MAX_RECONNECT);
      if (ConnectToServer()) gReconnectAttempts = 0;
   }
}

//+------------------------------------------------------------------+
void OnTradeTransaction(const MqlTradeTransaction &trans,
                        const MqlTradeRequest &request,
                        const MqlTradeResult &result) {
   if (trans.type == TRADE_TRANSACTION_DEAL_ADD) {
      if (HistoryDealSelect(trans.deal)) {
         if (HistoryDealGetInteger(trans.deal, DEAL_MAGIC) == InpMagicNumber) {
            string action = (HistoryDealGetInteger(trans.deal, DEAL_TYPE) == DEAL_TYPE_BUY)
               ? "buy" : "sell";
            string tradeMsg = StringFormat(
               "{\"type\":\"trade\",\"ea_name\":\"%s\",\"magic\":%d,"
               "\"symbol\":\"%s\",\"action\":\"%s\",\"lots\":%.2f,"
               "\"price\":%.5f,\"profit\":%.2f}\n",
               MQLInfoString(MQL_PROGRAM_NAME), InpMagicNumber,
               HistoryDealGetString(trans.deal, DEAL_SYMBOL), action,
               HistoryDealGetDouble(trans.deal, DEAL_VOLUME),
               HistoryDealGetDouble(trans.deal, DEAL_PRICE),
               HistoryDealGetDouble(trans.deal, DEAL_PROFIT));
            SendJSON(tradeMsg);
         }
      }
   }
}

//+------------------------------------------------------------------+
bool ConnectToServer() {
   if (gSocket != INVALID_HANDLE) {
      SocketClose(gSocket);
      gSocket = INVALID_HANDLE;
   }
   gSocket = SocketCreate(SOCKET_AF_INET, SOCKET_STREAM, SOCKET_IPPROTO_TCP);
   if (gSocket == INVALID_HANDLE) {
      Print("SocketCreate failed, error=", GetLastError());
      return false;
   }
   if (!SocketConnect(gSocket, InpServerHost, InpServerPort, 1000)) {
      Print("SocketConnect failed to ", InpServerHost, ":", InpServerPort,
            " error=", GetLastError());
      SocketClose(gSocket);
      gSocket = INVALID_HANDLE;
      return false;
   }
   Print("Connected to bridge at ", InpServerHost, ":", InpServerPort);
   gReconnectAttempts = 0;
   string hello = StringFormat(
      "{\"type\":\"hello\",\"ea_name\":\"%s\",\"magic\":%d,"
      "\"symbol\":\"%s\",\"tf\":\"%s\",\"balance\":%.2f,\"equity\":%.2f}\n",
      MQLInfoString(MQL_PROGRAM_NAME), InpMagicNumber,
      _Symbol, EnumToString(Period()),
      AccountInfoDouble(ACCOUNT_BALANCE), AccountInfoDouble(ACCOUNT_EQUITY));
   SendJSON(hello);
   return true;
}

//+------------------------------------------------------------------+
void SendJSON(string jsonStr) {
   if (gSocket == INVALID_HANDLE || !SocketIsConnected(gSocket)) return;
   uchar data[];
   int size = StringToCharArray(jsonStr, data, 0, WHOLE_ARRAY, CP_UTF8);
   if (size > 0) SocketSend(gSocket, data, size);
}

//+------------------------------------------------------------------+
void ProcessCommand(string jsonMsg) {
   if (StringFind(jsonMsg, "\"ping\"") >= 0) {
      string pong = StringFormat(
         "{\"type\":\"heartbeat\",\"ea_name\":\"%s\",\"magic\":%d,"
         "\"symbol\":\"%s\",\"tf\":\"%s\",\"status\":\"running\","
         "\"balance\":%.2f,\"equity\":%.2f}\n",
         MQLInfoString(MQL_PROGRAM_NAME), InpMagicNumber,
         _Symbol, EnumToString(Period()),
         AccountInfoDouble(ACCOUNT_BALANCE), AccountInfoDouble(ACCOUNT_EQUITY));
      SendJSON(pong);
   }
   else if (StringFind(jsonMsg, "\"set_params\"") >= 0) {
      double newLot = ExtractParamDouble(jsonMsg, "lot_size");
      if (newLot > 0) gLotSize = newLot;
      int newRisk = ExtractParamInt(jsonMsg, "risk_percent");
      if (newRisk > 0) gRiskPercent = newRisk;
      double newSL = ExtractParamDouble(jsonMsg, "stop_loss_pips");
      if (newSL > 0) gStopLossPips = newSL;
      double newTP = ExtractParamDouble(jsonMsg, "take_profit_pips");
      if (newTP > 0) gTakeProfitPips = newTP;
      Print("Params updated: lot=", gLotSize, " risk=", gRiskPercent,
            " sl=", gStopLossPips, " tp=", gTakeProfitPips);
   }
   else if (StringFind(jsonMsg, "\"close_all\"") >= 0) {
      CloseAllPositions();
   }
   else if (StringFind(jsonMsg, "\"shutdown\"") >= 0) {
      CloseAllPositions();
      ExpertRemove();
   }
}

//+------------------------------------------------------------------+
void CloseAllPositions() {
   for (int i = PositionsTotal() - 1; i >= 0; i--) {
      ulong ticket = PositionGetTicket(i);
      if (PositionSelectByTicket(ticket)) {
         if (PositionGetInteger(POSITION_MAGIC) == InpMagicNumber) {
            trade.PositionClose(ticket);
         }
      }
   }
   Print("All positions closed for magic ", InpMagicNumber);
}

//+------------------------------------------------------------------+
double ExtractParamDouble(string json, string key) {
   string search = "\"" + key + "\":";
   int pos = StringFind(json, search);
   if (pos < 0) return -1;
   pos += StringLen(search);
   string val = "";
   while (pos < StringLen(json)) {
      ushort ch = StringGetCharacter(json, pos);
      if (ch == ',' || ch == '}' || ch == ']') break;
      val += ShortToString(ch);
      pos++;
   }
   return StringToDouble(val);
}

//+------------------------------------------------------------------+
int ExtractParamInt(string json, string key) {
   string search = "\"" + key + "\":";
   int pos = StringFind(json, search);
   if (pos < 0) return -1;
   pos += StringLen(search);
   string val = "";
   while (pos < StringLen(json)) {
      ushort ch = StringGetCharacter(json, pos);
      if (ch == ',' || ch == '}' || ch == ']') break;
      val += ShortToString(ch);
      pos++;
   }
   return StringToInteger(val);
}
//+------------------------------------------------------------------+
