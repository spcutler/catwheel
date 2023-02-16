#pragma once

struct HistoSlot
{
    uint32_t count;
    uint32_t value;
};

void ComputeHistoBins(float *bins, uint32_t numBins, HistoSlot *slots, uint32_t numSlots);

