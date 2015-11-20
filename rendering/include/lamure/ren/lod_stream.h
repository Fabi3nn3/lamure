// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#ifndef REN_LOD_STREAM_H_
#define REN_LOD_STREAM_H_

#include <fstream>
#include <vector>
#include <string>
#include <cstdio>

#include <lamure/ren/platform.h>
#include <lamure/utils.h>
#include <lamure/ren/config.h>

namespace lamure {
namespace ren
{

class RENDERING_DLL LodStream
{
public:
                        LodStream();
                        LodStream(const LodStream&) = delete;
                        LodStream& operator=(const LodStream&) = delete;
    virtual             ~LodStream();


    void                Open(const std::string& file_name);
    void                OpenForWriting(const std::string& file_name);
    void                Close();
    const bool          is_file_open() const { return is_file_open_; };
    const std::string&  file_name() const { return file_name_; };

    void                Read(char* const data,
                            const size_t start_in_file,
                            const size_t length_in_bytes) const;
                            
    void                Write(char* const data,
                            const size_t start_in_file,
                            const size_t length_in_bytes);

private:
    mutable std::fstream stream_;

    std::string         file_name_;
    bool                is_file_open_;
};

} } // namespace lamure

#endif // REN_LOD_STREAM_H_
