# ESP32-S3 4G LTE CAT1 Hardware Expert

## Description

Expert knowledge for developing firmware, debugging hardware, and integrating peripherals on the ESP32-S3 4G LTE CAT1 development board featuring:

* ESP32-S3 MCU
* A7670 LTE CAT1 modem
* USB-to-UART bridge
* SIM card interface
* TF (MicroSD) card slot
* Battery charging circuitry
* Power protection circuitry
* LTE status and control GPIOs

Use this skill whenever working with firmware, hardware integration, serial communications, modem control, or troubleshooting on this board.

---

# Core Knowledge

## Architecture

The board consists of two primary subsystems:

### ESP32-S3

Responsible for:

* Application logic
* WiFi
* Bluetooth
* Peripheral control
* Communication with LTE modem

### A7670 LTE CAT1 Modem

Responsible for:

* Cellular registration
* Voice calls
* SMS
* TCP/IP connectivity
* GNSS (if supported by firmware variant)

Communication between the ESP32-S3 and modem occurs through UART using AT commands.

---

# Development Guidelines

## Modem Communication

Always communicate with the A7670 through UART.

Typical workflow:

1. Power modem
2. Wait for boot
3. Verify communication

```text
AT
```

Expected:

```text
OK
```

Check SIM:

```text
AT+CPIN?
```

Check network registration:

```text
AT+CREG?
AT+CEREG?
AT+CGREG?
```

Signal quality:

```text
AT+CSQ
```

Operator:

```text
AT+COPS?
```

---

## Power Management

Before diagnosing modem issues:

Verify:

* Battery voltage
* Main power rail
* Modem power rail
* PWRKEY control signal

Common symptom:

```text
AT commands timeout
```

Possible causes:

* Modem not powered
* Incorrect UART pins
* Incorrect baud rate
* PWRKEY sequence not executed

---

## ESP32-S3 Firmware Patterns

### Recommended Structure

```cpp
setup()
{
    initSerial();
    initModem();
    initStorage();
    initApplication();
}

loop()
{
    handleModem();
    handleNetwork();
    handleApplication();
}
```

Avoid blocking delays.

Prefer:

```cpp
millis()
```

based scheduling.

---

# LTE Modem Control

## Startup Sequence

1. Enable modem power rail
2. Assert PWRKEY
3. Wait for modem boot
4. Wait for UART response
5. Verify SIM
6. Verify registration

### Health Check

```text
AT
AT+CPIN?
AT+CSQ
AT+CEREG?
```

Board should not proceed to application networking until all checks succeed.

---

# Networking

## Recommended Initialization

```text
AT
AT+CPIN?
AT+CSQ
AT+CEREG?
AT+CGATT?
```

Only create sockets after:

```text
+CEREG: registered
```

or

```text
+CGREG: registered
```

status is confirmed.

---

# SMS

Typical flow:

```text
AT+CMGF=1
AT+CMGS="+123456789"
```

Wait for prompt:

```text
>
```

Send message text.

Terminate with:

```text
CTRL+Z
```

---

# Voice Calls

Dial:

```text
ATD<number>;
```

Hang up:

```text
ATH
```

Answer:

```text
ATA
```

---

# SIM Card Troubleshooting

## Symptom

```text
+CPIN: NOT INSERTED
```

Checks:

* SIM orientation
* SIM holder solder joints
* SIM power rail
* SIM detect circuit

## Symptom

```text
+CPIN: READY
```

but no registration.

Checks:

* Antenna connection
* Network availability
* APN settings
* Signal strength

---

# MicroSD Card

Use FAT filesystem.

Recommended initialization order:

1. Mount card
2. Verify capacity
3. Verify read/write
4. Start application logging

Common uses:

* Data logging
* GPS logs
* Diagnostic traces
* Firmware configuration

---

# USB and Debugging

## Debug Console

Use USB or USB-UART bridge for:

* Firmware upload
* Log output
* Modem diagnostics

Always expose:

```cpp
Serial.println()
```

logs for:

* Modem state
* Registration status
* Signal quality
* Socket status

---

# GPIO Usage

When working with board GPIOs:

1. Verify schematic assignment
2. Confirm boot strap requirements
3. Avoid conflicts with:

   * UART
   * USB
   * SD card
   * LTE control signals

Never reassign modem-control pins without reviewing hardware dependencies.

---

# Troubleshooting Checklist

## No AT Response

Check:

* UART wiring
* Baud rate
* Modem power
* PWRKEY sequence

---

## SIM Detected But No Network

Check:

* Antenna
* Signal strength
* Carrier support
* Registration status

Commands:

```text
AT+CSQ
AT+CEREG?
AT+COPS?
```

---

## Data Session Fails

Check:

* APN configuration
* Registration state
* PDP context

---

## ESP32 Reboots Unexpectedly

Check:

* Power rail stability
* Battery condition
* LTE transmit current spikes
* Brownout detector events

---

# Coding Standards

When generating code:

* Prefer ESP-IDF or Arduino-ESP32 APIs.
* Use non-blocking designs.
* Use state machines for modem control.
* Implement modem recovery logic.
* Log all AT command exchanges.
* Retry transient network failures.
* Validate modem state before network operations.

---

# Expected Deliverables

When asked to generate firmware:

1. Include modem initialization.
2. Include error handling.
3. Include reconnection logic.
4. Include diagnostic logging.
5. Include comments explaining LTE interactions.
6. Assume UART-based communication with the A7670 modem.
