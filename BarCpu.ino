/*
Ecco lo specchietto finale per conferma:

    Pompe (8): GPIO0, GPIO1, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7, GPIO10

    LED WS2812B (1): GPIO2

    Pulsante (1): GPIO9

    Bilancia HX711 (2): GPIO20, GPIO21

POMPA   LISCIO  GAS Leggero   GAS
P3/P4   8.265   -             -
Altre   9.025   8.89          8.7


pagine:

http://192.168.1.67/setup

http://192.168.1.67/setup_pompe

http://192.168.1.67/testluci

http://192.168.1.67/wifisetup

*/  


#include <HX711.h>
#include "FastLEDmin.h"  //https://gitlab.com/Vishal1695/fastled_min


#define NUM_LEDS 14
#define BRIGHTNESS 30

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include "esp_bt.h"
#include <DNSServer.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;



// --- WRAPPER SICURO SERIALE + WI-FI TELNET ---
class SafeDualSerial : public Print {
  WiFiServer server{23};
  WiFiClient client;
  bool ready = false;

public:
  void begin(unsigned long baud) {
    Serial.begin(baud);
    // Non avviamo il server qui: deve essere avviato solo DOPO il Wi-Fi!
  }

  void startTelnet() {
    server.begin();
    server.setNoDelay(true); // Riduce la latenza dei pacchetti
    ready = true;
  }

  void handle() {
    if (!ready) return;

    if (server.hasClient()) {
      if (client) client.stop();
      client = server.available();

      // MESSAGGIO DI BENVENUTO QUANDO TI CONNETTI DA PUTTY
      client.println("\n=================================");
      client.println("  CONNESSO AL DEBUG ESP32-C3!");
      client.println("=================================\n");
    }
  }

  size_t write(uint8_t c) override {
    Serial.write(c); // La seriale USB funziona sempre in sicurezza
    
    // Invia via Wi-Fi SOLO se la rete è pronta e il client è connesso
    if (ready && client && client.connected()) {
      client.write(c);
    }
    return 1;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    Serial.write(buffer, size);
    if (ready && client && client.connected()) {
      client.write(buffer, size);
    }
    return size;
  }

  int available() { return Serial.available(); }
  int read() { return Serial.read(); }
  int peek() { return Serial.peek(); }
  void flush() { Serial.flush(); }
};

SafeDualSerial RemoteSerial;
#define Serial RemoteSerial

CRGB leds[NUM_LEDS];


const uint32_t BREATHE_DURATION = 4000; // 4 secondi per ogni respiro

const char IMG_LOGO[] PROGMEM = 
"data:image/png;base64,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";   // la tua base64

// --------------------------------------------------
// CONFIGURAZIONE HARDWARE
// --------------------------------------------------
const int pumpPin[8] = {0, 1, 3, 4, 5, 6, 7, 10};
const int NUM_PUMPS = sizeof(pumpPin) / sizeof(pumpPin[0]);

// Se una pompa resta OFF oltre questa soglia, prima del dosaggio fa un breve innesco anti-aria.
const unsigned long x_pompa_off = 1UL * 60UL * 1000UL; // 1 minuti (ms)
const unsigned long x_pompa_innesco_ms = 500UL;         // Extra fisso (ms)

unsigned long ultimoUsoPompaMs[8] = {0};

inline bool isPumpIdValido(int id) {
  return id >= 0 && id < NUM_PUMPS;
}

inline void segnaUsoPompa(int id) {
  if (isPumpIdValido(id)) {
    ultimoUsoPompaMs[id] = millis();
  }
}

const int BUTTON_PIN = 9;   // Tasto in parallelo al boot
const int LED_PIN = 2;      // Striscia LED
const int HX711_DT = 20;    // Bilancia Data (RX)
const int HX711_SCK = 21;   // Bilancia Clock (TX)


unsigned long ultimoTempoBottone = 0;
const unsigned long debounceDelay = 250; // Millisecondi di attesa antirimbalzo
bool bottonePremuto = false;


HX711 scale;

// --------------------------------------------------
// STRUTTURE DATI
// --------------------------------------------------
struct Ingrediente {
  String nome;
  int ml;
  float p;
};

struct RicettaIngrediente {
  int id;
  int ml;
};

struct Ricetta {
  String nome;
  std::vector<RicettaIngrediente> ingredienti;
};

struct Parametri {
  int Ric_default;
  int velocita;
  int ripetizioni;
};

struct UtenteStat {
  String nome;
  String ip;
  uint32_t bevande_servite;
};

// --------------------------------------------------
// VARIABILI GLOBALI
// --------------------------------------------------
std::vector<Ingrediente> ingredienti;
std::vector<Ricetta> ricette;
std::vector<String> frasi;
Parametri parametri;
std::vector<UtenteStat> utenti;

WebServer server(80);

const int WIFI_TX_POWER_MIN = 16; // 4 dBm (step ESP-IDF: 0.25 dBm)
const int WIFI_TX_POWER_HIGHEST = 84;

String wifi_ssid = "";
String wifi_pass = "";

// Stato calibrazione portata pompa (100 ml tramite pulsante hardware)
bool setupPortataAttiva = false;
int setupPortataPumpId = 0;
unsigned long setupPortataAccumMs = 0;
bool setupPortataButtonDown = false;
unsigned long setupPortataPressStartMs = 0;
bool setupPortataInAttesaSalvataggio = false;
int setupPortataPendingPumpId = 0;
float setupPortataPendingMlS = 0.0f;
const float setupPortataMsFactor = 1.09f;

// --------------------------------------------------
// FILE SYSTEM
// --------------------------------------------------
String loadFile(const char* path) {
  if (!SPIFFS.exists(path)) return "{}";
  File f = SPIFFS.open(path, "r");
  String s = f.readString();
  f.close();
  return s;
}

void saveFile(const char* path, const String& data) {
  File f = SPIFFS.open(path, "w");
  f.print(data);
  f.close();
}

bool isVelocitaWiFiValida(int velocita) {
  return velocita >= WIFI_TX_POWER_MIN && velocita <= WIFI_TX_POWER_HIGHEST;
}

int normalizzaVelocitaWiFi(int velocita) {
  if (isVelocitaWiFiValida(velocita)) return velocita;
  return WIFI_TX_POWER_HIGHEST;
}

void applicaPotenzaWiFi() {
  parametri.velocita = normalizzaVelocitaWiFi(parametri.velocita);
  esp_err_t err = esp_wifi_set_max_tx_power((int8_t)parametri.velocita);
  
  if (err == ESP_OK) {
    Serial.printf("Potenza WiFi impostata da parametri.velocita: %d (%.2f dBm)\n", parametri.velocita, parametri.velocita / 4.0f);
  } else {
    Serial.printf("Errore esp_wifi_set_max_tx_power(%d): %d\n", parametri.velocita, err);
  }
/*
esp_err_t psErr = esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
  if (psErr == ESP_OK) {
    Serial.println("WiFi power save impostato: WIFI_PS_MAX_MODEM");
  } else {
    Serial.printf("Errore esp_wifi_set_ps(WIFI_PS_MAX_MODEM): %d\n", psErr);
  }
  */
}

// --------------------------------------------------
// PARSE DB.JSON
// --------------------------------------------------
void parseDB() {
  String json = loadFile("/db.json");
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("Errore JSON db.json");
    return;
  }

  // INGREDIENTI
  ingredienti.clear();
  JsonArray arrIng = doc["ingredienti"].as<JsonArray>();
  for (JsonVariant v : arrIng) {
    Ingrediente i;
    i.nome = v["nome"].as<String>();
    i.ml   = v["ml"];
    i.p    = v["p"];
    ingredienti.push_back(i);
  }

  // RICETTE
  ricette.clear();
  JsonArray arrRic = doc["ricette"].as<JsonArray>();
  for (JsonVariant r : arrRic) {
    Ricetta ric;
    ric.nome = r["nome"].as<String>();

    JsonArray arrRi = r["ingredienti"].as<JsonArray>();
    for (JsonVariant ri : arrRi) {
      RicettaIngrediente x;
      x.id = ri["id"];
      x.ml = ri["ml"];
      ric.ingredienti.push_back(x);
    }

    ricette.push_back(ric);
  }

  // PARAMETRI
  parametri.Ric_default = doc["parametri"]["Ric_default"];
  parametri.velocita    = doc["parametri"]["velocita"];
  parametri.ripetizioni = doc["parametri"]["ripetizioni"];
  int velocitaNormalizzata = normalizzaVelocitaWiFi(parametri.velocita);
  bool velocitaCorretta = velocitaNormalizzata != parametri.velocita;
  parametri.velocita = velocitaNormalizzata;
  doc["parametri"]["velocita"] = parametri.velocita;

  // FRASI
  frasi.clear();
  JsonArray arrFrasi = doc["frasi"].as<JsonArray>();
  for (JsonVariant f : arrFrasi) {
    frasi.push_back(f.as<String>());
  }

  if (velocitaCorretta) {
    String jsonNormalizzato;
    serializeJson(doc, jsonNormalizzato);
    saveFile("/db.json", jsonNormalizzato);
    Serial.println("parametri.velocita non valido: forzato alla potenza WiFi massima");
  }
}

// --------------------------------------------------
// UTENTI
// --------------------------------------------------
void loadUtenti() {
  String json = loadFile("/utenti.json");
  DynamicJsonDocument doc(4096);
  deserializeJson(doc, json);

  utenti.clear();
  JsonArray arr = doc.as<JsonArray>();

  for (JsonVariant v : arr) {
    UtenteStat u;
    u.nome = v["nome"].as<String>();
    u.ip   = v["ip"].as<String>();
    u.bevande_servite = v["bevande_servite"];
    utenti.push_back(u);
  }
}

void salvaDB() {
  DynamicJsonDocument doc(8192); // Stessa dimensione usata per il parse

  // 1. INGREDIENTI
  JsonArray arrIng = doc.createNestedArray("ingredienti");
  for (const auto& i : ingredienti) {
    JsonObject v = arrIng.createNestedObject();
    v["nome"] = i.nome;
    v["ml"]   = i.ml;
    v["p"]    = i.p;
  }

  // 2. RICETTE
  JsonArray arrRic = doc.createNestedArray("ricette");
  for (const auto& ric : ricette) {
    JsonObject r = arrRic.createNestedObject();
    r["nome"] = ric.nome;

    JsonArray arrRi = r.createNestedArray("ingredienti");
    for (const auto& x : ric.ingredienti) {
      JsonObject ri = arrRi.createNestedObject();
      ri["id"] = x.id;
      ri["ml"] = x.ml;
    }
  }

  // 3. PARAMETRI
  JsonObject param = doc.createNestedObject("parametri");
  param["Ric_default"] = parametri.Ric_default;
  param["velocita"]    = parametri.velocita;
  param["ripetizioni"] = parametri.ripetizioni;

  // 4. FRASI
  JsonArray arrFrasi = doc.createNestedArray("frasi");
  for (const auto& f : frasi) {
    arrFrasi.add(f);
  }

  // Serializzazione del JSON in stringa
  String json;
  serializeJson(doc, json);

  // Salvataggio effettivo su SPIFFS usando la tua funzione saveFile
  saveFile("/db.json", json);
}
void saveUtenti() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();

  for (auto& u : utenti) {
    JsonObject o = arr.createNestedObject();   // ✔ FUNZIONA SEMPRE
    o["nome"] = u.nome;
    o["ip"] = u.ip;
    o["bevande_servite"] = u.bevande_servite;
  }

  String out;
  serializeJsonPretty(doc, out);
  saveFile("/utenti.json", out);
}

