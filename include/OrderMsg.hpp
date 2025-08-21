#pragma once
#include "MatchingEngine.hpp" // defines OrderIn, SIDE_BUY/SELL
// Keep the worker/producers independent of engine internals if you want;
// here we reuse OrderIn directly for simplicity.
using OrderMsg = OrderIn;
