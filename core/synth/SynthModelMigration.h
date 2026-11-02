#pragma once

// p01/p02 stored KFM at selector index 7 and KIT at index 8. p03 removes KFM
// from the selector; both former custom-engine choices now select the sole KIT.
constexpr int normalizeSynthModelIndex(int savedIndex)
{
    return savedIndex == 8 ? 7 : savedIndex;
}
