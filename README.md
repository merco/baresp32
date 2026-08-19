# baresp32

Il dispositivo è in grado di dosare 8 ingredienti distinti. Di ogni pompa deve essere indicata la portata.

Le pompe P3/P4 hanno una portata per liquidi piatti di 8.265 ml/s, le rimanenti di 9.025 ml/s

Il dispositivo fa da access point ( rete: "BarESP32" pwd : 12345678) 

ma può anche collegarsi al WiFi esistente (vedi pagina http://192.168.4.1/wifisetup)



## tecnicamente

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

## Web site locale 192.168.4.1 

- Accesso
  
<img src="https://github.com/merco/baresp32/blob/main/images/Index_0.JPG" width="250" alt="Accesso">

- Pagina principale
  
<img src="https://github.com/merco/baresp32/blob/main/images/Index_1.JPG" width="250" alt="Principale">

- Listino
  
<img src="https://github.com/merco/baresp32/blob/main/images/listino.JPG" width="250" alt="Listino">

- Statistiche
  
<img src="https://github.com/merco/baresp32/blob/main/images/statistiche.JPG" width="250" alt="Listino">


- Setup
  
<img src="https://github.com/merco/baresp32/blob/main/images/setup1.JPG" width="250" alt="Setup1">

<img src="https://github.com/merco/baresp32/blob/main/images/setup2.JPG" width="250" alt="Setup2">

- Rifornimento
  
<img src="https://github.com/merco/baresp32/blob/main/images/setup_ingredienti.JPG" width="250" alt="Refill">




## Editor Web esterno facilitato


<img src="https://github.com/merco/baresp32/blob/main/images/Editor_1.JPG" width="250" alt="Editor">

<img src="https://github.com/merco/baresp32/blob/main/images/Editor_2.JPG" width="250" alt="Editor">



