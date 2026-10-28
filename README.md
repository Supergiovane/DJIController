# DJIController

Controller per gimbal DJI RS 4 / RS 4 Pro basato su ESP32, che comunica con il gimbal tramite la porta
RSA/NATO usando il protocollo **DJI R SDK** su bus CAN.

## Stato

- [x] Libreria protocollo `DjiRSdk` (framing, CRC, riassemblaggio CAN, comandi), testata sul pacchetto di esempio ufficiale DJI
- [x] Driver CAN per ESP32 (periferica TWAI integrata)
- [x] Console seriale per provare i comandi
- [x] Controllo via Wi-Fi con comandi HTTP
- [x] Interfaccia web: joystick, preset, collegamento Wi-Fi, backup della configurazione, guida alle API
- [ ] Morsetto NATO stampato in 3D con pogo pin
- [ ] Verifica su RS 4 / RS 4 Pro reale

## Perché ESP32 (e non Arduino Uno/Nano)

L'ESP32 ha un controller CAN integrato (TWAI): basta un transceiver da 3,3 V come l'**SN65HVD230**.
Un Arduino AVR richiederebbe un modulo MCP2515 via SPI, e a 1 Mbit/s serve la versione con quarzo da 16 MHz.
In più l'ESP32 offre Bluetooth/Wi-Fi, utili per un eventuale controllo wireless.

## Hardware

| Componente | Note |
|---|---|
| ESP32 dev board (es. ESP32-DevKitC) | Anche ESP32-S3/C3 vanno bene: hanno tutti TWAI |
| Transceiver CAN SN65HVD230 | Es. modulo Waveshare, già con resistenza di terminazione da 120 Ω |
| Convertitore buck 8 V → 5 V | Per alimentare l'ESP32 dalla porta RSA |
| Resistenza 47 kΩ | Pull-down su AD_COM, senza la quale la porta non eroga corrente |
| Connettore RSA/NATO | Vedi le opzioni sotto |

**Alternativa tutto-in-uno: LilyGO T-CAN485.** È un ESP32 con transceiver CAN (SN65HVD231) a bordo e
ingresso 7–12 V, alimentabile direttamente dagli 8 V della porta RSA senza buck. Il margine sul minimo è però
ridotto (circa 0,6 V). Si compila con `pio run -e tcan485`: CAN su GPIO27 (TX) e GPIO26 (RX), mentre GPIO23
viene tenuto basso per la modalità ad alta velocità del transceiver.

### Pinout della porta RSA/NATO

Fonte: *DJI R SDK Protocol and User Interface v2.3*, §3.1.2. Il documento descrive l'RS 2; DJI dichiara il
supporto del protocollo anche su RS 3 Pro, RS 4 e RS 4 Pro, ma **verifica i pin con un multimetro prima di collegare**.

| Pin | Segnale | Note |
|---|---|---|
| 1 | VCC | Uscita **8 V ± 0,4 V**, 0,8 A nominali (1,2 A di picco). **Non collegarla direttamente all'ESP32** |
| 2 | CANL | |
| 3 | SBUS_RX | Non usato |
| 4 | CANH | |
| 5 | AD_COM | Rilevamento accessorio: serve un pull-down da 10–100 kΩ verso GND, altrimenti la porta non eroga alimentazione |
| 6 | GND | |

### Collegamenti

```
Porta RSA                  SN65HVD230              ESP32
 pin 4 CANH  ───────────── CANH
 pin 2 CANL  ───────────── CANL
                           CTX  ─────────────────── GPIO5  (CAN_TX_PIN)
                           CRX  ─────────────────── GPIO4  (CAN_RX_PIN)
                           3V3  ─────────────────── 3V3
 pin 6 GND   ──────┬────── GND  ─────────────────── GND
 pin 5 AD_COM ─[47k]┘
 pin 1 VCC 8V ──── buck 8V→5V ───────────────────── 5V / VIN
```

Il gimbal non è terminato: la resistenza da 120 Ω va solo sul nostro lato (sul modulo Waveshare è già presente).
I pin CAN si cambiano in [platformio.ini](platformio.ini).

### Come collegarsi fisicamente alla porta RSA

La porta RSA ha contatti a molla nel morsetto NATO. Le strade possibili:

1. **Morsetto NATO stampato in 3D con pogo pin** (scelta del progetto): modello, STL e istruzioni in
   [hardware/](hardware/README.md). La porta dell'RS 4 ha 8 contatti, non 6: la mappatura va verificata.