void resetUtenti() {
  utenti.clear();            // svuota il vettore
  saveFile("/utenti.json", "[]");   // salva array vuoto

  
  Serial.println("Utenti azzerati !");
}

void aggiornaUtente(const String& ip) {
  for (auto& u : utenti) {
    if (u.ip == ip) {
      u.bevande_servite++;
      saveUtenti();
      return;
    }
  }

  UtenteStat nuovo;
  nuovo.nome = "Utente " + ip;
  nuovo.ip = ip;
  nuovo.bevande_servite = 1;
  utenti.push_back(nuovo);
  saveUtenti();
}
void loadWiFiConfig() {
  if (!SPIFFS.exists("/wifi.json")) {
    saveFile("/wifi.json", "{\"ssid\":\"d_mercanti_2570\",\"password\":\"0192837465\"}");
  }

  String json = loadFile("/wifi.json");
  DynamicJsonDocument doc(512);
  deserializeJson(doc, json);

  wifi_ssid = doc["ssid"].as<String>();
  wifi_pass = doc["password"].as<String>();
}
void creaDBBase() {
  DynamicJsonDocument doc(4096);

  JsonArray ingr = doc.createNestedArray("ingredienti");
  {
    JsonObject o = ingr.createNestedObject();
    o["nome"] = "Acqua";
    o["ml"] = 500;
    o["p"] = 1.0;

    o = ingr.createNestedObject();
    o["nome"] = "Vino Bianco";
    o["ml"] = 500;
    o["p"] = 1.0;

    o = ingr.createNestedObject();
    o["nome"] = "Aperol";
    o["ml"] = 300;
    o["p"] = 1.0;
  }

  JsonArray ric = doc.createNestedArray("ricette");
  {
    JsonObject r = ric.createNestedObject();
    r["nome"] = "Spritz Classico";
    JsonArray ri = r.createNestedArray("ingredienti");
    JsonObject x = ri.createNestedObject();
    x["id"] = 1;
    x["ml"] = 60;
    x = ri.createNestedObject();
    x["id"] = 2;
    x["ml"] = 40;

    r = ric.createNestedObject();
    r["nome"] = "Spritz Light";
    ri = r.createNestedArray("ingredienti");
    x = ri.createNestedObject();
    x["id"] = 1;
    x["ml"] = 40;
    x = ri.createNestedObject();
    x["id"] = 2;
    x["ml"] = 20;
    x = ri.createNestedObject();
    x["id"] = 0;
    x["ml"] = 20;
  }

  JsonObject par = doc.createNestedObject("parametri");
  par["Ric_default"] = 0;
  par["velocita"] = WIFI_TX_POWER_HIGHEST;
  par["ripetizioni"] = 1;

  JsonArray fr = doc.createNestedArray("frasi");
  fr.add("Se non ti piace il cocktail , parla col tecnico, non col barman!");
  fr.add("Attenzione: dopo questo drink potresti diventare più simpatico del previsto.");
  fr.add("Shakerando… anche se qui non c’è nessuno che shakera davvero.");
  fr.add("Cin cin! Se senti la testa leggera… è il WiFi, non l’alcol.");
  fr.add("Il tuo drink è arrivato. Lui sì che ti capisce.");
  fr.add("Il tuo drink è arrivato. Lui sì che ti capisce.");
  fr.add("Le amicizie migliori iniziano con: ‘Ne facciamo un altro?’");
  fr.add("Sete rilevata. Avvio procedura di idratazione… alcolica.");
  fr.add("Complimenti! Hai sbloccato il livello ‘Bevitore Esperto’.");
  fr.add("Questo cocktail è talmente buono che potresti volerlo abbracciare.");
  fr.add("Bere da soli è triste. Bere in compagnia è un bug che non correggeremo mai.");
  fr.add("Secondo i miei calcoli… ti mancava proprio questo drink.");
  fr.add("Bevi responsabilmente. O almeno divertiti mentre ci provi.");
  fr.add("Complimenti! Hai sbloccato il livello ‘Bevitore Esperto’.");
  String out;
  serializeJsonPretty(doc, out);
  saveFile("/db.json", out);

  Serial.println("Creato db.json base");
}


void flashStrip(CRGB color, uint32_t flash_speed_ms, uint32_t total_duration_ms) {
  uint32_t start_time = millis();
  bool leds_on = true;

  // Continua a lampeggiare finché non scade il tempo totale
  while (millis() - start_time < total_duration_ms) {
    
    if (leds_on) {
      // 1. Accendi tutti i led con il colore scelto
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
      }
    } else {
      // 2. Spegni tutti i led
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
      }
    }
    
    // Invia i dati alla striscia
    FastLED_min<LED_PIN>.show();
    
    // Inverte lo stato per il prossimo ciclo (se erano accesi si spengono, e viceversa)
    leds_on = !leds_on;
    
    // Pausa che determina la velocità del lampeggio
    delay(flash_speed_ms);
  }
  
  // Spegne definitivamente i led alla fine dell'effetto
  FastLED_min<LED_PIN>.clear();
  FastLED_min<LED_PIN>.show();
}

void bounceLed(CRGB color, uint32_t wait_per_step_ms, uint32_t total_duration_ms) {
  uint32_t start_time = millis();
  int current_led = 0;
  int direction = 1; // 1 significa avanti, -1 significa indietro

  // Il ciclo continua finché non scade il tempo totale impostato
  while (millis() - start_time < total_duration_ms) {
    
    // 1. Spegni tutta la striscia prima di disegnare il nuovo frame
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
    
    // 2. Accendi solo il led nella posizione corrente
    leds[current_led] = color;
    
    // 3. Mostra il risultato sulla striscia
    FastLED_min<LED_PIN>.show();
    
    // 4. Pausa che determina la velocità del rimbalzo
    delay(wait_per_step_ms);
    
    // 5. Calcola la prossima posizione
    current_led += direction;
    
    // 6. Gestisci il rimbalzo ai due estremi della striscia
    if (current_led >= NUM_LEDS - 1) {
      current_led = NUM_LEDS - 1;
      direction = -1; // Inverte la marcia verso indietro
    } 
    else if (current_led <= 0) {
      current_led = 0;
      direction = 1;  // Inverte la marcia verso avanti
    }
  }
  
  // Opzionale: spegne la striscia alla fine dell'effetto prima di uscire dalla funzione
  FastLED_min<LED_PIN>.clear();
  FastLED_min<LED_PIN>.show();
}

void breatheStrip(uint32_t duration_ms) {
  // 1. Array di 16 colori definito direttamente dentro la funzione
  const int NUM_COLORS = 16;
  /*
  static const CRGB colorPalette[NUM_COLORS] = {
    CRGB::Red,     CRGB::Green,   CRGB::Blue,    CRGB::Yellow,
    CRGB::Magenta, CRGB::Cyan,    CRGB::White,   CRGB::Orange,
    CRGB::Purple,  CRGB::Pink,    CRGB(255, 69, 0),   // OrangeRed
    CRGB(0, 255, 127),  // SpringGreen
    CRGB(128, 255, 0),  // Lime
    CRGB(255, 20, 147), // DeepPink
    CRGB(0, 191, 255),  // DeepSkyBlue
    CRGB(139, 69, 19)   // SaddleBrown
  };
*/
static const CRGB colorPalette[NUM_COLORS] = {
    CRGB::Red,     CRGB::Green,   CRGB::Blue,    CRGB::Yellow,
    CRGB(0, 0, 0), CRGB::Cyan,    CRGB::White,   CRGB::Orange,
    CRGB::Purple,  CRGB::Pink,    CRGB(255, 69, 0),   // OrangeRed
    CRGB(0, 255, 127),  // SpringGreen
    CRGB(0, 0, 0),  // Lime
    CRGB(255, 20, 147), // DeepPink
    CRGB(0, 191, 255),  // DeepSkyBlue
    CRGB(139, 69, 19)   // SaddleBrown
  };
  // 2. Variabili statiche: mantengono il valore tra una chiamata e l'altra
  static uint8_t colorIndex = 0;
  static uint32_t cycleStartTime = millis();
  
  uint32_t elapsed = millis() - cycleStartTime;

  // 3. Quando il tempo del singolo respiro è scaduto, resettiamo il timer e cambiamo colore
  if (elapsed >= duration_ms) {
    cycleStartTime = millis();
    elapsed = 0;
    
    colorIndex++;
    if (colorIndex >= NUM_COLORS) {
      colorIndex = 0; // Ricomincia dal primo colore
    }
  }

  // 4. Calcoliamo l'angolo basandoci sul tempo trascorso dall'INIZIO DI QUESTO CICLO
  float angle = (2.0f * PI * elapsed) / duration_ms;
  uint8_t target_brightness = (uint8_t)((1.0f - cosf(angle)) * 127.5f);
  
  // 5. Applichiamo il colore corrente e la luminosità
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = colorPalette[colorIndex];
  }
  
  FastLED_min<LED_PIN>.setBrightness(target_brightness);
  FastLED_min<LED_PIN>.show();
}
void breatheStrip___(CRGB color, uint32_t duration_ms) {
  // Calcola il tempo corrente in millisecondi
  uint32_t now = millis();
  
  // Converte il tempo in un angolo in radianti basato sulla durata totale del respiro
  // 2 * PI * tempo_corrente / durata_totale
  float angle = (2.0f * PI * now) / duration_ms;
  
  // La funzione cos() oscilla tra -1 e 1. 
  // La trasformiamo per farla oscillare tra 0 e 255.
  // Usiamo il coseno invertito (1 - cos) così l'effetto parte da spento (0)
  uint8_t target_brightness = (uint8_t)((1.0f - cosf(angle)) * 127.5f);
  
  // 1. Riempi la striscia con il colore desiderato
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
  
  // 2. Imposta la luminosità calcolata per questo istante
  FastLED_min<LED_PIN>.setBrightness(target_brightness);
  
  // 3. Invia i dati ai LED
  FastLED_min<LED_PIN>.show();
}

void fillStrip(CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = color;
    }
    FastLED_min<LED_PIN>.show();
  }


void controllaLivelliIngredienti() {
  Serial.println(F("\n--- VERIFICA LIVELLI INGREDIENTI ---"));
  
  bool allOk = true;

  for (const auto& ing : ingredienti) {
   
      Serial.print(ing.nome);
      Serial.print(F(" -> Rimanenti: "));
      Serial.print(ing.ml);
      Serial.println(F(" ml ("));
      
    if (ing.ml <= 0) {
      Serial.print(F("❌ AGGIUNGERE IMMEDIATAMENTE: "));
      Serial.print(ing.nome);
      Serial.println(F(" è FINITO! (0 ml)"));
      allOk = false;
    } 
    else if (ing.ml <= 50.0) {
      Serial.print(ing.nome);
      Serial.println(F("⚠️ IN ESAURIMENTO (<30ml): "));
      
      
      allOk = false;
    }
  }

  if (allOk) {
    Serial.println(F("✅ Tutti gli ingredienti sono a livelli sufficienti (>10%)."));
  }
  Serial.println(F("------------------------------------\n"));
}

