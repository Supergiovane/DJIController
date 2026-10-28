# DJIController

Controller per gimbal DJI RS 4 / RS 4 Pro basato su ESP32, che comunica con il gimbal tramite la porta
RSA/NATO usando il protocollo **DJI R SDK** su bus CAN.

## Stato

- [x] Libreria protocollo `DjiRSdk` (framing, CRC, riassemblaggio CAN, comandi), testata sul pacchetto di esempio ufficiale DJI
- [x] Driver CAN per ESP32 (periferica TWAI integrata)
- [x] Console seriale per provare i comandi
- [x] Controllo via Wi-Fi con comandi HTTP
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

1. **Morsetto NATO stampato in 3D con pogo pin**: la soluzione più pulita, ma richiede il rilievo dei contatti.
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
| Password | `djigimbal` |
| Indirizzo | `http://192.168.4.1` |

Per salvare la tua rete: `http://192.168.4.1/api/wifi?ssid=NomeRete&password=Segreto` (oppure da console
`wifi NomeRete Segreto`). L'ESP32 si riavvia e da quel momento risponde anche su `http://djicontroller.local`
(mDNS). `api/wifi?reset=1` dimentica la rete. SSID, password e hostname predefiniti si cambiano in
[platformio.ini](platformio.ini).

### API HTTP

Tutti gli endpoint accettano GET o POST con i parametri nella query string e rispondono in JSON
(`{"ok":true}` oppure `{"ok":false,"error":"..."}`). L'elenco è disponibile anche su `/`.

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
| `/api/wifi?ssid=&password=` | Salva la rete e riavvia (`reset=1` la dimentica) |

Esempi:

```sh
curl "http://djicontroller.local/api/position?yaw=45&pitch=-10&time=2"
curl "http://djicontroller.local/api/joystick?yaw=0.3&ms=2000"
curl "http://djicontroller.local/api/recenter"
```

`speed` e `joystick` si fermano da soli allo scadere di `ms`. Per un movimento "finché tengo premuto" (es. Stream
Deck) basta ripetere la chiamata più spesso di `ms` e chiamare `/api/stop` al rilascio.

> L'API non ha autenticazione: chiunque sia sulla stessa rete può muovere il gimbal. In access point la
> protezione è la password Wi-Fi, quindi cambia quella predefinita.

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
| `wifi SSID PASS` | Salva la rete e riavvia (`wifi reset` la dimentica) |

## Struttura

```
lib/DjiRSdk/      libreria protocollo, senza dipendenze dall'hardware (testabile sul PC)
src/CanBus.*      driver TWAI dell'ESP32
src/Gimbal.*      client R SDK: invio a frammenti, attesa risposte, push
src/MotionHold.*  invio continuo di velocità/joystick con scadenza
src/WifiSetup.*   rete: station con credenziali salvate, oppure access point
src/WebApi.*      API HTTP
src/main.cpp      avvio e console seriale
test/             test Unity (env native)
docs/protocol.md  riassunto del protocollo
```

## Riferimenti

- DJI R SDK Protocol and User Interface v2.3 (supporto: Ronin.SDK@dji.com)
- [DJI-RS-Control](https://github.com/NewDayNaz/DJI-RS-Control): note sul protocollo verificate da RS 2 a RS 5
- [esp32-dji-ronin-controller](https://github.com/yifan-gu/esp32-dji-ronin-controller): ESP32 + SN65HVD230 su RS 4
