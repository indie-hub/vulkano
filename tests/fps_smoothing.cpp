#include <cassert>

static double smooth_update(double prev_ms, double dt_ms, double factor) {
    return (1.0 - factor) * prev_ms + factor * dt_ms;
}

int main() {
    double s = 16.0; // initial
    const double f = 0.1;
    // Simulate a few frames at 20 ms, then at 10 ms
    for (int i = 0; i < 10; ++i) { s = smooth_update(s, 20.0, f); }
    // Should be close to 20 after several updates
    assert(s > 18.0 && s < 20.1);
    for (int i = 0; i < 10; ++i) { s = smooth_update(s, 10.0, f); }
    // Should have moved towards 10
    assert(s < 12.0);
    return 0;
}

