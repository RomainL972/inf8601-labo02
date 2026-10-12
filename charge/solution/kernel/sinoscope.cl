#include "helpers.cl"

typedef struct sinoscope_int_params {
    uint width;
    uint height;
    uint taylor;
    uint interval;
} sinoscope_int_params_t;

__kernel void sinoscope_kernel(__global uchar* buffer, sinoscope_int_params_t ints, float interval_inverse, float time,
                               float max, float phase0, float phase1, float dx, float dy) {
    int global_index = get_global_id(0);

    /* Same traversal as the serial version: `row` indexes the height, `col` the width. */
    int row = global_index / ints.width;
    int col = global_index % ints.width;

    float px    = dx * row - 2 * M_PI;
    float py    = dy * col - 2 * M_PI;
    float value = 0;

    for (int k = 1; k <= (int)ints.taylor; k += 2) {
        value += sin(px * k * phase1 + time) / k;
        value += cos(py * k * phase0) / k;
    }

    value = (atan(value) - atan(-value)) / M_PI;
    value = (value + 1) * 100;

    pixel_t pixel;
    color_value(&pixel, value, ints.interval, interval_inverse);

    int index = global_index * 3;

    buffer[index + 0] = pixel.bytes[0];
    buffer[index + 1] = pixel.bytes[1];
    buffer[index + 2] = pixel.bytes[2];
}