bool ricettaDisponibile(int idx) {
  // 1. Verifica di sicurezza sull'indice della ricetta
  if (idx < 0 || idx >= (int)ricette.size()) {
    Serial.println(F("Controllo disponibilità: Indice ricetta non valido!"));
    return false;
  }

  // 2. Ciclo sugli ingredienti richiesti da questa specifica ricetta
  for (const auto& ri : ricette[idx].ingredienti) {
    
    // Cerchiamo l'ingrediente corrispondente usando l'indice (idI)
    int idI = 0;
    bool ingredienteTrovato = false;
    
    for (const auto& ing : ingredienti) {
      if (idI == ri.id) {
        ingredienteTrovato = true;
        
        // CONTROLLO CRITICO: Verifica se i ml rimasti nel bar sono inferiori a quelli richiesti dalla ricetta
        if (ing.ml < ri.ml) {
          Serial.printf("Disponibilità KO: Per la ricetta '%s' manca l'ingrediente '%s' (Richiesti: %d ml, Disponibili: %d ml)\n", 
                        ricette[idx].nome.c_str(), ing.nome.c_str(), ri.ml, ing.ml);
          return false; // Non c'è abbastanza liquido, blocca subito e ritorna false
        }
        break; // Trovato l'ingrediente, esce dal ciclo interno per passare al prossimo ingrediente della ricetta
      }
      idI++;
    }

    // Gestione di sicurezza se per errore l'id dell'ingrediente non esiste nel vettore globale
    if (!ingredienteTrovato) {
      Serial.printf("Disponibilità KO: Ingrediente ID %d non trovato nel database!\n", ri.id);
      return false;
    }
  }

  // Se il ciclo termina senza mai entrare negli "if" di blocco, significa che tutti i liquidi bastano
  return true; 
}

// --------------------------------------------------
// EROGAZIONE POMPE
// --------------------------------------------------
void erogaIngrediente_____nobreak(int idIng, int ml) {
  float sec = (float)ml / ingredienti[idIng].p;  // p = ml/s
  // Convertiamo esplicitamente in millisecondi (unsigned long)
  unsigned long ms = (unsigned long)(sec * 1000.0);
  Serial.println("Portata  e ms");
  
  Serial.println( ingredienti[idIng].p);
  Serial.println( ms);
  digitalWrite(pumpPin[idIng], HIGH);
  delay(ms);
  digitalWrite(pumpPin[idIng], LOW);
}
int erogaIngrediente(int idIng, int ml) {
  if (!isPumpIdValido(idIng)) {
    Serial.printf("Errore: pompa non valida (%d)\n", idIng);
    return 0;
  }

  unsigned long nowMs = millis();
  if (ultimoUsoPompaMs[idIng] == 0 || (nowMs - ultimoUsoPompaMs[idIng]) > x_pompa_off) {
    Serial.printf("Pre-innesco pompa %d per %lu ms\n", idIng, x_pompa_innesco_ms);
    digitalWrite(pumpPin[idIng], HIGH);
    delay(x_pompa_innesco_ms);
    digitalWrite(pumpPin[idIng], LOW);
    segnaUsoPompa(idIng);
    delay(20); // breve pausa per stabilizzare il flusso prima del dosaggio reale
  }

  float sec = (float)ml / ingredienti[idIng].p; // p = ml/s
  unsigned long msTarget = (unsigned long)(sec * 1000.0);

  digitalWrite(pumpPin[idIng], HIGH);
  segnaUsoPompa(idIng);
  
  unsigned long startMs = millis();
  unsigned long elapsedMs = 0;
  bool interrotto = false;

  // Ciclo di erogazione non bloccante
  while (millis() - startMs < msTarget) {
    // Legge il pulsante (PULLUP -> LOW = premuto)
    if (digitalRead(BUTTON_PIN) == LOW) {
      interrotto = true;
      break; // Esci immediatamente dal ciclo while
    }
    delay(10); // Piccolissima pausa per stabilità
  }

  // Calcola il tempo effettivo prima dello spegnimento della pompa
  elapsedMs = millis() - startMs;
  digitalWrite(pumpPin[idIng], LOW); // Spegni SUBITO la pompa
  segnaUsoPompa(idIng);

  // Calcola i millilitri erogati in base al tempo trascorso
  float secEffettivi = (float)elapsedMs / 1000.0;
  int mlErogati = (int)(secEffettivi * ingredienti[idIng].p);

  // Evita che per arrotondamenti superi i ml richiesti inizialmente
  if (mlErogati > ml) mlErogati = ml;

  if (interrotto) {
    Serial.print(F("Erogazione INTERROTTA! ml erogati reali: "));
    Serial.println(mlErogati);
  } else {
    Serial.print(F("Erogazione completata: "));
    Serial.println(mlErogati);
  }

  return mlErogati;
}
void eseguiRicetta(int idx) {
  if (idx < 0 || idx >= (int)ricette.size()) {
    Serial.println(F("Errore: Indice ricetta non valido!"));
    return;
  }

  FastLED_min<LED_PIN>.setBrightness(BRIGHTNESS);
  FastLED_min<LED_PIN>.clear();            
  FastLED_min<LED_PIN>.show();

  flashStrip(CRGB::Red, 60, 3000);
  fillStrip(CRGB::Red);

  String ip = server.client().remoteIP().toString();
  aggiornaUtente(ip);
    
  bool interrotto = false;

  // Ciclo sugli ingredienti previsti dalla ricetta
  for (auto& ri : ricette[idx].ingredienti) {
    bounceLed(CRGB::Red, 45, 1500);
    fillStrip(CRGB::Red);

    // 1. Eroga e scopri quanti ml sono stati erogati davvero
    int mlErogatiReali = erogaIngrediente(ri.id, ri.ml);

    // 2. Decrementa la quantità residua nel database con i ml REALI
    int idI=0;
    for (auto& ing : ingredienti) {
       if (idI == ri.id) {
        ing.ml -= mlErogatiReali;
        if (ing.ml < 0) ing.ml = 0;
        break;
      }
       idI=idI+1;
    }

    // Se i ml erogati sono inferiori a quelli previsti, l'utente ha premuto STOP
    if (mlErogatiReali < ri.ml) {
      interrotto = true;
      break; // Interrompe l'erogazione dei successivi ingredienti della ricetta
    }
  }

  // Riscontro visivo in base al risultato
  if (interrotto) {
    flashStrip(CRGB::Red, 100, 1000); // Segnalazione di stop
    Serial.println(F("Ricetta interrotta dall'utente. DB aggiornato."));
  } else {
    bounceLed(CRGB::Green, 45, 1500);
    flashStrip(CRGB::Green, 60, 3000);
    fillStrip(CRGB::Green);
  }

  controllaLivelliIngredienti();
  salvaDB();
  delay(1500);
}
void eseguiRicetta__nobreak(int idx) {
  // Verifica di sicurezza sull'indice della ricetta
  FastLED_min<LED_PIN>.setBrightness(BRIGHTNESS);
  FastLED_min<LED_PIN>.clear();            
  FastLED_min<LED_PIN>.show();

   flashStrip(CRGB::Red, 60, 3000);
   fillStrip(CRGB::Red);
  if (idx < 0 || idx >= (int)ricette.size()) {
    Serial.println(F("Errore: Indice ricetta non valido!"));
    return;
  }

  String ip = server.client().remoteIP().toString();
  aggiornaUtente(ip);
    
  // Ciclo sugli ingredienti previsti dalla ricetta
  for (auto& ri : ricette[idx].ingredienti) {
    // 1. Eroga fisicamente l'ingrediente
     bounceLed(CRGB::Red, 45, 1500);
     fillStrip(CRGB::Red);
     erogaIngrediente(ri.id, ri.ml);

    // 2. Decrementa la quantità residua nel database degli ingredienti globali
    int idI=0;
    for (auto& ing : ingredienti) {
      if (idI == ri.id) { // Assumendo che la struct Ingrediente abbia la proprietà id
        ing.ml -= ri.ml;
        if (ing.ml < 0) ing.ml = 0; // Evita valori negativi in caso di anomalie
        break;
      }
      idI=idI+1;
    }
  }
  bounceLed(CRGB::Green, 45, 1500);
  flashStrip(CRGB::Green, 60, 3000);
   fillStrip(CRGB::Green);
  // Al termine del cocktail, eseguiamo il controllo dello stato dei liquidi
  controllaLivelliIngredienti();
  
  // (Opzionale) Ricordati di salvare le modifiche su SPIFFS se mantieni i ml salvati nel file JSON
salvaDB();
   delay(1500);
}
void eseguiRicetta__originale(int idx) {
     String ip = server.client().remoteIP().toString();
    aggiornaUtente(ip);
    
  for (auto& ri : ricette[idx].ingredienti) {
    erogaIngrediente(ri.id, ri.ml);
  }
}

// --------------------------------------------------
// PAGINA WEB
// --------------------------------------------------

const char SETUP_LIVELLI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Setup Livelli Ingredienti</title>
<style>
* { box-sizing: border-box; }
body {
  font-family: Arial;
  background: #fafafa;
  margin: 0;
  padding: 0;
  font-size: 18px;
}
.card {
  background: white;
  padding: 20px;
  margin: 12px;
  border-radius: 12px;
  box-shadow: 0 3px 6px rgba(0,0,0,0.25);
}
h2 { margin-top: 0; }
.ing-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px;
  border-bottom: 1px solid #eee;
  border-radius: 8px;
  transition: background-color 0.2s ease;
  flex-wrap: wrap;
  gap: 8px;
}
.ing-item:last-child { border-bottom: none; }

/* Evidenziazione rossa della riga all'attivazione della pompa */
.ing-item.pumping {
  background-color: #ffebee !important;
  border: 1px solid #ef5350;
}

