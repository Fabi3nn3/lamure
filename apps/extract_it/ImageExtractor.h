//
// Created by moqe3167 on 04/12/17.
//

#ifndef LAMURE_IMAGEEXTRACTOR_H
#define LAMURE_IMAGEEXTRACTOR_H

#include <boost/algorithm/string.hpp>
#include <regex>
#include <fstream>

typedef std::array<uint64_t, 2> pixel_pos;

class ImageExtractor {
public:
    ImageExtractor(const std::string &input_path, const std::string &output_path);
    void extract(const pixel_pos &start_pixel, const pixel_pos &end_pixel);

private:
    std::ifstream input_stream;

    uint64_t vt_height;
    uint64_t vt_width;

    std::string input_path;
    std::string output_path;

    void get_vt_dimensions();
    std::string generate_output_file_name(const pixel_pos &start_pixel, const pixel_pos &end_pixel);
    void pixel_validity(const pixel_pos &start, const pixel_pos &end);
};

#endif //LAMURE_IMAGEEXTRACTOR_H
