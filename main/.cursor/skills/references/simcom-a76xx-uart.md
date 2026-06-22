# SIMCom A76XX UART Communication Expert

## Description

Expert guidance for integrating and troubleshooting SIMCom A76XX series LTE modems using UART communication.

Applies to:

* A7670
* A7600
* A7602
* A7608
* Other A76XX family devices

This skill should be used whenever implementing:

* AT command communication
* ESP32 ↔ A76XX UART interfaces
* Flow control
* Sleep and wake logic
* SMS
* Voice calls
* LTE data connections
* Serial debugging

---

# Critical Hardware Facts

## UART Defaults

Default UART configuration:

* Baud Rate: 115200
* Data Bits: 8
* Stop Bits: 1
* Parity: None

The modem powers up using these defaults unless explicitly changed.

---

## UART Voltage Levels

A76XX UART pins are NOT 3.3V tolerant.

Logic levels:

| Signal            | Voltage      |
| ----------------- | ------------ |
| Logic High Input  | 0.8V – 1.83V |
| Logic High Output | 0.9V – 1.83V |
| Logic Low         | <0.3V        |

Always verify the board design includes proper level translation or compatibility circuitry.

---

# ESP32 Integration

## Minimal Wiring

For most projects only connect:

| ESP32 | A7670 |
| ----- | ----- |
| TX    | RXD   |
| RX    | TXD   |
| GND   | GND   |

This is called Simple Modem mode.

No RTS/CTS required.

Recommended for:

* AT commands
* SMS
* MQTT
* HTTPS
* REST APIs
* General IoT devices

---

## ESP32 UART Initialization

Example:

```cpp
HardwareSerial modem(1);

modem.begin(
    115200,
    SERIAL_8N1,
    MODEM_RX_PIN,
    MODEM_TX_PIN
);
```

Recommended startup test:

```cpp
modem.println("AT");
```

Expected response:

```text
OK
```

---

# Modem Startup Workflow

Always follow:

1. Power modem
2. Execute PWRKEY sequence
3. Wait for boot URCs
4. Send AT
5. Verify SIM
6. Verify signal
7. Verify registration
8. Start application

Example:

```text
AT
AT+CPIN?
AT+CSQ
AT+CEREG?
AT+CGREG?
```

Application networking must not start before registration succeeds.

---

# Flow Control

## Default

Flow control is disabled.

The modem operates without RTS/CTS.

Suitable for:

* Normal AT commands
* MQTT
* HTTPS
* Small payloads

---

## Enable Hardware Flow Control

Use:

```text
AT+IFC=2,2
```

This enables RTS/CTS hardware flow control.

Required for:

* High-speed transfers
* Large file uploads
* Firmware updates
* Continuous data streaming

---

## CTS Behavior

CTS is an INPUT to the modem.

When CTS is LOW:

```text
Modem may transmit.
```

When CTS is HIGH:

```text
Modem pauses transmission.
```

---

## RTS Behavior

RTS is an OUTPUT from the modem.

When RTS goes HIGH:

```text
Modem receive buffer is nearly full.
Stop sending data.
```

When RTS goes LOW:

```text
Safe to continue transmitting.
```

---

# DTR Sleep Control

## Enable Sleep Mode

```text
AT+CSCLK=1
```

DTR becomes sleep control.

Behavior:

| DTR  | Modem |
| ---- | ----- |
| HIGH | Sleep |
| LOW  | Awake |

---

## Disable Sleep Mode

```text
AT+CSCLK=0
```

DTR behaves as a normal GPIO.

---

## Wake Procedure

Before sending AT commands:

1. Pull DTR LOW
2. Wait 20 ms
3. Send AT command

Never transmit immediately after waking the modem.

---

# RI Wake-Up Signal

RI = Ring Indicator

Used by modem to notify host.

---

## Incoming Call

RI goes LOW.

Returns HIGH when:

* Call answered
* Caller hangs up
* ATH executed

---

## Incoming SMS

RI pulse:

```text
LOW for approximately 120 ms
```

Then returns HIGH.

---

## URC Notifications

Examples:

```text
+CMTI
+CREG
+CEREG
```

RI pulse:

```text
LOW for approximately 60 ms
```

Then returns HIGH.

Use RI as an interrupt source for low-power systems.

---

# Auto Baud Detection

## Enable

```text
AT+IPR=0
```

or

```text
AT+IPREX=0
```

---

## Synchronization Procedure

Host sends:

```text
AT
```

Modem replies:

```text
OK
```

If no response:

Send AT again until synchronization succeeds.

---

# Multiplexing

A76XX supports GSM 07.10 multiplexing.

Enable:

```text
AT+CMUX=0
```

Capabilities:

* Four virtual serial channels
* Multiple simultaneous logical sessions

Use only if both host firmware and modem support GSM 07.10 multiplexing.

For ESP32 projects, avoid CMUX unless there is a specific need.

Single UART AT communication is simpler and more reliable.

---

# Common ESP32 Patterns

## Send AT Command

```cpp
modem.println("AT+CSQ");
```

Read until:

```cpp
OK
```

or

```cpp
ERROR
```

---

## Wait For Network

Poll:

```text
AT+CEREG?
```

Valid states:

```text
+CEREG: 1
+CEREG: 5
```

Only continue once registered.

---

## Check SIM

```text
AT+CPIN?
```

Expected:

```text
+CPIN: READY
```

---

# Troubleshooting

## No AT Response

Check:

* RX/TX swapped
* Baud rate mismatch
* Modem not powered
* PWRKEY not asserted
* Wrong UART port

---

## Garbage Characters

Check:

* Incorrect baud rate
* Auto-baud mismatch
* Electrical noise

---

## SMS Not Received

Check:

```text
AT+CPIN?
AT+CSQ
AT+CEREG?
```

Verify network registration.

---

## LTE Registered But No Data

Check:

* APN settings
* PDP context
* Carrier restrictions

---

# Best Practices

Always:

* Use 115200 baud unless higher throughput is required.
* Log every AT command.
* Log every modem response.
* Parse URCs asynchronously.
* Implement modem recovery.
* Handle RI events.
* Use DTR for low-power designs.
* Enable RTS/CTS only when required.
* Wait for registration before opening sockets.

For ESP32 projects, prefer a state-machine-based modem manager rather than blocking AT command sequences.