.ing-name {
  font-weight: bold;
  flex: 1;
  min-width: 120px;
}
.ing-input {
  width: 80px;
  padding: 8px;
  font-size: 16px;
  border-radius: 6px;
  border: 1px solid #ccc;
  text-align: center;
}
.btn-group {
  display: flex;
  gap: 8px;
  align-items: center;
}
.btn-save {
  background: #4CAF50;
  color: white;
  padding: 8px 12px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 15px;
}
.btn-save:hover { background: #45a049; }

.btn-pump {
  background: #f44336;
  color: white;
  padding: 8px 12px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 15px;
  user-select: none;
  -webkit-user-select: none;
  touch-action: manipulation;
}
.btn-pump:active, .btn-pump.active {
  background: #b71c1c;
}

.homebtn {
  background: #6200ee;
  color: white;
  padding: 16px 24px;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  font-size: 18px;
  width: 100%;
  margin-top: 20px;
}
</style>
</head>
<body>

<div class="card">
  <h2>Rifornimento Ingredienti</h2>
  <p>Forza il valore in ML o aziona manualmente la pompa:</p>
  <div id="lista-ingredienti">Caricamento in corso...</div>
  
  <button class="homebtn" onclick="location.href='/'">Torna alla Home</button>
</div>

<script>
function caricaIngredienti() {
  fetch('/get-ingredienti')
    .then(r => r.json())
    .then(data => {
      let container = document.getElementById('lista-ingredienti');
      if(data.length === 0) {
        container.innerHTML = "<p>Nessun ingrediente trovato nel database.</p>";
        return;
      }
      
      let html = "";
      data.forEach((ing) => {
        html += `
          <div class="ing-item" id="item_${ing.id}">
            <div class="ing-name">${ing.nome}</div>
            <div style="display: flex; align-items: center; gap: 5px;">
              <input type="number" id="ml_${ing.id}" class="ing-input" value="${ing.ml}" min="0">
              <span style="font-size: 14px; color: #666; margin-right: 8px;">ml</span>
            </div>
            <div class="btn-group">
              <button class="btn-save" onclick="aggiornaLivello(${ing.id})">Aggiorna</button>
              <button class="btn-pump" 
                      onmousedown="avviaPompa(${ing.id})" 
                      onmouseup="fermaPompa(${ing.id})" 
                      onmouseleave="fermaPompa(${ing.id})"
                      ontouchstart="avviaPompaTouch(event, ${ing.id})" 
                      ontouchend="fermaPompaTouch(event, ${ing.id})"
                      ontouchcancel="fermaPompaTouch(event, ${ing.id})">
                Pompa ${ing.id}
              </button>
            </div>
          </div>
        `;
      });
      container.innerHTML = html;
    });
}

function avviaPompa(id) {
  let item = document.getElementById(`item_${id}`);
  if (item) item.classList.add('pumping');
  fetch('/pompaOn?id=' + id);
}

function fermaPompa(id) {
  let item = document.getElementById(`item_${id}`);
  if (item) item.classList.remove('pumping');
  fetch('/pompaOff?id=' + id);
}

function avviaPompaTouch(e, id) {
  e.preventDefault();
  avviaPompa(id);
}

function fermaPompaTouch(e, id) {
  e.preventDefault();
  fermaPompa(id);
}

function aggiornaLivello(id) {
  let nuovoMl = document.getElementById(`ml_${id}`).value;
  
  if (nuovoMl === "" || nuovoMl < 0) {
    alert("Inserisci un valore valido maggiore o uguale a 0!");
    return;
  }

  fetch('/aggiorna-livello', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id: id, ml: parseInt(nuovoMl) })
  })
  .then(r => r.text())
  .then(risposta => {
    alert("Livello aggiornato con successo!");
    caricaIngredienti();
  })
  .catch(err => alert("Errore durante l'aggiornamento."));
}

caricaIngredienti();
</script>

</body>
</html>
)rawliteral";

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Bar ESP32</title>

<style>
* {
  box-sizing: border-box;
}

#overlay {
  position: fixed;
  top:0; left:0;
  width:100%; height:100%;
  background: rgba(0,0,0,0.6);
  color:white;
  display:none;
  justify-content:center;
  align-items:center;
  font-size:24px;
  z-index:999;
}

body {
  font-family: Arial;
  background:#fafafa;
  margin:0;
  padding:0;
  font-size: 18px;
}

.card {
  background:white;
  padding:20px;
  margin:12px;
  border-radius:12px;
  box-shadow:0 3px 6px rgba(0,0,0,0.25);
}

button {
  background:#6200ee;
  color:white;
  padding:16px 24px;
  border:none;
  border-radius:8px;
  cursor:pointer;
  font-size:18px;
  width:100%;
  margin-top:10px;
}

select, input {
  width:100%;
  padding:14px;
  margin-top:12px;
  font-size:18px;
  border-radius:8px;
  border:1px solid #ccc;
}

h3 {
  font-size:22px;
  margin-bottom:10px;
}

/* Stile per il badge della temperatura */
.temp-badge {
  font-weight: bold;
  font-size: 20px;
  color: #333;
  margin-top: 8px;
}
</style>
</head>

<body>
<div id="overlay">Sto preparando il tuo cocktail… 🍹</div>

<div style="text-align:center; margin:20px 0;">
  <img id="logo" src="data:image/png;base64,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" style="max-width:70%; height:auto;">
  <!-- Elemento per visualizzare temperatura e tempo di accensione sotto il logo -->
  <div id="tempBox" class="temp-badge">🌡️ Temp.: -- °C | ON: --:--</div>
</div>

<div class="card" id="loginCard" style="display:none;">
  <h3>Benvenuto!</h3>
  <p>Inserisci il tuo nome per iniziare:</p>
  <input id="nome" placeholder="Il tuo nome">
  <button onclick="registra()">Entra</button>
</div>

<div class="card" id="serviceCard" style="display:none;">
  <h3 id="saluto"></h3>
  <p id="statistiche"></p>
  <p id="frase"></p>

  <h3>Scegli il cocktail</h3>
  <select id="ricetta"></select><br><br>
  <button onclick="servi()">Servi</button>
</div>

<div class="card" id="statCard">
  <button onclick="location.href='/listino'" style="background:#2e7d32; margin-top:10px;">📋 Guarda il Listino Completo</button>
  <button onclick="location.href='/stats'">Statistiche</button>
</div>

<div class="card" id="alertCard" style="display:none; background:#ffebeb; border-left:6px solid #f44336;">
  <h3 style="color:#d32f2f; margin-top:0;">⚠️ Attenzione Livelli Bar</h3>
  <div id="alertLista" style="font-size:16px; color:#333;"></div>
</div>

<script>
let db = {};
let utente = null;

function init() {
  controllaAvvisiIngredienti();
  caricaStatoHome(); // Legge temperatura e tempo ON all'avvio
  setInterval(caricaStatoHome, 60000);
  
  fetch('/utente')
    .then(r => r.json())
    .then(u => {
      if (!u.registrato) {
        document.getElementById('loginCard').style.display = 'block';
      } else {
        utente = u;
        caricaDB();
      }
    });

  
  
  
}

function caricaStatoHome() {
  Promise.all([
    fetch('/temperatura').then(r => r.text()),
    fetch('/uptime').then(r => r.text())
  ])
    .then(([temp, uptime]) => {
      document.getElementById('tempBox').innerHTML = '🌡️ Temp.: ' + temp + ' °C | ON: ' + uptime;
    })
    .catch(() => {
      document.getElementById('tempBox').innerHTML = '🌡️ Temp.: -- °C | ON: --:--';
    });
}

function controllaAvvisiIngredienti() {
  fetch('/livelli-allerta')
    .then(r => r.json())
    .then(avvisi => {
      let alertCard = document.getElementById('alertCard');
      let alertLista = document.getElementById('alertLista');
      
      if (avvisi.length === 0) {
        alertCard.style.display = 'none';
        return;
      }

      alertCard.style.display = 'block';
      let html = "";

      avvisi.forEach(a => {
        if (a.stato === "finito") {
          html += `<p style="margin:5px 0;">❌ <b>${a.nome}</b> è completamente <b>FINITO</b> (0 ml)!</p>`;
        } else {
          html += `<p style="margin:5px 0;">⚠️ <b>${a.nome}</b> è in esaurimento: rimangono solo <b>${a.ml} ml</b></p>`;
        }
      });

      alertLista.innerHTML = html;
    });
}

function registra() {
  let nomeInput = document.getElementById('nome').value;
  let nome = nomeInput.trim(); 

  if (nome === "") {
    alert("Per favore, inserisci un nome valido prima di entrare!");
    return; 
  }
  
  fetch('/registra', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({nome:nome})
  }).then(() => location.reload());
}

function caricaDB() {
  fetch('/config').then(r=>r.json()).then(j=>{
    db = j;

    document.getElementById('serviceCard').style.display = 'block';

    document.getElementById('saluto').textContent =
      "Ciao " + utente.nome + "!";

    document.getElementById('statistiche').textContent =
      "Hai già bevuto " + utente.bevande + " cocktail. Totale serviti: " + utente.totale;

    let f = db.frasi[Math.floor(Math.random()*db.frasi.length)];
    document.getElementById('frase').textContent = f;

    let sel = document.getElementById('ricetta');
    sel.innerHTML = '';
    j.ricette.forEach((r,i)=>{
      let o = document.createElement('option');
      o.value = i;
      o.textContent = r.nome;
      sel.appendChild(o);
    });
  });
}

function servi() {
  let idx = document.getElementById('ricetta').value;
  let ricettaScelta = db.ricette[idx];
  let bloccato = false;
  let ingredienteMancante = "";

  ricettaScelta.ingredienti.forEach(ri => {
    let ingDisponibile = db.ingredienti[ri.id]; 
    if (ingDisponibile && ingDisponibile.ml < ri.ml) {
      bloccato = true;
      ingredienteMancante = ingDisponibile.nome;
    }
  });

  if (bloccato) {
    alert("Impossibile avviare: Non c'è abbastanza '" + ingredienteMancante + "' per preparare questo cocktail!");
    return;
  }

  document.getElementById('overlay').style.display = 'flex';
    
  fetch('/servi?ricetta=' + idx)
    .then(r => r.text())
    .then(() => {
      controllaAvvisiIngredienti();
      location.reload();
    });
}

init();
</script>

</body>
</html>
)rawliteral";

