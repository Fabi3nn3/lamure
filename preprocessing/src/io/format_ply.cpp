// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/pre/io/format_ply.h>

#include <ply.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>
#include <tuple>
#include <memory>

namespace lamure {
namespace pre {

void FormatPLY::
Read(const std::string& filename, SurfelCallbackFuntion callback)
{
    using namespace std::placeholders;
    typedef std::tuple<std::function<void()>, std::function<void()>> FuncTuple;

    const std::string basename = boost::filesystem::path(filename).stem().string();
    auto begin_point = [&](){ current_surfel_ = Surfel(); };
    auto end_point   = [&](){ 
        callback(current_surfel_); 
    };

    ply::ply_parser ply_parser;

    // define scalar property definition callbacks
    ply::ply_parser::scalar_property_definition_callbacks_type scalar_callbacks;

    using namespace ply;
    at<ply::float32>(scalar_callbacks) = std::bind(&FormatPLY::scalar_callback<ply::float32>, this, _1, _2);
    at<ply::uint8>(scalar_callbacks) = std::bind(&FormatPLY::scalar_callback<ply::uint8>, this, _1, _2);

    // set callbacks
    ply_parser.scalar_property_definition_callbacks(scalar_callbacks);
    ply_parser.element_definition_callback(
        [&](const std::string& element_name, std::size_t count) {
            if (element_name == "vertex")
                return FuncTuple(begin_point, end_point);
            else if(element_name == "face")
	      return FuncTuple(nullptr,nullptr);
            else 
                throw std::runtime_error("FormatPLY::Read() : Invalid element_name!");
    });

    ply_parser.info_callback([&](std::size_t line, const std::string& message) {
			       LOGGER_INFO(basename << " (" << line << "): " << message);
			     });
    ply_parser.warning_callback([&](std::size_t line, const std::string& message) {
				  LOGGER_WARN(basename << " (" << line << "): " << message);
				});
    ply_parser.error_callback([&](std::size_t line, const std::string& message) {
				LOGGER_ERROR(basename << " (" << line << "): " << message);
				throw std::runtime_error("Failed to parse PLY file");
			      });

    // convert
    ply_parser.parse(filename);
}

void FormatPLY::
Write(const std::string& filename, BufferCallbackFuntion callback)
{
    throw std::runtime_error("Not implemented yet!");
}

template <>
std::function<void(float)> FormatPLY::
scalar_callback(const std::string& element_name, const std::string& property_name)
{
    if (element_name == "vertex") {
        if (property_name == "x")
            return [this](float value) { current_surfel_.pos().x = value; };
        else if (property_name == "y")
            return [this](float value) { current_surfel_.pos().y = value; };
        else if (property_name == "z")
            return [this](float value) { current_surfel_.pos().z = value; };
        else if (property_name == "nx")
            return [this](float value) { current_surfel_.normal().x = value; };
        else if (property_name == "ny")
            return [this](float value) { current_surfel_.normal().y = value; };
        else if (property_name == "nz")
            return [this](float value) { current_surfel_.normal().z = value; };
	else
          throw std::runtime_error("FormatPLY::scalar_callback(): Invalid property_name!");
    }
    else
      throw std::runtime_error("FormatPLY::scalar_callback(): Invalid element_name!");
}

template <>
std::function<void(uint8_t)> FormatPLY::
scalar_callback(const std::string& element_name, const std::string& property_name)
{
    if (element_name == "vertex") {
        if (property_name == "red")
            return [this](uint8_t value) { current_surfel_.color().x = value; };
        else if (property_name == "green")
            return [this](uint8_t value) { current_surfel_.color().y = value; };
        else if (property_name == "blue")
            return [this](uint8_t value) { current_surfel_.color().z = value; };
        else if (property_name == "alpha")
	  return [this](uint8_t value) { /*current_surfel_.color().z = value*/; };
        else
          throw std::runtime_error("FormatPLY::scalar_callback(): Invalid property_name!");
    }
    else
      throw std::runtime_error("FormatPLY::scalar_callback(): Invalid element_name!");
}

} // namespace pre
} // namespace lamure