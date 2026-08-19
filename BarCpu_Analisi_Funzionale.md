# BarCpu ESP32-C3 - Analisi funzionale rapida

## Scopo del firmware
Controller di un bar automatico su ESP32-C3 con:
- 8 pompe dosatrici per ingredienti
- UI web locale (setup, servizio cocktail, statistiche)
- calibrazione portata pompe con procedura guidata a 100ml (`setup_portata`)
- salvataggio configurazioni su SPIFFS (`/db.json`, `/utenti.json`, `/wifi.json`)
- LED WS2812B come feedback stato
- pulsante fisico per erogazione ricetta di default
- bilancia HX711 (test/diagnostica)
- OTA update (`esp32c3-ota`)
- captive portal DNS su AP locale
- debug seriale + telnet su porta 23

## Hardware mappato
- Pompe: GPIO `0,1,3,4,5,6,7,10`
- LED strip WS2812B: GPIO `2` (`NUM_LEDS=14`)
- Pulsante: GPIO `9` (`INPUT_PULLUP`)
- HX711: `DT=20`, `SCK=21`

## Flusso operativo principale
1. Boot: inizializza GPIO, SPIFFS, DB, WiFi, server HTTP, OTA, DNS captive portal, LED e bilancia.
2. Rete: AP sempre attivo (`BarESP32`), in parallelo tenta STA con credenziali da `/wifi.json`; la potenza massima WiFi viene applicata da `parametri.velocita` tramite `esp_wifi_set_max_tx_power`.
   Durante `setupWiFi()`: strip LED tutta gialla all'avvio, poi nel tentativo STA viene spenta progressivamente (un LED alla volta). Al termine: `flashStrip` verde 3s se STA connessa, altrimenti rosso 3s.
3. UI web: espone pagine e API per listino, setup, pompe, livelli, portata, wifi, statistiche.
4. Erogazione: `/servi?ricetta=N` verifica disponibilita, attiva pompe, aggiorna giacenze e utenti.
5. Loop: gestisce OTA, HTTP, DNS captive, telnet client e pulsante fisico.

## Nota importante per AI e manutenzione
Nel `setup()` viene chiamato `resetUtenti();` ad ogni avvio: questo azzera sempre `/utenti.json` al boot.
Impatto: le statistiche utenti non persistono tra riavvii.

## Endpoint HTTP completi
Base URL tipica:
- AP: `http://192.168.4.1`
- LAN: `http://<ip-locale-esp32>`

### API e pagine applicative
| Metodo | Endpoint | Input | Output | Effetto |
|---|---|---|---|---|
| GET | `/` | - | HTML `INDEX_HTML` | Home principale |
| GET | `/setup` | - | HTML `SETUP_HTML` | Setup JSON, combobox potenza WiFi, reset, link utilita |
| GET | `/wifisetup` | - | HTML `WIFISETUP_HTML` | Setup credenziali WiFi |
| GET | `/setup_pompe` | - | HTML `SETUP_POMPE_HTML` | Test/controllo pompe |
| GET | `/setup_portata` | - | HTML `SETUP_PORTATA_HTML` | Calibrazione portata pompa con bottone hardware |
| GET | `/setup_livelli` | - | HTML `SETUP_LIVELLI_HTML` | Gestione livelli ingredienti |
| GET | `/setup-livelli` | - | HTML `SETUP_LIVELLI_HTML` | Alias endpoint precedente |
| GET | `/listino` | - | HTML `LISTINO_HTML` | Lista cocktail ordinabili |
| GET | `/stats` | - | HTML `STAT_HTML` | Dashboard statistiche |
| GET | `/config` | - | JSON (`/db.json`) | Legge configurazione completa |
| POST | `/config` | body JSON | `OK` | Sovrascrive `/db.json`, normalizza `parametri.velocita`, ricarica DB in RAM e riapplica la potenza WiFi |
| GET | `/get-ingredienti` | - | JSON array `{id,nome,ml}` | Elenco ingredienti con indice reale |
| POST | `/aggiorna-livello` | body JSON `{id,ml}` | `OK` / errore | Aggiorna livello ingrediente e salva DB |
| GET | `/livelli-allerta` | - | JSON array avvisi | Ingredienti finiti/in esaurimento |
| GET | `/listino-dati` | - | JSON `{ingredienti,ricette}` | Dati unificati per UI listino |
| GET | `/pompe` | - | JSON array nomi pompe | Lista pompe disponibili |
| GET | `/pompaOn` | query `id` | `ON` / errore | Attiva pompa indicata |
| GET | `/pompaOff` | query `id` | `OFF` / errore | Disattiva pompa indicata |
| GET | `/pompa5` | query `id` | `OK` / errore | Attiva pompa per 10 secondi |
| GET | `/setup_portata_avvia` | query `id` | JSON esito | Avvia/riavvia la procedura e azzera i ms accumulati |
| GET | `/setup_portata_completa` | - | JSON `{pump,pressed_ms,portata_ml_s,saved,can_save}` | Chiude procedura, applica correzione ms `x0.9`, calcola ml/s su 100ml e prepara il valore per conferma (non salva automaticamente) |
| GET | `/setup_portata_salva` | - | JSON `{pump,portata_ml_s,saved}` | Salva su DB la portata calcolata solo dopo conferma utente |
| GET | `/servi` | query `ricetta` | `Servito` / errore | Esegue ricetta e aggiorna stock/utenti |
| GET | `/utente` | - | JSON stato utente corrente | Dati utente per IP chiamante |
| POST | `/registra` | body JSON `{nome}` | `OK` | Registra utente con IP remoto |
| GET | `/statistiche` | - | JSON classifica + totale | Statistiche aggregate utenti |
| GET | `/resetutenti` | - | `OK` | Azzera utenti e contatori |
| GET | `/resetjson` | - | `OK` | Ricrea DB base (`/db.json`) |
| GET | `/reboot` | - | `Riavvio...` | Riavvia ESP32 |
| GET | `/getwifi` | - | JSON `{ssid,password}` | Legge credenziali salvate |
| POST | `/setwifi` | body JSON `{ssid,password}` | `OK` / errore | Salva `/wifi.json` |
| GET | `/wifiinfo` | - | JSON info rete | Stato AP/STA e IP |
| GET | `/temperatura` | - | testo (float C) | Temperatura interna ESP32-C3 |
| GET | `/uptime` | - | testo `hh:mm` | Tempo ON dall'ultimo avvio, senza storico |
| GET | `/testluci` | - | testo vuoto | Esegue sequenza test LED |
| GET | `/testbilancia` | - | testo vuoto | Routine calibrazione/test HX711 |
| GET | `/pesaBilancia` | - | testo vuoto | Stampa letture HX711 via seriale |
| GET | `/logo` | - | data URI base64 PNG | Logo inline con cache lunga |

