//
// Created by moqe3167 on 04/12/17.
//

#ifndef LAMURE_IMAGEEXTRACTOR_H
#define LAMURE_IMAGEEXTRACTOR_H

#include <boost/algorithm/string.hpp>
#include <regex>
#include <fstream>

typedef std::array<uint64_t, 2> pixel_pos;

/**
 * ImageExtractor extracts from a given binary image file (virtual texture)
 * a subimage specified by the uper-left and lower-right corners.
 *
 * This class is the opposite to the stitch_it application to check whether
 * the stitched image is correct while the file size exceeds the limits of gimp.
 */
class ImageExtractor {
public:
    ImageExtractor(const std::string &input_path, const std::string &output_path);

    /**
     * Extract subimage data based on the start and end pixel from the given input file.
     * @param start_pixel Coordinates of the upper left corner pixel of the subimage
     * @param end_pixel Coordinates of the lower right corner pixel of the subimage
     */
    void extract(const pixel_pos &start_pixel, const pixel_pos &end_pixel);

private:
    std::ifstream input_stream;

    uint64_t vt_height;
    uint64_t vt_width;

    std::string input_path;
    std::string output_path;

    /**
     * Extracts the information about the virtual texture from the file name.
     */
    void get_vt_dimensions();


    /**
     * Depending on the virtual texture size and the start and end pixel,
     * it creates the output file name.
     * @param start_pixel Coordinates of the upper left corner pixel of the subimage
     * @param end_pixel Coordinates of the lower right corner pixel of the subimage
     * @return output file name
     */
    std::string generate_output_file_name(const pixel_pos &start_pixel, const pixel_pos &end_pixel);


    /**
     * Checks of the start and end pixels are in the bounds of the virtual texture.
     * If not, it throws an InvalidArgument exception.
     * @param start Coordinates of the upper left corner pixel of the subimage
     * @param end Coordinates of the lower right corner pixel of the subimage
     */
    void pixel_validity(const pixel_pos &start, const pixel_pos &end);
};

#endif //LAMURE_IMAGEEXTRACTOR_H
