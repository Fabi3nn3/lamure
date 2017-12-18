#include <lamure/vt/ren/VTRenderer.h>


namespace vt
{
VTRenderer::VTRenderer(VTContext *context, uint32_t _width, uint32_t _height, CutUpdate *cut_update)
{
    this->_vtcontext = context;
    this->_cut_update = _cut_update;
    this->_width = _width;
    this->_height = _height;
    this->init();
}

void VTRenderer::init()
{
    _projection_matrix = scm::math::mat4f::identity();

    _scm_core.reset(new scm::core(0, nullptr));

    std::string vs_source;
    std::string fs_source;

    if(!scm::io::read_text_file(std::string(LAMURE_SHADERS_DIR) + "/virtual_texturing.glslv", vs_source) ||
       !scm::io::read_text_file(std::string(LAMURE_SHADERS_DIR) + "/virtual_texturing.glslf", fs_source))
    {
        scm::err() << "error reading shader files" << scm::log::end;
        throw std::runtime_error("Error reading shader files");
    }

    _device.reset(new scm::gl::render_device());
    _render_context = _device->main_context();

    _shader_program =
        _device->create_program(boost::assign::list_of(_device->create_shader(scm::gl::STAGE_VERTEX_SHADER, vs_source))(_device->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, fs_source)));

    if(!_shader_program)
    {
        scm::err() << "error creating shader program" << scm::log::end;
        throw std::runtime_error("Error creating shader program");
    }

    _dstate_less = _device->create_depth_stencil_state(true, true, scm::gl::COMPARISON_LESS);

    // TODO: gua scenegraph to handle geometry eventually
    _obj.reset(new scm::gl::wavefront_obj_geometry(_device, std::string(LAMURE_PRIMITIVES_DIR) + "/quad.obj"));

    _filter_nearest = _device->create_sampler_state(scm::gl::FILTER_MIN_MAG_NEAREST, scm::gl::WRAP_CLAMP_TO_EDGE);
    _filter_linear = _device->create_sampler_state(scm::gl::FILTER_MIN_MAG_LINEAR, scm::gl::WRAP_CLAMP_TO_EDGE);

    _index_texture_dimension = scm::math::vec2ui(_vtcontext->get_size_index_texture(), _vtcontext->get_size_index_texture());
    // TODO: uncomment when physical texture size calculation is complete
    // _physical_texture_dimension = scm::math::vec2ui(_vtcontext->get_size_physical_texture(), _vtcontext->get_size_physical_texture().);
    _physical_texture_dimension = scm::math::vec2ui(7, 3);

    initialize_index_texture();
    initialize_physical_texture();
    initialize_feedback();

    _ms_no_cull = _device->create_rasterizer_state(scm::gl::FILL_SOLID, scm::gl::CULL_NONE, scm::gl::ORIENT_CCW, true);
}