const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Setup Sistema</title>
<style>
body { font-family: Arial; background:#fafafa; margin:0; }
.card { background:white; padding:16px; margin:16px; border-radius:8px;
        box-shadow:0 2px 4px rgba(0,0,0,0.2); }
button { background:#6200ee; color:white; padding:10px 20px; border:none;
         border-radius:4px; cursor:pointer; margin-right:10px; margin-bottom:10px; }
button.red { background:#d32f2f; }
button.orange { background:#f97316; }
select { padding:8px; min-width:260px; }
textarea { width:100%; height:400px; font-family: monospace; }
</style>
</head>
<body>

<div class="card">
<h3>Setup JSON</h3>
<form action="https://davidemercanti.altervista.org/baresp32/index.php" method="POST">
    <textarea id="cfg" name="json_data" rows="10" cols="50"></textarea>
    <br><br>
    <button type="submit">Apri nell'Editor di Altervista</button>
</form>
<br>
<button onclick="salva()">Salva su Dispositivo</button>
<button onclick="carica()">Ricarica Locale</button>
<button class="orange" onclick="prelevaDaCloud()">Preleva da Cloud</button>
<a href="/"><button>Home</button></a>
</div>

<div class="card">
<h3>Potenza WiFi</h3>
<p>Valore JSON <b>parametri.velocita</b>, usato da esp_wifi_set_max_tx_power.</p>
<select id="velocitaWifi" onchange="aggiornaVelocitaJson()"></select>
</div>

<div class="card">
<h3>Gestione Utenti</h3>
<p>Resetta completamente il database utenti (nomi, IP, contatori).</p>
<button class="red" onclick="resetUtenti()">Reset Utenti</button>
</div>

<div class="card">
<h3>Gestione Json Globale</h3>
<p>Resetta completamente il database ricette.</p>
<button class="red" onclick="resetJSON()">Reset JSON</button>
</div>

<div class="card">
  <h3>Gestione pompe</h3>
  <button onclick="location.href='/setup_pompe'">Imposta</button>
</div>

<div class="card">
  <h3>Gestione Livelli</h3>
  <button onclick="location.href='/setup_livelli'">Imposta</button>
</div>

<div class="card">
  <h3>Setup Portata Pompa</h3>
  <p>Calibrazione a 100ml con pulsante hardware.</p>
  <button onclick="location.href='/setup_portata'">Apri Setup Portata</button>
</div>

<script>
const VELOCITA_WIFI_MIN = 16;
const VELOCITA_WIFI_MAX = 84;

function initVelocitaOptions(){
 const select = document.getElementById('velocitaWifi');
 select.innerHTML = '';
 for (let value = VELOCITA_WIFI_MAX; value >= VELOCITA_WIFI_MIN; value--) {
   const opt = document.createElement('option');
   opt.value = value;
   opt.textContent = value + ' - ' + (value / 4).toFixed(2) + ' dBm' + (value === VELOCITA_WIFI_MAX ? ' - massima velocita' : '');
   select.appendChild(opt);
 }
}

function normalizzaVelocitaWifi(value){
 const n = Number(value);
 return Number.isInteger(n) && n >= VELOCITA_WIFI_MIN && n <= VELOCITA_WIFI_MAX ? n : VELOCITA_WIFI_MAX;
}

function leggiJsonSetup(){
 return JSON.parse(document.getElementById('cfg').value || '{}');
}

function scriviJsonSetup(json){
 document.getElementById('cfg').value = JSON.stringify(json, null, 2);
}

function syncVelocitaFromJson(){
 const json = leggiJsonSetup();
 if (!json.parametri) json.parametri = {};
 const velocita = normalizzaVelocitaWifi(json.parametri.velocita);
 json.parametri.velocita = velocita;
 document.getElementById('velocitaWifi').value = String(velocita);
 scriviJsonSetup(json);
}

function aggiornaVelocitaJson(){
 const json = leggiJsonSetup();
 if (!json.parametri) json.parametri = {};
 json.parametri.velocita = normalizzaVelocitaWifi(document.getElementById('velocitaWifi').value);
 scriviJsonSetup(json);
}

function carica(){
 fetch('/config').then(r=>r.json()).then(j=>{
   document.getElementById('cfg').value = JSON.stringify(j,null,2);
   syncVelocitaFromJson();
 });
}

function salva(){
 syncVelocitaFromJson();
 fetch('/config',{
   method:'POST',
   headers:{'Content-Type':'application/json'},
   body:document.getElementById('cfg').value
 }).then(()=>alert('Salvato nel dispositivo con successo!'));
}

function prelevaDaCloud(){
 if (!confirm("Vuoi sovrascrivere il testo attuale scaricando la configurazione salvata su Altervista?")) return;

 // Puntiamo a salva.php invece che direttamente al file JSON grezzo
 fetch('https://davidemercanti.altervista.org/baresp32/salva.php?action=get', {
   method: 'GET'
 })
 .then(response => {
   if (!response.ok) throw new Error('Stato server: ' + response.status);
   return response.json();
 })
 .then(jsonCloud => {
   document.getElementById('cfg').value = JSON.stringify(jsonCloud, null, 2);
   syncVelocitaFromJson();
   alert('Dati scaricati dal cloud!');
 })
 .catch(error => {
   alert('Errore durante il download dal cloud: ' + error.message);
 });
}

function resetUtenti(){
 if (!confirm("Sei sicuro di voler cancellare tutti gli utenti?")) return;
 fetch('/resetutenti')
   .then(()=>alert("Utenti resettati"))
   .then(()=>location.reload());
}

function resetJSON(){
 if (!confirm("Sei sicuro di voler cancellare tutte le ricette?")) return;
 fetch('/resetjson')
   .then(()=>alert("Ricette resettate"))
   .then(()=>location.reload());
}

initVelocitaOptions();
carica();
</script>

</body>
</html>
)rawliteral";

const char SETUP_PORTATA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Setup Portata</title>
<style>
* { box-sizing: border-box; }
body {
  font-family: Arial;
  background:#fafafa;
  margin:0;
  padding:0;
  font-size:18px;
}
.card {
  background:white;
  padding:20px;
  margin:12px;
  border-radius:12px;
  box-shadow:0 3px 6px rgba(0,0,0,0.25);
}
button {
  background:#6200ee;
  color:white;
  padding:14px 18px;
  border:none;
  border-radius:8px;
  cursor:pointer;
  font-size:18px;
  width:100%;
  margin-top:10px;
}
button.green { background:#2e7d32; }
button.orange { background:#ef6c00; }
select {
  width:100%;
  padding:12px;
  margin-top:10px;
  border-radius:8px;
  border:1px solid #ccc;
  font-size:18px;
}
.small { font-size:15px; color:#555; }
#out { white-space:pre-line; }
</style>
</head>
<body>

<div class="card">
  <h3>Setup Portata (100ml)</h3>
  <p class="small">1) Seleziona pompa. 2) Premi Avvia. 3) Tieni premuto/rilascia il pulsante hardware fino a raggiungere 100ml nel dosatore. 4) Premi Completa.</p>

  <label>Pompa</label>
  <select id="pump"></select>

  <button class="green" onclick="avvia()">Avvia</button>
  <button class="orange" onclick="completa()">Completa</button>
  <button onclick="location.href='/setup'">Torna a Setup</button>

  <p id="out" class="small">In attesa...</p>
</div>

<script>
function init() {
  const sel = document.getElementById('pump');
  sel.innerHTML = '';
  for (let i = 0; i <= 7; i++) {
    const o = document.createElement('option');
    o.value = i;
    o.textContent = 'Pompa ' + i;
    sel.appendChild(o);
  }
}

function avvia() {
  const id = document.getElementById('pump').value;
  fetch('/setup_portata_avvia?id=' + id)
    .then(r => r.json())
    .then(j => {
      if (j.ok) {
        document.getElementById('out').textContent =
          'Calibrazione avviata. Pompa: ' + j.pump + '\nContatore ms azzerato.';
      } else {
        document.getElementById('out').textContent = 'Errore: ' + (j.err || 'sconosciuto');
      }
    })
    .catch(() => {
      document.getElementById('out').textContent = 'Errore di comunicazione.';
    });
}

function completa() {
  fetch('/setup_portata_completa')
    .then(r => r.json())
    .then(j => {
      if (!j.ok) {
        document.getElementById('out').textContent = 'Errore: ' + (j.err || 'procedura non avviata');
        return;
      }

      const msgBase =
        'Pompa: ' + j.pump + '\n' +
        'Tempo totale bottone: ' + j.pressed_ms + ' ms\n' +
        'Portata calcolata: ' + j.portata_ml_s.toFixed(3) + ' ml/s';

      if (!j.can_save) {
        document.getElementById('out').textContent = msgBase + '\nValore non valido: nessun salvataggio disponibile.';
        return;
      }

      const okSave = confirm(
        msgBase + '\n\nVuoi salvare questa nuova portata per la pompa ' + j.pump + '?'
      );

      if (!okSave) {
        document.getElementById('out').textContent = msgBase + '\nSalvata in db.json: NO';
        return;
      }

      fetch('/setup_portata_salva')
        .then(r => r.json())
        .then(s => {
          if (!s.ok) {
            document.getElementById('out').textContent = msgBase + '\nErrore salvataggio: ' + (s.err || 'sconosciuto');
            return;
          }
          document.getElementById('out').textContent = msgBase + '\nSalvata in db.json: SI';
        })
        .catch(() => {
          document.getElementById('out').textContent = msgBase + '\nErrore di comunicazione durante il salvataggio.';
        });
    })
    .catch(() => {
      document.getElementById('out').textContent = 'Errore di comunicazione.';
    });
}

init();
</script>

</body>
</html>
)rawliteral";




const char WIFISETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8" name="viewport" content="width=device-width, initial-scale=1.0">


<title>WiFi Setup</title>
<style>
* {
  box-sizing: border-box;
}

body {
  font-family: Arial;
  background:#fafafa;
  margin:0;
  padding:0;
  font-size:18px;              /* più grande per smartphone */
}

.card {
  background:white;
  padding:20px;
  margin:12px;
  border-radius:12px;
  box-shadow:0 3px 6px rgba(0,0,0,0.25);
}

button {
  background:#6200ee;
  color:white;
  padding:16px 24px;           /* più grande */
  border:none;
  border-radius:8px;
  cursor:pointer;
  font-size:18px;
  width:100%;                  /* pulsanti full-width */
  margin-top:10px;
}

input {
  width:100%;
  padding:14px;                /* più grande */
  margin-bottom:12px;
  font-size:18px;
  border-radius:8px;
  border:1px solid #ccc;
}

h3 {
  font-size:22px;
  margin-bottom:10px;
}
</style>

</head>
<body>

<div class="card">
  <h3>Informazioni di rete</h3>
  <p><b>IP Access Point:</b> <span id="apip"></span></p>
  <p><b>IP Locale:</b> <span id="localip"></span></p>
  <p><b>SSID:</b> <span id="ssidl"></span></p>
  <p><b>Modalità:</b> <span id="mode"></span></p>
</div>

<div class="card">
<h3>Configurazione WiFi</h3>

<label>SSID</label>
<input id="ssid" placeholder="Nome rete WiFi">

<label>Password</label>
<input id="password" type="password" placeholder="Password">

<button onclick="salva()">Salva e Riavvia</button>
</div>

<script>
function carica() {
  fetch('/getwifi')
    .then(r => r.json())
    .then(j => {
      document.getElementById('ssid').value = j.ssid;
      document.getElementById('password').value = j.password;
    });
}

function salva() {
  const data = {
    ssid: document.getElementById('ssid').value,
    password: document.getElementById('password').value
  };

  fetch('/setwifi', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(data)
  }).then(() => {
    alert("Salvato! Riavvio ESP32...");
    fetch('/reboot');
  });
}
function caricaIP() {
  fetch('/wifiinfo')
    .then(r => r.json())
    .then(info => {
      document.getElementById('apip').textContent = info.ap_ip;
      document.getElementById('localip').textContent = info.local_ip;
      document.getElementById('ssidl').textContent = info.ssid;
      document.getElementById('mode').textContent = info.mode;
    });
}

caricaIP();
carica();
</script>

</body>
</html>
)rawliteral";

const char STAT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Statistiche Bar ESP32</title>

<style>
* { box-sizing: border-box; }

body {
  font-family: Arial;
  background:#fafafa;
  margin:0;
  padding:0;
  font-size:18px;
}
.homebtn {
  background:#6200ee;
  color:white;
  padding:16px 24px;
  border:none;
  border-radius:8px;
  cursor:pointer;
  font-size:18px;
  width:100%;
  margin-top:10px;
}
.card {
  background:white;
  padding:20px;
  margin:12px;
  border-radius:12px;
  box-shadow:0 3px 6px rgba(0,0,0,0.25);
}

h2 { margin-top:0; }

.bar {
  background:#6200ee;
  height:22px;
  border-radius:6px;
}

.row {
  margin-bottom:14px;
}

.name {
  font-weight:bold;
}

.percent {
  float:right;
  font-size:14px;
  opacity:0.7;
}
</style>
</head>
<body>

<div style="text-align:center; margin:20px 0;">
  <img id="logo" style="max-width:70%; height:auto;">
</div>


<div class="card">
  <h2>Statistiche Generali</h2>
  <p id="totale"></p>
  <p id="utenti"></p>
</div>

<div class="card">
  <h2>Classifica Bevitori</h2>
  <div id="lista"></div>
</div>

<div class="card">
<button class="homebtn" onclick="location.href='/'">Torna alla Home</button>


</div>

