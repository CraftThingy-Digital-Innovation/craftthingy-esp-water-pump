#pragma once

#include <Arduino.h>

/**
 * @brief Lightweight, zero-dependency Modbus-RTU driver for PZEM-004T v3.0 / v4.0.
 * 
 * Communicates at 9600 baud, 8N1.
 * Memory footprint: < 1KB flash, 0 heap allocations.
 */

struct PZEMData {
    float voltage;     // Volts (e.g. 220.5V)
    float current;     // Amperes (e.g. 1.450A)
    float power;       // Watts (e.g. 320.0W)
    float energy;      // kWh (e.g. 12.450 kWh)
    float frequency;   // Hz (e.g. 50.0Hz)
    float powerFactor; // 0.00 - 1.00
    bool alarm;        // Power alarm flag
    bool valid;        // True if last read was verified with CRC16
};

class PZEM004Tv30 {
public:
    PZEM004Tv30() : _serial(&Serial), _addr(0x01), _lastReadMs(0) {}

    void begin(Stream* serialPort = &Serial, uint8_t addr = 0x01) {
        _serial = serialPort;
        _addr = addr;
    }

    /**
     * @brief Queries PZEM-004T v3.0 registers and validates CRC16.
     * @param data Reference to PZEMData struct to populate.
     * @param timeoutMs Maximum milliseconds to wait for 25-byte response (default: 200ms).
     * @return true if valid frame received and CRC matches, false on timeout/error.
     */
    bool read(PZEMData& data, uint32_t timeoutMs = 200) {
        if (!_serial) return false;

        // Drain any stale bytes in RX buffer
        while (_serial->available()) {
            _serial->read();
        }

        // Build 8-byte Modbus-RTU Read Input Registers command (0x04)
        // Read 10 registers starting at 0x0000:
        // [Addr] [0x04] [0x00] [0x00] [0x00] [0x0A] [CRC_Lo] [CRC_Hi]
        uint8_t cmd[8] = { _addr, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00 };
        uint16_t crc = calculateCRC(cmd, 6);
        cmd[6] = (uint8_t)(crc & 0xFF);        // CRC Low
        cmd[7] = (uint8_t)((crc >> 8) & 0xFF); // CRC High

        _serial->write(cmd, 8);
        _serial->flush();

        // Read 25-byte response:
        // [Addr] [0x04] [ByteCount=0x14] [Reg0..9 = 20 bytes] [CRC_Lo] [CRC_Hi]
        uint8_t response[25];
        uint8_t received = 0;
        uint32_t startMs = millis();

        while ((millis() - startMs) < timeoutMs && received < 25) {
            if (_serial->available()) {
                response[received++] = _serial->read();
            } else {
                delay(2);
            }
        }

        if (received < 25) {
            data.valid = false;
            return false;
        }

        // Validate Modbus header
        if (response[0] != _addr || response[1] != 0x04 || response[2] != 0x14) {
            data.valid = false;
            return false;
        }

        // Verify 16-bit CRC
        uint16_t expectedCrc = calculateCRC(response, 23);
        uint16_t receivedCrc = response[23] | (response[24] << 8);
        if (expectedCrc != receivedCrc) {
            data.valid = false;
            return false;
        }

        // Parse 10 Registers (20 payload bytes, Big-Endian / Modbus register standard)
        // Reg 0: Voltage (0.1V)
        data.voltage = (float)((response[3] << 8) | response[4]) * 0.1f;

        // Reg 1..2: Current (0.001A, 32-bit: Low word in Reg 1, High word in Reg 2)
        uint32_t currentRaw = ((uint32_t)((response[7] << 8) | response[8]) << 16) |
                              ((response[5] << 8) | response[6]);
        data.current = (float)currentRaw * 0.001f;

        // Reg 3..4: Power (0.1W, 32-bit: Low word in Reg 3, High word in Reg 4)
        uint32_t powerRaw = ((uint32_t)((response[11] << 8) | response[12]) << 16) |
                            ((response[9] << 8) | response[10]);
        data.power = (float)powerRaw * 0.1f;

        // Reg 5..6: Energy (1Wh, 32-bit: Low word in Reg 5, High word in Reg 6) -> convert to kWh
        uint32_t energyRaw = ((uint32_t)((response[15] << 8) | response[16]) << 16) |
                             ((response[13] << 8) | response[14]);
        data.energy = (float)energyRaw * 0.001f;

        // Reg 7: Frequency (0.1Hz)
        data.frequency = (float)((response[17] << 8) | response[18]) * 0.1f;

        // Reg 8: Power Factor (0.01)
        data.powerFactor = (float)((response[19] << 8) | response[20]) * 0.01f;

        // Reg 9: Alarm
        data.alarm = (((response[21] << 8) | response[22]) == 0xFFFF);

        data.valid = true;
        _lastReadMs = millis();
        return true;
    }

    static uint16_t calculateCRC(const uint8_t* buffer, uint8_t len) {
        uint16_t crc = 0xFFFF;
        for (uint8_t pos = 0; pos < len; pos++) {
            crc ^= (uint16_t)buffer[pos];
            for (uint8_t i = 8; i != 0; i--) {
                if ((crc & 0x0001) != 0) {
                    crc >>= 1;
                    crc ^= 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

private:
    Stream* _serial;
    uint8_t _addr;
    uint32_t _lastReadMs;
};