2. **Ricevitore/accessorio RSA di terze parti** (es. ricevitore cablato Tilta) da cui prelevare i segnali: è la strada
   seguita da [esp32-dji-ronin-controller](https://github.com/yifan-gu/esp32-dji-ronin-controller) su RS 4.
3. **DJI Focus Wheel** montata sulla porta RSA: espone un connettore a 4 pin (VCC_5V, GND, CANH, CANL) con
   alimentazione a 5 V. L'interruttore dell'accessorio va su `CAN`, non su `S-BUS`.

## Software

Serve [PlatformIO](https://platformio.org/) (CLI o estensione VS Code).

```sh
pio test -e native        # test della libreria protocollo sul computer
pio run -e esp32dev       # compila il firmware
pio run -e esp32dev -t upload -t monitor
```

### Rete Wi-Fi

All'avvio l'ESP32 prova a collegarsi alla rete salvata in flash. Se non ce n'è una, o se non la raggiunge entro
10 s, crea un proprio access point:

| | |
|---|---|
| SSID | `DJIController` |
| Password | nessuna (rete aperta) |
| Indirizzo | `http://192.168.4.1` |

Per collegarlo alla tua rete usa la scheda **Impostazioni** dell'interfaccia web (oppure da console
`wifi NomeRete Segreto`). L'ESP32 prova la rete tenendo attivo il proprio access point e la salva solo se la
connessione riesce; dal riavvio successivo risponde su `http://djicontroller.local` (mDNS) o sul suo IP.
SSID, hostname e un'eventuale password dell'access point (almeno 8 caratteri) si impostano in
[platformio.ini](platformio.ini).

### Interfaccia web

Apri l'indirizzo del controller nel browser (`http://192.168.4.1` in access point, altrimenti
`http://djicontroller.local`). La pagina è servita dall'ESP32 e funziona senza internet. Ha quattro schede:

| Scheda | Contenuto |
|---|---|
| Controllo | Joystick virtuale pan/tilt (joystick nativo o velocità), recenter, stop, modalità follow, camera, posizione esatta |
| Preset | Salva la posizione attuale con un nome, richiamala con un tempo di transizione, aggiornala o eliminala |
| Impostazioni | Ricerca reti e collegamento Wi-Fi, backup della configurazione (esporta e importa), "dimentica rete" |
| API | Guida agli endpoint con esempi copiabili e prova dal vivo di quelli in sola lettura |

I preset sono salvati sull'ESP32, quindi sono gli stessi da qualsiasi dispositivo. Le preferenze del joystick
(modalità, velocità, inversioni) restano nel browser.

Il **backup** è un file JSON con preset, rete salvata e preferenze della pagina. La password Wi-Fi è inclusa
solo se selezioni l'apposita casella; se manca, la pagina la chiede durante l'importazione. All'importazione i preset
sostituiscono quelli attuali e il controller si collega subito alla rete del file. Se in quel momento è collegato a
un'altra rete, la nuova viene salvata solo se il collegamento riesce, così il controller resta raggiungibile.

La pagina è in [web/index.html](web/index.html): a ogni build viene compressa e incorporata nel firmware da
[scripts/embed_web.py](scripts/embed_web.py).

### API HTTP

Tutti gli endpoint accettano GET o POST con i parametri nella query string e rispondono in JSON
(`{"ok":true}` oppure `{"ok":false,"error":"..."}`). L'elenco testuale è su `/api`, la guida interattiva nella
scheda API dell'interfaccia.