void VTRenderer::render()
{
    _render_context->set_viewport(scm::gl::viewport(scm::math::vec2ui(0, 0), 1 * scm::math::vec2ui(_width, _height)));

    reset_feedback_image();

    scm::math::mat4f view_matrix = _vtcontext->get_event_handler()->get_trackball_manip().transform_matrix();
    scm::math::mat4f model_matrix = scm::math::mat4f::identity();

    model_matrix = scm::math::make_translation(0.0f, 0.0f, -2.0f);
    scm::math::mat4f model_view_matrix = /*view_matrix **/ model_matrix;
    scm::math::mat4f mv_inv_transpose = transpose(inverse(model_view_matrix));

    _shader_program->uniform("projection_matrix", _projection_matrix);
    _shader_program->uniform("model_view_matrix", model_view_matrix);
    _shader_program->uniform("model_view_matrix_inverse_transpose", mv_inv_transpose);

    // upload necessary information to vertex shader
    _shader_program->uniform("in_physical_texture_dim", _physical_texture_dimension);
    _shader_program->uniform("in_index_texture_dim", _index_texture_dimension);
    _shader_program->uniform("in_max_level", (uint32_t)_vtcontext->get_depth_quadtree());
    _shader_program->uniform("in_toggle_view", _vtcontext->get_event_handler()->isToggle_phyiscal_texture_image_viewer());

    _shader_program->uniform("in_tile_size", (uint32_t)_vtcontext->get_size_tile());
    _shader_program->uniform("in_tile_padding", (uint32_t)_vtcontext->get_size_padding());

    _render_context->clear_default_color_buffer(scm::gl::FRAMEBUFFER_BACK, scm::math::vec4f(.6f, .2f, .2f, 1.0f));
    _render_context->clear_default_depth_stencil_buffer();

    _render_context->apply();

    {
        // multi sample pass
        scm::gl::context_state_objects_guard csg(_render_context);
        scm::gl::context_texture_units_guard tug(_render_context);
        scm::gl::context_framebuffer_guard fbg(_render_context);

        _render_context->set_depth_stencil_state(_dstate_less);

        // don't perform backface culling
        _render_context->set_rasterizer_state(_ms_no_cull);

        _render_context->bind_program(_shader_program);

        // bind our texture and tell the graphics card to filter the samples linearly
        // TODO physical texture later with linear filter
        _render_context->bind_texture(_physical_texture, _filter_linear, 0);
        _render_context->bind_texture(_index_texture, _filter_nearest, 1);

        // bind feedback image
        _render_context->bind_image(_feedback_image, scm::gl::FORMAT_R_32UI, scm::gl::ACCESS_READ_WRITE, 2);

        _render_context->bind_storage_buffer(_atomic_feedback_storage_ssbo, 0);

        _render_context->apply();

        _obj->draw(_render_context, scm::gl::geometry::MODE_SOLID);

        //////////////////////////////////////////////////////////////////////////////
        // FEEDBACK STUFF
        //////////////////////////////////////////////////////////////////////////////

        auto data = (char*)_render_context->map_buffer(_atomic_feedback_storage_ssbo, scm::gl::ACCESS_READ_ONLY);

        if(data)
        {
            memcpy(&_copy_memory[0], data, _copy_buffer_size);
        }

        _render_context->unmap_buffer(_atomic_feedback_storage_ssbo);

        _render_context->clear_buffer_data(_atomic_feedback_storage_ssbo, scm::gl::FORMAT_R_32UI, 0);

        // Test prints for feedback
//        for( uint32_t y_feedback_slot_id = 0; y_feedback_slot_id < _physical_texture_dimension.y; ++y_feedback_slot_id ) {
//            for( uint32_t x_feedback_slot_id = 0; x_feedback_slot_id < _physical_texture_dimension.x; ++x_feedback_slot_id ) {
//                uint32_t one_d_feedback_idx = x_feedback_slot_id + _physical_texture_dimension.x * y_feedback_slot_id;
//                std::cout << _copy_memory[one_d_feedback_idx] << " ";
//            }
//            std::cout << "\n";
//        }
//
//        std::cout << "\n";

        // TODO enable when cut update is done
//        _cut_update->feedback(_copy_memory);

    }
}

void VTRenderer::render_feedback()
{
    // TODO
//    _cut_update->feedback();
}

void VTRenderer::initialize_index_texture()
{
    int img_size = _index_texture_dimension.x * _index_texture_dimension.y * 3;
    _index_texture = _device->create_texture_2d(_index_texture_dimension, scm::gl::FORMAT_RGB_8UI);

    // create img_size elements in vector with value 0
    std::vector<uint8_t> cpu_index_buffer(img_size, 0);
    update_index_texture(cpu_index_buffer);
}

void VTRenderer::update_index_texture(std::vector<uint8_t> const &cpu_buffer)
{
    _render_context->update_sub_texture(_index_texture, scm::gl::texture_region(scm::math::vec3ui(0, 0, 0), scm::math::vec3ui(_index_texture_dimension, 1)), 0, scm::gl::FORMAT_RGB_8UI,
                                        &cpu_buffer[0]);
}