<script>
function carica() {
  fetch('/statistiche')
    .then(r => r.json())
    .then(data => {
      
      document.getElementById('totale').textContent =
        "Cocktail totali serviti: " + data.totale;

      document.getElementById('utenti').textContent =
        "Utenti registrati: " + data.utenti.length;

      let max = data.utenti.length > 0 ? data.utenti[0].bevande : 1;

      let html = "";
      data.utenti.forEach(u => {
        let perc = data.totale > 0 ? ((u.bevande / data.totale) * 100).toFixed(1) : "0.0";
        let width = (u.bevande / max) * 100;

        html += `
          <div class="row">
            <div class="name">${u.nome} (${u.bevande}) 
              <span class="percent">${perc}%</span>
            </div>
            <div class="bar" style="width:${width}%"></div>
          </div>
        `;
      });

      document.getElementById('lista').innerHTML = html;
    });
}

carica();
</script>

</body>
</html>
)rawliteral";

const char SETUP_POMPE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Setup Pompe</title>

<style>
* { box-sizing: border-box; }

body {
  font-family: Arial;
  background:#fafafa;
  margin:0;
  padding:0;
  font-size:18px;
}

.card {
  background:white;
  padding:20px;
  margin:12px;
  border-radius:12px;
  box-shadow:0 3px 6px rgba(0,0,0,0.25);
}

button {
  background:#6200ee;
  color:white;
  padding:16px 24px;
  border:none;
  border-radius:8px;
  cursor:pointer;
  font-size:18px;
  width:100%;
  margin-top:10px;
}

select {
  width:100%;
  padding:14px;
  margin-top:12px;
  font-size:18px;
  border-radius:8px;
  border:1px solid #ccc;
}
</style>
</head>

<body>

<div class="card">
  <h3>Setup Pompe</h3>

  <label>Seleziona la pompa:</label>
  <select id="pompa"></select>

  <button id="holdBtn">Tieni premuto per attivare</button>
  <button onclick="eroga5()">Eroga 5 secondi</button>

  <button onclick="location.href='/'">Torna alla Home</button>
</div>

<script>
function caricaPompe() {
  fetch('/pompe')
    .then(r => r.json())
    .then(p => {
      let sel = document.getElementById('pompa');
      sel.innerHTML = "";
      p.forEach((nome, i) => {
        let o = document.createElement('option');
        o.value = i;
        o.textContent = nome;
        sel.appendChild(o);
      });
    });
}

function eroga5() {
  let id = document.getElementById('pompa').value;
  fetch('/pompa5?id=' + id);
}

// gestione tasto "tieni premuto"
let holdBtn = null;
let pompaOn=false;
window.onload = () => {
  caricaPompe();

  holdBtn = document.getElementById('holdBtn');

  holdBtn.addEventListener('mousedown', startPump);
  holdBtn.addEventListener('touchstart', startPump);

  holdBtn.addEventListener('mouseup', stopPump);
  holdBtn.addEventListener('mouseleave', stopPump);
  holdBtn.addEventListener('touchend', stopPump);
};

function startPump() {
  let id = document.getElementById('pompa').value;
  pompaOn=true;
  fetch('/pompaOn?id=' + id);
}

function stopPump() {
  if (!pompaOn) return;
  let id = document.getElementById('pompa').value;
 
  fetch('/pompaOff?id=' + id);
   pompaOn=false;
}
</script>

</body>
</html>
)rawliteral";

const char LISTINO_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Listino Cocktail Disponibili</title>
<style>
* { box-sizing: border-box; }

#overlay {
  position: fixed;
  top: 0; left: 0;
  width: 100%; height: 100%;
  background: rgba(0,0,0,0.6);
  color: white;
  display: none;
  justify-content: center;
  align-items: center;
  font-size: 24px;
  z-index: 999;
}

body {
  font-family: Arial;
  background: #fafafa;
  margin: 0;
  padding: 0;
  font-size: 18px;
}
.card {
  background: white;
  padding: 20px;
  margin: 12px;
  border-radius: 12px;
  box-shadow: 0 3px 6px rgba(0,0,0,0.25);
}
h2 { margin-top: 0; }
.cocktail-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 14px 0;
  border-bottom: 1px solid #eee;
}
.cocktail-item:last-child { border-bottom: none; }
.cocktail-info {
  flex: 1;
  padding-right: 10px;
}
.cocktail-name {
  font-weight: bold;
  font-size: 20px;
}
.cocktail-details {
  font-size: 14px;
  color: #666;
  margin-top: 4px;
}
.action-area {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 6px;
}
.badge {
  padding: 6px 12px;
  border-radius: 20px;
  font-size: 14px;
  font-weight: bold;
  text-align: center;
  min-width: 120px;
}
.disponibile {
  background: #e8f5e9;
  color: #2e7d32;
}
.esaurito {
  background: #ffebee;
  color: #c62828;
}
.servebtn {
  background: #4CAF50;
  color: white;
  padding: 8px 16px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 15px;
  font-weight: bold;
  width: 120px;
  text-align: center;
}
.servebtn:hover {
  background: #45a049;
}
.homebtn {
  background: #6200ee;
  color: white;
  padding: 16px 24px;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  font-size: 18px;
  width: 100%;
  margin-top: 20px;
}
</style>
</head>
<body>

<div id="overlay">Sto preparando il tuo cocktail… 🍹</div>

<div class="card">
  <h2>Menu & Disponibilità Bar</h2>
  <p>Ecco quali e quanti cocktail puoi ordinare con le scorte attuali:</p>
  <div id="lista-listino">Caricamento menu in corso...</div>
  
  <button class="homebtn" onclick="location.href='/'">Torna alla Home</button>
</div>

<script>
function caricaListino() {
  fetch('/listino-dati')
    .then(r => r.json())
    .then(data => {
      let container = document.getElementById('lista-listino');
      let ingredienti = data.ingredienti; 
      let ricette = data.ricette;         
      
      if (!ricette || ricette.length === 0) {
        container.innerHTML = "<p>Nessuna ricetta presente nel database.</p>";
        return;
      }
      
      let html = "";
      
      // Recuperiamo l'indice (index) della ricetta per l'URL del servizio
      ricette.forEach((ricetta, index) => {
        let maxCocktail = Infinity; 
        let dettagliIngredienti = [];
        let erroreStruttura = false;

        ricetta.ingredienti.forEach(ri => {
          let ingReale = ingredienti[ri.id]; 
          
          if (ingReale) {
            dettagliIngredienti.push(`${ingReale.nome} (${ri.ml}ml)`);
            
            let quantiConQuesto = Math.floor(ingReale.ml / ri.ml);
            if (quantiConQuesto < maxCocktail) {
              maxCocktail = quantiConQuesto;
            }
          } else {
            erroreStruttura = true;
          }
        });

        if (erroreStruttura || maxCocktail === Infinity) {
          maxCocktail = 0;
        }

        let badgeClass = maxCocktail > 0 ? "badge disponibile" : "badge esaurito";
        let badgeTesto = maxCocktail > 0 ? `Quantità: ${maxCocktail}` : "Non Disponibile";

        // Costruiamo l'area di destra: mostra il bottone solo se maxCocktail > 0
        let bottoneHTML = maxCocktail > 0 
          ? `<button class="servebtn" onclick="servi(${index})">Ordina</button>` 
          : '';

        html += `
          <div class="cocktail-item">
            <div class="cocktail-info">
              <div class="cocktail-name">${ricetta.nome}</div>
              <div class="cocktail-details">${dettagliIngredienti.join(", ")}</div>
            </div>
            <div class="action-area">
              <div class="${badgeClass}">${badgeTesto}</div>
              ${bottoneHTML}
            </div>
          </div>
        `;
      });
      
      container.innerHTML = html;
    })
    .catch(err => {
      document.getElementById('lista-listino').innerHTML = "<p>Errore nel caricamento dei dati.</p>";
    });
}

function servi(idx) {
  // Mostra l'overlay di preparazione
  document.getElementById('overlay').style.display = 'flex';
  
  // Esegue la chiamata al server web dell'ESP32
  fetch('/servi?ricetta=' + idx)
    .then(r => r.text())
    .then(() => {
      // Ricarica il listino per aggiornare le quantità residue a schermo dopo l'erogazione
      location.reload();
    })
    .catch(err => {
      alert("Errore durante l'erogazione!");
      document.getElementById('overlay').style.display = 'none';
    });
}

caricaListino();
</script>

</body>
</html>
)rawliteral";


 void testcolori() {

FastLED_min<LED_PIN>.setBrightness(BRIGHTNESS);
  FastLED_min<LED_PIN>.clear();            
   FastLED_min<LED_PIN>.show();


        fillStrip(CRGB::Red);
        FastLED_min<LED_PIN>.show();
        delay(1500);
         FastLED_min<LED_PIN>.clear();           
      FastLED_min<LED_PIN>.show();

        fillStrip(CRGB::Green);
        FastLED_min<LED_PIN>.show();
        delay(1500);
        FastLED_min<LED_PIN>.clear();            
        FastLED_min<LED_PIN>.show();
        
          fillStrip(CRGB::Blue);
        FastLED_min<LED_PIN>.show();
        delay(1500);
        FastLED_min<LED_PIN>.clear();            
        FastLED_min<LED_PIN>.show();
        
            fillStrip(CRGB::Magenta);
        FastLED_min<LED_PIN>.show();
        delay(1500);

                    fillStrip(CRGB::White);
        FastLED_min<LED_PIN>.show();
        delay(1500);
                FastLED_min<LED_PIN>.clear();            
        FastLED_min<LED_PIN>.show();


    bounceLed(CRGB::Cyan, 75, 2000);

       FastLED_min<LED_PIN>.clear();            
        FastLED_min<LED_PIN>.show();

        flashStrip(CRGB::Orange, 80, 3000);
       
 }

 void testBilancia() {

if (scale.is_ready()) {
    scale.set_scale();    
    Serial.println("Tare... remove any weights from the scale.");
    delay(8000);
    scale.tare();
    Serial.println("Tare done...");
    Serial.print("Place a known weight on the scale...");
    delay(8000);
    long reading = scale.get_units(10);
    Serial.print("Result: ");
    Serial.println(reading);
  } 
  else {
    Serial.println("HX711 not found.");
  }

  
 }
 void pesaBilancia() {

if (scale.is_ready()) {
    Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read());      // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));   // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));   // print the average of 5 readings from the ADC minus the tare weight (not set yet)

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);  // print the average of 5 readings from the ADC minus tare weight (not set) divided
            // by the SCALE parameter (not set yet)
            
  scale.set_scale(-0.02);
  //scale.set_scale(-471.497);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();               // reset the scale to 0

  Serial.println("After setting up the scale:");

  Serial.print("read: \t\t");
  Serial.println(scale.read());                 // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));       // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));   // print the average of 5 readings from the ADC minus the tare weight, set with tare()

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight, divided
            // by the SCALE parameter set with set_scale
  } 
  else {
    Serial.println("HX711 not found.");
  }

  
 }



 void setupCaptivePortal() {
  // 1. Reindirizza qualsiasi query DNS verso l'IP dell'ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // 2. Handler universale di reindirizzamento
  auto handleCaptiveRedirect = []() {
     Serial.println("DNS http://192.168.4.1 ");
    server.sendHeader("Location", "http://192.168.4.1/", true);
    // Android 10 richiede un body HTML col reindirizzamento JS per attivare il popup WebView
    server.send(302, "text/html", "<!DOCTYPE html><html><head><script>location.href='http://192.168.4.1/';</script></head><body>Reindirizzamento al Bar...</body></html>");
  };

  // Endpoint specifici usati da Android e dai vari sistemi operativi per il test connettività
  server.on("/generate_204", handleCaptiveRedirect);
  server.on("/gen_204", handleCaptiveRedirect);
  server.on("/connecttest.txt", handleCaptiveRedirect);
  server.on("/redirect", handleCaptiveRedirect);
  server.on("/hotspot-detect.html", handleCaptiveRedirect); // Per iOS/Apple

  // Tutte le altre rotte non trovate vengono reindirizzate
  server.onNotFound(handleCaptiveRedirect);
}
// --------------------------------------------------
// WEB SERVER
// --------------------------------------------------
void setupServer() {


server.on("/temperatura", HTTP_GET, []() {
  // Legge il sensore di temperatura interno dell'ESP32-C3 in gradi Celsius
  float tempInterna = temperatureRead(); 

  // Invia il valore formattato con 1 cifra decimale
  server.send(200, "text/plain", String(tempInterna, 1));
});

server.on("/uptime", HTTP_GET, []() {
  unsigned long totalMinutes = millis() / 60000UL;
  unsigned long hours = totalMinutes / 60UL;
  unsigned long minutes = totalMinutes % 60UL;
  char uptime[12];
  snprintf(uptime, sizeof(uptime), "%02lu:%02lu", hours, minutes);
  server.send(200, "text/plain", uptime);
});

  server.on("/testbilancia", HTTP_GET, []() {
    testBilancia();
  server.send(200, "text/plain", "");
});
  server.on("/pesaBilancia", HTTP_GET, []() {
    pesaBilancia();
  server.send(200, "text/plain", "");
});

  server.on("/logo", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "max-age=31536000, public");
  server.send(200, "text/plain", IMG_LOGO);
});

  server.on("/testluci", HTTP_GET, []() {
    testcolori();
  server.send(200, "text/plain", "");
});

