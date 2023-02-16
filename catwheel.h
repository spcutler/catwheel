#pragma once

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
