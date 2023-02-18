#include <stdint.h>
#include "histogram.h"

// distributes the data in slots across a number of output bins.
// note that each slot may contain a number of hits, which may
// straddle multiple bins.  for instance, suppose that we have
// two HistoSlots:
// count=1000 value=10
// count=200 value=20
// we then want to output into two bins.  we should get:
// count=600 value=600*10=6000 bin=10
// count=600 value=400*10 + 200*20=8000 bin=13.33
//
// note that the computation is destructive to the slots array
void ComputeHistoBins(float *bins, uint32_t numBins, HistoSlot *slots, uint32_t numSlots)
{
    uint32_t totalCount = 0;
    for (uint32_t i=0; i<numSlots; i++)
    {
        totalCount += slots[i].count;
    }

    uint32_t lowerLimit = 0;
    uint32_t upperLimit = 0;
    uint32_t curSlot = 0;
    for (uint32_t i=0; i<numBins; i++)
    {
        lowerLimit = upperLimit;
        upperLimit = (i + 1) * totalCount / numBins;
        uint32_t valueCountOrig = upperLimit - lowerLimit;
        uint32_t valueTotal = 0;

        uint32_t valueCount = valueCountOrig; 
        while (valueCount)
        {
            if (valueCount >= slots[curSlot].count)
            {
                valueTotal += slots[curSlot].value;
                valueCount -= slots[curSlot].count;
                slots[curSlot].count = 0;
                slots[curSlot].value = 0;
                curSlot++;
            }
            else
            {
                uint32_t valuePortion = slots[curSlot].value * valueCount / slots[curSlot].count;
                valueTotal += valuePortion;
                slots[curSlot].count -= valueCount;
                slots[curSlot].value -= valuePortion;
                valueCount = 0;
            }
        }

        bins[i] = float(valueTotal) / valueCountOrig;
    }
}