void VTRenderer::initialize_physical_texture()
{
    _physical_texture = _device->create_texture_2d(_physical_texture_dimension * _vtcontext->get_size_tile(), scm::gl::FORMAT_RGBA_8);
    physical_texture_test_layout();
}

void VTRenderer::update_physical_texture_blockwise(char *buffer, uint32_t x, uint32_t y)
{
    _render_context->update_sub_texture(_physical_texture,
                                        scm::gl::texture_region(scm::math::vec3ui(x * _vtcontext->get_size_tile(),
                                                                                  y * _vtcontext->get_size_tile(), 0),
                                                                scm::math::vec3ui(_vtcontext->get_size_tile(),
                                                                                  _vtcontext->get_size_tile(), 1)),
                                        0,
                                        scm::gl::FORMAT_RGBA_8, &buffer[0]);
}

void VTRenderer::physical_texture_test_layout() {
    int tilesize = _vtcontext->get_size_tile() * _vtcontext->get_size_tile() * 4;
    std::ifstream is(_vtcontext->get_name_mipmap() + ".data", std::ios::binary);

    if(is)
    {
        auto *buffer = new char[tilesize];
        for(unsigned y = 0; y < _physical_texture_dimension.y; ++y)
        {
            for(unsigned x = 0; x < _physical_texture_dimension.x; ++x)
            {
                is.read(buffer, tilesize);
                update_physical_texture_blockwise(buffer, x, y);
            }
            is.seekg(is.tellg());
        }

        delete[] buffer;
    };
}

void VTRenderer::initialize_feedback()
{
    using namespace scm::gl;
    using namespace scm::math;
    _copy_buffer_size = _physical_texture_dimension.x * _physical_texture_dimension.y * size_of_format(scm::gl::FORMAT_R_32UI);
    _copy_framebuffer = _device->create_frame_buffer();

    _atomic_feedback_storage_ssbo = _device->create_buffer(scm::gl::BIND_STORAGE_BUFFER, scm::gl::USAGE_STREAM_COPY, _copy_buffer_size);

    _feedback_image = _device->create_texture_2d(_physical_texture_dimension, scm::gl::FORMAT_R_32UI);
    reset_feedback_image();

    _copy_framebuffer->attach_color_buffer(0, _feedback_image);
    _copy_memory = std::vector<uint32_t>(_copy_buffer_size/4, 0);


}

void VTRenderer::reset_feedback_image()
{
    int img_size = _physical_texture_dimension.x * _physical_texture_dimension.y;
    std::vector<uint32_t> cpu_feeedback_buffer(img_size, 0);
//         cpu_feeedback_buffer = {0,0,0,1,0,0,0,
//                                 0,0,UINT32_MAX,1,0,0,0,
//                                 0,0,0,1,0,0,0};

    _render_context->update_sub_texture(_feedback_image, scm::gl::texture_region(scm::math::vec3ui(0, 0, 0), scm::math::vec3ui(_physical_texture_dimension, 1)), 0, scm::gl::FORMAT_R_32UI,
                                        &cpu_feeedback_buffer[0]);
}

VTRenderer::~VTRenderer()
{
    _shader_program.reset();
    _index_buffer.reset();
    _vertex_array.reset();

    _obj.reset();

    _filter_nearest.reset();
    _filter_linear.reset();

    _ms_no_cull.reset();

    _render_context.reset();
    _device.reset();
    _scm_core.reset();
}
void VTRenderer::resize(int _width, int _height)
{
    this->_width = static_cast<uint32_t>(_width);
    this->_height = static_cast<uint32_t>(_height);
    _render_context->set_viewport(scm::gl::viewport(scm::math::vec2ui(0, 0), scm::math::vec2ui(this->_width, this->_height)));
    scm::math::perspective_matrix(_projection_matrix, 60.f, float(_width) / float(_height), 0.01f, 1000.0f);
}
}