| Endpoint | Effetto |
|---|---|
| `/api/status` | Rete, movimento in corso, angoli e stato della connessione con il gimbal |
| `/api/angles?type=attitude\|joint` | Angoli correnti in gradi |
| `/api/version` | Versione del modulo R SDK |
| `/api/position?yaw=&roll=&pitch=&time=1&mode=abs\|rel` | Va alla posizione in `time` secondi; gli assi omessi restano fermi |
| `/api/speed?yaw=&roll=&pitch=&ms=500` | Ruota in °/s per `ms` millisecondi (da 50 a 60000) |
| `/api/joystick?pitch=&roll=&yaw=&ms=500` | Ingresso joystick nativo, -1..1 per asse |
| `/api/stop` | Ferma velocità/joystick |
| `/api/recenter` | Recenter |
| `/api/follow?mode=lock\|yaw\|sport` | Modalità follow |
| `/api/record?on=1\|0` | Avvia o ferma la registrazione (serve il cavo di controllo camera) |
| `/api/shutter` | Scatto |
| `/api/activetrack` | Attiva o disattiva ActiveTrack |
| `/api/presets` | Elenco dei preset |
| `/api/presets/save?name=[&slot=][&yaw=&roll=&pitch=]` | Salva la posizione attuale (o gli angoli dati) come preset |
| `/api/presets/recall?slot=&time=2` | Va al preset in `time` secondi |
| `/api/presets/delete?slot=` | Elimina un preset |
| `/api/wifi/status` | Access point, rete collegata, rete salvata, esito del collegamento |
| `/api/wifi/scan[?refresh=1]` | Reti vicine (`{"scanning":true}` finché la ricerca non finisce) |
| `/api/wifi/join?ssid=&password=` | Prova una rete e la salva solo se il collegamento riesce |
| `/api/wifi/forget` | Dimentica la rete salvata e riavvia |
| `/api/config[?secrets=1]` | Esporta la configurazione (password Wi-Fi solo con `secrets=1`) |
| `POST /api/config` | Importa una configurazione (corpo JSON) |

Esempi:

```sh
curl "http://djicontroller.local/api/position?yaw=45&pitch=-10&time=2"
curl "http://djicontroller.local/api/joystick?yaw=0.3&ms=2000"
curl "http://djicontroller.local/api/recenter"
```

`speed` e `joystick` si fermano da soli allo scadere di `ms`. Per un movimento "finché tengo premuto" (es. Stream
Deck) basta ripetere la chiamata più spesso di `ms` e chiamare `/api/stop` al rilascio.

> L'API non ha autenticazione e l'access point è aperto: chiunque sia nel raggio del Wi-Fi può collegarsi e
> muovere il gimbal. Se serve, imposta `WIFI_AP_PASSWORD` in [platformio.ini](platformio.ini).

### Console seriale (115200 baud)

| Comando | Effetto |
|---|---|
| `v` | Versione del modulo R SDK (buon primo test della comunicazione) |
| `a` | Legge l'assetto |
| `c` | Recenter |
| `p Y R P [sec]` | Va a yaw/roll/pitch assoluti in gradi |
| `m Y R P [sec]` | Movimento relativo |
| `s Y R P` | Velocità continua in °/s (inviata di nuovo ogni 100 ms) |
| `j P R Y` | Joystick nativo (-1..1), usa la velocità e lo smoothing impostati sul gimbal |
| `x` | Ferma velocità/joystick |
| `push 1\|0` | Attiva o disattiva l'invio periodico degli angoli |
| `rec 1\|0` | Avvia o ferma la registrazione (serve il cavo di controllo camera) |
| `ip` | Mostra la rete |
| `wifi SSID PASS` | Prova una rete e la salva se il collegamento riesce (`wifi reset` la dimentica e riavvia) |
| `sniff [raw]` | Sniffer in sola ricezione su tutti gli ID CAN, per analizzare un accessorio esistente (`sniff off` per uscire) |

## Struttura

```
lib/DjiRSdk/      libreria protocollo, senza dipendenze dall'hardware (testabile sul PC)
src/CanBus.*      driver TWAI dell'ESP32
src/Gimbal.*      client R SDK: invio a frammenti, attesa risposte, push
src/MotionHold.*  invio continuo di velocità/joystick con scadenza
src/WifiSetup.*   rete: station con credenziali salvate oppure access point, ricerca e collegamento
src/PresetStore.* preset salvati in flash
src/WebApi.*      interfaccia web e API HTTP
src/main.cpp      avvio e console seriale
web/index.html    interfaccia web (incorporata nel firmware a ogni build)
scripts/          script di build
test/             test Unity (env native)
hardware/         morsetto NATO stampabile in 3D (modello parametrico Python + STL)
docs/protocol.md  riassunto del protocollo
```

## Riferimenti

- DJI R SDK Protocol and User Interface v2.3 (supporto: Ronin.SDK@dji.com)
- [DJI-RS-Control](https://github.com/NewDayNaz/DJI-RS-Control): note sul protocollo verificate da RS 2 a RS 5
- [esp32-dji-ronin-controller](https://github.com/yifan-gu/esp32-dji-ronin-controller): ESP32 + SN65HVD230 su RS 4