// 1. Fornisce la pagina web del listino
server.on("/listino", HTTP_GET, []() {
  server.send(200, "text/html", LISTINO_HTML);
});

// 2. Endpoint unificato che impacchetta ricette e ingredienti in un unico JSON
server.on("/listino-dati", HTTP_GET, []() {
  DynamicJsonDocument doc(8192); // Dimensione sufficiente per contenere entrambi i vettori
  
  // Impacchetta gli ingredienti (con i ml rimanenti in memoria)
  JsonArray arrIng = doc.createNestedArray("ingredienti");
  for (const auto& ing : ingredienti) {
    JsonObject obj = arrIng.createNestedObject();
    obj["nome"] = ing.nome;
    obj["ml"] = ing.ml;
  }
  
  // Impacchetta le ricette
  JsonArray arrRic = doc.createNestedArray("ricette");
  for (const auto& ric : ricette) {
    JsonObject obj = arrRic.createNestedObject();
    obj["nome"] = ric.nome;
    
    JsonArray arrRi = obj.createNestedArray("ingredienti");
    for (const auto& x : ric.ingredienti) {
      JsonObject ri = arrRi.createNestedObject();
      ri["id"] = x.id; // L'indice dell'ingrediente
      ri["ml"] = x.ml; // Quanti ml richiede la ricetta
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
});

// 1. Mostra la pagina web di setup livelli
server.on("/setup_livelli", HTTP_GET, []() {
  server.send(200, "text/html", SETUP_LIVELLI_HTML);
});

// 1. Mostra la pagina web di setup livelli
server.on("/setup-livelli", HTTP_GET, []() {
  server.send(200, "text/html", SETUP_LIVELLI_HTML);
});




// 2. Endpoint che restituisce gli ingredienti usando l'indice del vettore come ID
server.on("/get-ingredienti", HTTP_GET, []() {
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  
  // Usiamo un ciclo for con indice per mappare la posizione reale
  for (size_t i = 0; i < ingredienti.size(); i++) {
    JsonObject obj = arr.createNestedObject();
    obj["id"] = i; // L'ID inviato al browser è l'indice stesso del vettore
    obj["nome"] = ingredienti[i].nome;
    obj["ml"] = ingredienti[i].ml;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
});

// 3. Endpoint POST per salvare la modifica tramite l'indice
server.on("/aggiorna-livello", HTTP_POST, []() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body vuoto");
    return;
  }
  
  String body = server.arg("plain");
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "text/plain", "JSON non valido");
    return;
  }
  
  int targetIdx = doc["id"]; // Riceviamo l'indice del vettore
  int nuoviMl = doc["ml"];
  
  // Verifica di sicurezza: controlliamo che l'indice esista nel vettore
  if (targetIdx >= 0 && targetIdx < (int)ingredienti.size()) {
    ingredienti[targetIdx].ml = nuoviMl; // Aggiornamento diretto senza loop!
    
    // Salva permanentemente la modifica nel file db.json su SPIFFS
    salvaDB(); 
    server.send(200, "text/plain", "OK");
    Serial.printf("Livello ingrediente all'indice %d (%s) forzato a %d ml\n", 
                  targetIdx, ingredienti[targetIdx].nome.c_str(), nuoviMl);
  } else {
    server.send(404, "text/plain", "Indice ingrediente non valido");
  }
});
server.on("/livelli-allerta", HTTP_GET, []() {
  DynamicJsonDocument doc(2048); 
  JsonArray avvisi = doc.to<JsonArray>();

  for (const auto& ing : ingredienti) {
    // Controllo a valore fisso: finito se <= 0, in esaurimento se sotto i 10 ml
    if (ing.ml <= 0 || ing.ml <= 50) {
      JsonObject obj = avvisi.createNestedObject();
      obj["nome"] = ing.nome;
      obj["ml"] = ing.ml;
      obj["stato"] = (ing.ml <= 0) ? "finito" : "esaurimento";
    }
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
});
  server.on("/", HTTP_GET, []() {
    //Serial.println(F("index"));
    server.send(200, "text/html", INDEX_HTML);
      //Serial.println(F("index_done"));
  });

server.on("/setup_pompe", HTTP_GET, []() {
  server.send(200, "text/html", SETUP_POMPE_HTML);
});

server.on("/setup_portata", HTTP_GET, []() {
  server.send(200, "text/html", SETUP_PORTATA_HTML);
});

server.on("/setup_portata_avvia", HTTP_GET, []() {
  int id = server.arg("id").toInt();
  if (!isPumpIdValido(id)) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"ID non valido\"}");
    return;
  }

  if (setupPortataButtonDown && isPumpIdValido(setupPortataPumpId)) {
    digitalWrite(pumpPin[setupPortataPumpId], LOW);
    segnaUsoPompa(setupPortataPumpId);
  }

  setupPortataAttiva = true;
  setupPortataPumpId = id;
  setupPortataAccumMs = 0;
  setupPortataButtonDown = false;
  setupPortataPressStartMs = 0;
  setupPortataInAttesaSalvataggio = false;
  setupPortataPendingPumpId = 0;
  setupPortataPendingMlS = 0.0f;

  DynamicJsonDocument doc(256);
  doc["ok"] = true;
  doc["pump"] = setupPortataPumpId;
  doc["reset_ms"] = true;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

server.on("/setup_portata_completa", HTTP_GET, []() {
  if (!setupPortataAttiva) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Procedura non avviata\"}");
    return;
  }

  if (setupPortataButtonDown) {
    unsigned long now = millis();
    setupPortataAccumMs += (now - setupPortataPressStartMs);
    setupPortataButtonDown = false;
    if (isPumpIdValido(setupPortataPumpId)) {
      digitalWrite(pumpPin[setupPortataPumpId], LOW);
      segnaUsoPompa(setupPortataPumpId);
    }
  }

  float setupPortataEffMs = (float)setupPortataAccumMs * setupPortataMsFactor;

  float portata = 0.0f;
  if (setupPortataEffMs > 0.0f) {
    portata = 100000.0f / setupPortataEffMs;
  }

  bool canSave = false;
  if (isPumpIdValido(setupPortataPumpId) && setupPortataPumpId < (int)ingredienti.size() && portata > 0.0f) {
    setupPortataInAttesaSalvataggio = true;
    setupPortataPendingPumpId = setupPortataPumpId;
    setupPortataPendingMlS = portata;
    canSave = true;
  } else {
    setupPortataInAttesaSalvataggio = false;
    setupPortataPendingPumpId = 0;
    setupPortataPendingMlS = 0.0f;
  }

  DynamicJsonDocument doc(256);
  doc["ok"] = true;
  doc["pump"] = setupPortataPumpId;
  doc["pressed_ms"] = (unsigned long)setupPortataEffMs;
  doc["portata_ml_s"] = portata;
  doc["saved"] = false;
  doc["can_save"] = canSave;

  String out;
  serializeJson(doc, out);

  setupPortataAttiva = false;
  setupPortataAccumMs = 0;
  setupPortataPressStartMs = 0;

  server.send(200, "application/json", out);
});

server.on("/setup_portata_salva", HTTP_GET, []() {
  if (!setupPortataInAttesaSalvataggio) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Nessuna portata in attesa di conferma\"}");
    return;
  }

  if (!isPumpIdValido(setupPortataPendingPumpId) || setupPortataPendingPumpId >= (int)ingredienti.size() || setupPortataPendingMlS <= 0.0f) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Dati portata non validi\"}");
    return;
  }

  ingredienti[setupPortataPendingPumpId].p = setupPortataPendingMlS;
  salvaDB();

  DynamicJsonDocument doc(256);
  doc["ok"] = true;
  doc["pump"] = setupPortataPendingPumpId;
  doc["portata_ml_s"] = setupPortataPendingMlS;
  doc["saved"] = true;

  String out;
  serializeJson(doc, out);

  setupPortataInAttesaSalvataggio = false;
  setupPortataPendingPumpId = 0;
  setupPortataPendingMlS = 0.0f;

  server.send(200, "application/json", out);
});

server.on("/pompe", HTTP_GET, []() {
  DynamicJsonDocument doc(256);
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < ingredienti.size(); i++)
    arr.add(String("Pompa ") + i);

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

server.on("/pompaOn", HTTP_GET, []() {
  int id = server.arg("id").toInt();
  if (!isPumpIdValido(id)) {
    server.send(400, "text/plain", "ID non valido");
    return;
  }
  Serial.println(F("POMPA ON"));
  digitalWrite(pumpPin[id], HIGH);
  segnaUsoPompa(id);
  server.send(200, "text/plain", "ON");
});
server.on("/pompaOff", HTTP_GET, []() {
  int id = server.arg("id").toInt();
  if (!isPumpIdValido(id)) {
    server.send(400, "text/plain", "ID non valido");
    return;
  }
  Serial.println(F("POMPA OFF"));
  digitalWrite(pumpPin[id], LOW);
  segnaUsoPompa(id);
  server.send(200, "text/plain", "OFF");
});

server.on("/pompa5", HTTP_GET, []() {
  int id = server.arg("id").toInt();
  if (!isPumpIdValido(id)) {
    server.send(400, "text/plain", "ID non valido");
    return;
  }
  Serial.println(F("POMPA 10 secondi ON"));
  digitalWrite(pumpPin[id], HIGH);
  segnaUsoPompa(id);
  delay(10000);
  digitalWrite(pumpPin[id], LOW);
  segnaUsoPompa(id);
  Serial.println(F("POMPA 10 secondi OFF"));
  server.send(200, "text/plain", "OK");
});

  server.on("/config", HTTP_GET, []() {
    server.send(200, "application/json", loadFile("/db.json"));
  });
server.on("/wifisetup", HTTP_GET, []() {
  server.send(200, "text/html", WIFISETUP_HTML);
});
server.on("/setup", HTTP_GET, []() {
  server.send(200, "text/html", SETUP_HTML);
});

server.on("/statistiche", HTTP_GET, []() {
  DynamicJsonDocument doc(4096);

  JsonArray arr = doc.createNestedArray("utenti");

  uint32_t totale = 0;
  for (auto &u : utenti) totale += u.bevande_servite;

  // ordina per bevande discendente
  std::vector<UtenteStat> sorted = utenti;
  std::sort(sorted.begin(), sorted.end(),
            [](const UtenteStat &a, const UtenteStat &b){
              return a.bevande_servite > b.bevande_servite;
            });

  for (auto &u : sorted) {
    JsonObject o = arr.createNestedObject();
    o["nome"] = u.nome;
    o["bevande"] = u.bevande_servite;
  }

  doc["totale"] = totale;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

server.on("/stats", HTTP_GET, []() {
  server.send(200, "text/html", STAT_HTML);
});

  
  server.on("/config", HTTP_POST, []() {
    String body = server.arg("plain");
    saveFile("/db.json", body);
    parseDB();
    applicaPotenzaWiFi();
    server.send(200, "text/plain", "OK");
  });

  server.on("/setwifi", HTTP_POST, []() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "No data");
    return;
  }

  String body = server.arg("plain");

  // Salva su file o su EEPROM, come preferisci
  saveFile("/wifi.json", body);

  loadWiFiConfig();

  server.send(200, "text/plain", "OK");
});