### Endpoint captive portal (redirect a AP)
Tutti redirigono a `http://192.168.4.1/` con HTTP 302:
- `/generate_204`
- `/gen_204`
- `/connecttest.txt`
- `/redirect`
- `/hotspot-detect.html`
- `onNotFound` (qualsiasi rotta non gestita)

## Link Altervista richiamati nel codice
Sono presenti 2 URL Altervista usati dalla pagina setup:
1. `https://davidemercanti.altervista.org/baresp32/index.php`
   - usato nel form `POST` per aprire/modificare il JSON nell'editor cloud.
2. `https://davidemercanti.altervista.org/baresp32/salva.php?action=get`
   - usato da `prelevaDaCloud()` (fetch GET) per scaricare configurazione JSON salvata in cloud.

## Parametro `parametri.velocita`
Usato come valore diretto per `esp_wifi_set_max_tx_power` in unita da 0,25 dBm. Valori ammessi dalla combobox setup: interi da `16` a `84`.
Soglia minima effettiva: `16` corrisponde a `4.00 dBm`.
Se il JSON contiene un valore fuori intervallo o non intero, il firmware e la pagina setup lo forzano a `84`, cioe la potenza massima.

## Altri link esterni trovati (non Altervista)
- `https://gitlab.com/Vishal1695/fastled_min` (commento libreria LED)

## Procedura setup_portata (dettaglio)
- Selezione pompa da combo (`0..7`).
- `Avvia`: entra in stato `gestione dosatura`, azzera il contatore ms; se era gia avviata, resetta comunque il conteggio.
- Durante lo stato attivo: ogni pressione del bottone hardware accende la pompa selezionata; al rilascio la spegne e somma i millisecondi reali di pressione.
- Pressioni multiple sono cumulative fino al raggiungimento dei 100ml nel dosatore graduato.
- `Completa`: chiude la procedura, forza OFF la pompa se necessario, corregge il tempo con fattore costante `1.09` (`ms_effettivi = ms_totali * 1.09`) e calcola la portata come `100000 / ms_effettivi` (ml/s).
- Dopo `Completa` la pagina mostra il nuovo valore e chiede conferma all'utente.
- Solo se l'utente conferma, viene chiamato `/setup_portata_salva` che aggiorna `ingredienti[id].p` in `/db.json`.

## Sintesi tecnica breve
Firmware completo per cocktail machine: UI web locale + controllo pompe + persistenza JSON + gestione utenti/statistiche + OTA + captive portal. Le API sono gia sufficienti per integrazione AI (lettura/stato/configurazione/azione), con attenzione al reset utenti automatico in avvio.