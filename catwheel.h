#pragma once

// just a simple LED blinker so I know things are still working
struct Led
{
    uint32_t toggleTime = 0;
    bool state = false;
    void checkToggle()
    {
        uint32_t curTime = millis();
        if ((curTime - toggleTime) > 1000)
        {
            if (state)
            {
                digitalWrite(LED_BUILTIN, LOW);
                state = false;
            }
            else
            {
                digitalWrite(LED_BUILTIN, HIGH);
                state = true;
            }
            toggleTime = curTime;
        }
    }
};

// the following helpers are for wraparound-resistant timing calculations.
// since I hope this code will stay up longer than Windows 95, I wanted
// to avoid the 49.7-day 32-bit millisecond wraparound bug.  

bool millisIsGreater(uint32_t a, uint32_t b)
{
    uint32_t d = a - b;
    return (d < 0x80000000);
}

uint32_t millisGetAverageOrdered(uint32_t M, uint32_t m)
{
    return m + ((M - m) >> 1);
}

uint32_t millisGetAverage(uint32_t a, uint32_t b)
{
    return millisIsGreater(a, b) ? 
        millisGetAverageOrdered(a, b) :
        millisGetAverageOrdered(b, a);
}

uint32_t millisGet75pctile(uint32_t a, uint32_t b)
{
    return millisIsGreater(a, b) ? 
        millisGetAverageOrdered(a, millisGetAverageOrdered(a, b)) :
        millisGetAverageOrdered(b, millisGetAverageOrdered(b, a));
}

uint32_t millisGet75pctileOrdered(uint32_t a, uint32_t b)
{
    return millisGetAverageOrdered(a, millisGetAverageOrdered(a, b));
}