server.on("/resetutenti", HTTP_GET, []() {
  resetUtenti();
  Serial.println("Reset utenti eseguito da pagina setup");
  server.send(200, "text/plain", "OK");
});

server.on("/resetjson", HTTP_GET, []() {
  creaDBBase();
  Serial.println("Reset utenti eseguito da pagina");
  server.send(200, "text/plain", "OK");
});
server.on("/reboot", HTTP_GET, []() {
  server.send(200, "text/plain", "Riavvio...");
  delay(500);
  ESP.restart();
});
server.on("/wifiinfo", HTTP_GET, []() {
  DynamicJsonDocument doc(256);

  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["local_ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["mode"] = (WiFi.getMode() == WIFI_MODE_AP) ? "AP" :
                (WiFi.getMode() == WIFI_MODE_STA) ? "STA" :
                (WiFi.getMode() == WIFI_MODE_APSTA) ? "AP+STA" : "UNKNOWN";

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

server.on("/getwifi", HTTP_GET, []() {
  DynamicJsonDocument doc(256);
  doc["ssid"] = wifi_ssid;
  doc["password"] = wifi_pass;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

  server.on("/servi", HTTP_GET, []() {
   if (!server.hasArg("ricetta")) {
    server.send(400, "text/plain", "Manca parametro ricetta");
    return;
  }

  int idx = server.arg("ricetta").toInt();

  // CONTROLLO DI SICUREZZA CON LA TUA NUOVA FUNZIONE
  if (!ricettaDisponibile(idx)) {
    server.send(400, "text/plain", "KO: Ingredienti insufficienti!");
    return; // Interrompe l'erogazione prima che si attivino le pompe
  }
  
    eseguiRicetta(idx);

    

    server.send(200, "text/plain", "Servito");
  });

server.on("/utente", HTTP_GET, []() {
  String ip = server.client().remoteIP().toString();

  for (auto &u : utenti) {
    if (u.ip == ip) {
      DynamicJsonDocument doc(256);
      doc["registrato"] = true;
      doc["nome"] = u.nome;
      doc["bevande"] = u.bevande_servite;

      uint32_t tot = 0;
      for (auto &x : utenti) tot += x.bevande_servite;
      doc["totale"] = tot;

      String out;
      serializeJson(doc, out);
      server.send(200, "application/json", out);
      return;
    }
  }

  // ⭐ Caso utente NON registrato
  DynamicJsonDocument doc(64);
  doc["registrato"] = false;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

server.on("/registra", HTTP_POST, []() {
  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);

  UtenteStat u;
  u.nome = doc["nome"].as<String>();
  u.ip = server.client().remoteIP().toString();
  u.bevande_servite = 0;

  utenti.push_back(u);
  saveUtenti();

  server.send(200, "text/plain", "OK");
});





  server.begin();
}



// --------------------------------------------------
// WIFI
// --------------------------------------------------

void setupWiFi() {
  loadWiFiConfig();

  // Feedback visivo: ingresso setup WiFi
  fillStrip(CRGB::Yellow);

  // Avvio sempre AP
  Serial.println("Avvio Access Point...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("BarESP32", "12345678");
  applicaPotenzaWiFi();
  
  Serial.print("AP attivo. IP: ");
  Serial.println(WiFi.softAPIP());

  bool staConnected = false;

  // Se ho SSID valido → provo anche STA
  if (wifi_ssid.length() > 0) {
    Serial.println("Provo connessione WiFi STA...");
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    unsigned long start = millis();
    int ledsSpenti = 0;
    const unsigned long timeoutMs = 8000;
    const unsigned long stepMs = max(1UL, timeoutMs / (unsigned long)NUM_LEDS);
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      int targetSpenti = min(NUM_LEDS, (int)((millis() - start) / stepMs) + 1);
      if (targetSpenti > ledsSpenti) {
        for (int i = ledsSpenti; i < targetSpenti; i++) {
          leds[i] = CRGB::Black;
        }
        ledsSpenti = targetSpenti;
        FastLED_min<LED_PIN>.show();
      }
      delay(200);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      staConnected = true;
      Serial.print("Connesso alla rete locale. IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Connessione STA fallita. Rimango solo in AP.");
    }
  } else {
    Serial.println("SSID non configurato: STA non avviata.");
  }

  if (staConnected) {
    flashStrip(CRGB::Green, 80, 3000);
  } else {
    flashStrip(CRGB::Red, 80, 3000);
  }
}


void setupWiFi__reteFallback() {
  loadWiFiConfig();

  if (wifi_ssid.length() > 0) {
    Serial.println("Provo connessione WiFi STA...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(200);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connesso a rete locale. IP: ");
      Serial.println(WiFi.localIP());
      return;
    }
  }

  // FALLBACK AP
  Serial.println("Avvio Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("BarESP32", "123456");

  Serial.print("Access Point attivo. IP: ");
  Serial.println(WiFi.softAPIP());
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println(F("BARCPU !!"));
esp_bt_controller_disable();
  esp_bt_controller_deinit();
 //setCpuFrequencyMhz(80);
 int numeroPompe = sizeof(pumpPin) / sizeof(pumpPin[0]);
for (int i = 0; i < numeroPompe; i++) {
    pinMode(pumpPin[i], OUTPUT);
    digitalWrite(pumpPin[i], LOW); 
    ultimoUsoPompaMs[i] = 0;
  }

  SPIFFS.begin(true);

 resetUtenti();   // <<< QUI AZZERIAMO IL DB UTENTI
 
  // Se db.json non esiste o è vuoto → crea DB base
  if (!SPIFFS.exists("/db.json")) {
    creaDBBase();
  } else {
    String content = loadFile("/db.json");
    if (content.length() < 10) {
      creaDBBase();
    }
  }
  
  // Se utenti.json non esiste → crea array vuoto
  if (!SPIFFS.exists("/utenti.json")) {
    saveFile("/utenti.json", "[]");
  }

  parseDB();
  loadUtenti();

  // Inizializza LED prima del setup WiFi per mostrare lo stato connessione.
  FASTLED_MIN_SETUP(LED_PIN, leds, NUM_LEDS);
  FastLED_min<LED_PIN>.setBrightness(BRIGHTNESS);
  FastLED_min<LED_PIN>.clear();
  FastLED_min<LED_PIN>.show();

  setupWiFi();
  setupServer();


  delay(100);

RemoteSerial.startTelnet();

  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  FastLED_min<LED_PIN>.setBrightness(BRIGHTNESS);
  FastLED_min<LED_PIN>.clear();            
   FastLED_min<LED_PIN>.show();

   // 5. CONFIGURAZIONE BILANCIA HX711
  scale.begin(HX711_DT, HX711_SCK);
  
  // Esegui la tara all'avvio (assicurati che non ci sia peso sulla bilancia quando accendi)
  scale.set_scale(); // Imposta il fattore di calibrazione di default
  scale.tare();      // Resetta la bilancia a 0 grammi

// Configurazione OTA
  ArduinoOTA.setHostname("esp32c3-ota");
  
  ArduinoOTA.onStart([]() {
    Serial.println("Inizio aggiornamento OTA...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nAggiornamento completato!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Avanzamento: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Errore OTA [%u]: ", error);
  });

  ArduinoOTA.begin();

  setupCaptivePortal();
  Serial.println("Servizio OTA avviato");
  
    Serial.println(F("SitemaON !!!"));
}

void gestioneBottone() {

  if (setupPortataAttiva) {
    int reading = digitalRead(BUTTON_PIN);

    if (reading == LOW && !setupPortataButtonDown) {
      setupPortataButtonDown = true;
      setupPortataPressStartMs = millis();
      if (isPumpIdValido(setupPortataPumpId)) {
        digitalWrite(pumpPin[setupPortataPumpId], HIGH);
        segnaUsoPompa(setupPortataPumpId);
      }
    }

    if (reading == HIGH && setupPortataButtonDown) {
      unsigned long elapsed_ms = millis() - setupPortataPressStartMs;
      setupPortataAccumMs += elapsed_ms;
      setupPortataButtonDown = false;

      if (isPumpIdValido(setupPortataPumpId)) {
        digitalWrite(pumpPin[setupPortataPumpId], LOW);
        segnaUsoPompa(setupPortataPumpId);
      }

      Serial.println("Accumulati");
      Serial.println(setupPortataAccumMs);
    }

    return;
  }

// LEGGI IL STATO DEL PULSANTE
  int reading = digitalRead(BUTTON_PIN);

  // Se il pulsante viene premuto (va a LOW perché è in PULLUP)
  if (reading == LOW && !bottonePremuto) {
     Serial.println(F("Pulsante push!"));
    // Controlla se è passato abbastanza tempo dall'ultima pressione (Debounce)
    if ((millis() - ultimoTempoBottone) > debounceDelay) {
      Serial.println(F("Pulsante premuto! Preparazione cocktail di default..."));
      
      // Accendi i LED di un colore specifico per feedback visivo (es. Blu)
      for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Blue;
      }
       FastLED_min<LED_PIN>.show();
    
      // --- CHIAMATA ALLA TUA FUNZIONE DI EROGAZIONE ---
     bool disp=ricettaDisponibile(parametri.Ric_default);
     if (disp) eseguiRicetta( parametri.Ric_default);

      
        FastLED_min<LED_PIN>.clear();            
        FastLED_min<LED_PIN>.show();
      ultimoTempoBottone = millis();
      bottonePremuto = true;
    }
  }

  // Resetta lo stato quando il pulsante viene rilasciato
  if (reading == HIGH && bottonePremuto) {
    bottonePremuto = false;
  }

  
}
// --------------------------------------------------
// LOOP
// --------------------------------------------------
void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  RemoteSerial.handle();
  dnsServer.processNextRequest();
  gestioneBottone();

  
  // 3. Richiamiamo la funzione respiro passando il colore corrente estratto dall'array
  breatheStrip( BREATHE_DURATION);
}
