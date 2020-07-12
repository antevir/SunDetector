#pragma once

#include <Arduino.h>

enum Color
{
    Off = 0,
    Red = 0x1,
    Green = 0x2,
    Blue = 0x4,
    Yellow = 0x3,
    Magenta = 0x5,
    Cyan = 0x6,
    White = 0x7
};

class RGBLed
{
private:
    int m_redPin;
    int m_greenPin;
    int m_bluePin;
    long m_blinkTs = 0;
    Color m_blinkColor;
    bool m_blinkState;
    int m_blinkOnTimeMs;
    int m_blinkOffTimeMs;
    int m_blinkCount;

protected:
    void setLed(Color c)
    {
        int mask = static_cast<int>(c);
        digitalWrite(m_redPin, (mask & Red) ? HIGH : LOW);
        digitalWrite(m_greenPin, (mask & Green) ? HIGH : LOW);
        digitalWrite(m_bluePin, (mask & Blue) ? HIGH : LOW);
    }

public:
    RGBLed(int redPin, int greenPin, int bluePin) : m_redPin(redPin),
                                                    m_greenPin(greenPin),
                                                    m_bluePin(bluePin)
    {
        pinMode(redPin, OUTPUT);
        pinMode(greenPin, OUTPUT);
        pinMode(bluePin, OUTPUT);
        setLed(Color::Off);
    }

    void startBlink(Color c, int onTimeMs, int offTimeMs, int blinkCount = 0)
    {
        m_blinkTs = millis();
        m_blinkColor = c;
        m_blinkOnTimeMs = onTimeMs;
        m_blinkOffTimeMs = offTimeMs;
        m_blinkState = true;
        m_blinkCount = blinkCount;
        setLed(m_blinkColor);
    }

    void stopBlink()
    {
        m_blinkTs = 0;
        setLed(Color::Off);
    }

    void set(Color c)
    {
        stopBlink();
        setLed(c);
    }

    void handle()
    {
        if (m_blinkTs == 0)
            return;

        int duration = m_blinkState ? m_blinkOnTimeMs : m_blinkOffTimeMs;
        if (millis() - m_blinkTs >= duration)
        {
            m_blinkState = !m_blinkState;
            setLed(m_blinkState ? m_blinkColor : Color::Off);
            m_blinkTs = millis();
            if (m_blinkCount > 0 && m_blinkState)
            {
                if (--m_blinkCount == 0)
                {
                    stopBlink();
                }
            }
        }
    }
};