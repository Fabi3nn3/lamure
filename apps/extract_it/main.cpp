#include <iostream>
#include "ImageExtractor.h"

int main(int argc, char **argv) {
    std::string input_path  = "/mnt/terabytes_of_textures/montblanc/montblanc_w1202116_h304384.data";
    std::string output_path = "../../apps/extract_it/images/";

    ImageExtractor extractor(input_path, output_path);

    // Extract upper left corner
    std::array<uint64_t, 2> start_at_pixel = {0, 0};
    std::array<uint64_t, 2> end_at_pixel   = {5000, 5000};
    extractor.extract(start_at_pixel, end_at_pixel);

    // extract the 3 leftmost tile columns somewhere in the middle of y axes
    start_at_pixel = {0, 477 * 256};
    end_at_pixel   = {256 * 3, 500 * 256};
    extractor.extract(start_at_pixel, end_at_pixel);

    // extract somewhere in the middle of everything
    start_at_pixel = {591058, 142192};
    end_at_pixel   = {611058, 162192};
    extractor.extract(start_at_pixel, end_at_pixel);

    return 0;
}