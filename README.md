## baresp32


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
