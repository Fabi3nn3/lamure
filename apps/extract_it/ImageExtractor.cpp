//
// Created by moqe3167 on 04/12/17.
//

#include "ImageExtractor.h"

ImageExtractor::ImageExtractor(const std::string &input_path,
                               const std::string &output_path) :
        input_path(input_path),
        output_path(output_path) {
    input_stream.open(input_path, std::ios::in | std::ios::binary);
    get_vt_dimensions();
}

void ImageExtractor::extract(const pixel_pos &start, const pixel_pos &end) {
    pixel_validity(start, end);


    std::string output_file_name = generate_output_file_name(start, end);

    std::ofstream output_stream;
    output_stream.open(output_file_name, std::ios::out | std::ios::binary);

    uint64_t start_pos          = (vt_width * start[1] + start[0]) * 4;
    uint64_t bytes_per_read_row = (end[0] - start[0]) * 4;

    input_stream.seekg(start_pos);

    auto* buffer = new char[bytes_per_read_row];
    if(input_stream) {
        for (int i = 0; i < end[1] - start[1]; ++i) {
            input_stream.read(buffer, bytes_per_read_row);
            output_stream.write(buffer, bytes_per_read_row);
            input_stream.seekg(input_stream.tellg() + (vt_width * 4 - bytes_per_read_row));
        }
    }
}

void ImageExtractor::get_vt_dimensions() {
    std::vector<std::string> strings;
    std::string file_name = input_path.substr(input_path.find_last_of('/') + 1);
    file_name = file_name.substr(0, file_name.find_first_of('.'));

    boost::split(strings, file_name, boost::is_any_of("_"));

    std::regex width_pattern{"(w)([[:digit:]]+)"};
    std::regex height_pattern{"(h)([[:digit:]]+)"};

    for(auto const& string : strings) {
        if(std::regex_match(string, width_pattern)) {
            std::string width_string = string.substr(1);
            vt_width = atoi(width_string.c_str());
        } else if(std::regex_match(string, height_pattern)) {
            std::string height_string = string.substr(1);
            vt_height = atoi(height_string.c_str());
        }
    }
}

std::string ImageExtractor::generate_output_file_name(const pixel_pos &start_pixel, const pixel_pos &end_pixel) {
    std::string output_file_name = output_path + "subimage";
    output_file_name +=   "_p_" + std::to_string(start_pixel[0]) + "_" + std::to_string(start_pixel[1])
                        + "_q_" + std::to_string(end_pixel[0])   + "_" + std::to_string(end_pixel[1])
                        + "_w"  + std::to_string(end_pixel[0] - start_pixel[0])
                        + "_h"  + std::to_string(end_pixel[1] - start_pixel[1])
                        + ".data";
    return output_file_name;
}

void ImageExtractor::pixel_validity(const pixel_pos &begin, const pixel_pos &end) {
    if(begin[0] < 0 || begin[0] > vt_width || begin[1] < 0 || begin[1] > vt_height ||
            end[0] < 0 || end[0] > vt_width || end[1] < 0 || end[1] > vt_height) {
        throw std::invalid_argument("Given pixels are out of image bounds.");
    }
}
