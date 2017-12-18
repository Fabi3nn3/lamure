#include <lamure/vt/VTContext.h>
#include <lamure/vt/ren/VTRenderer.h>
#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
#include <math.h>
namespace vt
{
VTContext::VTContext() { _config = new CSimpleIniA(true, false, false); }

uint16_t VTContext::get_size_tile() const { return _size_tile; }
const std::string &VTContext::get_name_texture() const { return _name_texture; }
const std::string &VTContext::get_name_mipmap() const { return _name_mipmap; }
bool VTContext::is_opt_run_in_parallel() const { return _opt_run_in_parallel; }
bool VTContext::is_opt_row_in_core() const { return _opt_row_in_core; }
VTContext::Config::FORMAT_TEXTURE VTContext::get_format_texture() const { return _format_texture; }
bool VTContext::is_keep_intermediate_data() const { return _keep_intermediate_data; }
bool VTContext::is_verbose() const { return _verbose; }

uint16_t VTContext::get_byte_stride() const
{
    uint8_t _byte_stride = 0;
    switch(_format_texture)
    {
    case Config::FORMAT_TEXTURE::RGBA8:
        _byte_stride = 4;
        break;
    case Config::FORMAT_TEXTURE::RGB8:
        _byte_stride = 3;
        break;
    case Config::FORMAT_TEXTURE::R8:
        _byte_stride = 1;
        break;
    }

    return _byte_stride;
}

void VTContext::start()
{
    if(access((_name_mipmap + ".data").c_str(), F_OK) != -1)
    {
        std::runtime_error("Mipmap file not found: " + _name_mipmap);
    }

    _depth_quadtree = identify_depth();
    _size_index_texture = identify_size_index_texture();
    _size_physical_texture = get_size_physical_texture();
    glfwSetErrorCallback(&VTContext::EventHandler::on_error);

    if(!glfwInit())
    {
        std::runtime_error("GLFW initialisation failed");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    _window = glfwCreateWindow(mode->width, mode->height, "Virtual Texturing", glfwGetPrimaryMonitor(), nullptr);

    if(!_window)
    {
        glfwTerminate();
        std::runtime_error("GLFW window creation failed");
    }

    glfwMakeContextCurrent(_window);

    glfwSetWindowUserPointer(_window, this);
    glfwSetWindowSizeCallback(_window, &VTContext::EventHandler::on_window_resize);

    glfwSwapInterval(1);

    glfwSetKeyCallback(_window, &VTContext::EventHandler::on_window_key_press);
    glfwSetCharCallback(_window, &VTContext::EventHandler::on_window_char);
    glfwSetMouseButtonCallback(_window, &VTContext::EventHandler::on_window_button_press);
    glfwSetCursorPosCallback(_window, &VTContext::EventHandler::on_window_move_cursor);
    glfwSetScrollCallback(_window, &VTContext::EventHandler::on_window_scroll);
    glfwSetCursorEnterCallback(_window, &VTContext::EventHandler::on_window_enter);

    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    //_cut_update = new CutUpdate();
    //_cut_update->start();

    _vtrenderer = new VTRenderer(this, (uint32_t)mode->width, (uint32_t)mode->height, _cut_update);

    glewInit();

    while(!glfwWindowShouldClose(_window))
    {
        // std::cout << "0" << std::endl;

        _vtrenderer->render();

        // std::cout << "1" << std::endl;

        //_vtrenderer->render_feedback();

        // std::cout << "2" << std::endl;

        glfwSwapBuffers(_window);
        glfwPollEvents();

        // std::cout << "3" << std::endl;
    }

    //_cut_update->stop();

    glfwDestroyWindow(_window);
    glfwTerminate();
}
uint16_t VTContext::get_depth_quadtree() const { return _depth_quadtree; }
uint16_t VTContext::identify_depth()
{
    size_t fsize = 0;
    std::ifstream file;

    file.open(_name_mipmap + ".data", std::ifstream::in | std::ifstream::binary);

    if(!file.is_open())
    {
        throw std::runtime_error("Mipmap could not be read");
    }

    fsize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::end);
    fsize = static_cast<size_t>(file.tellg()) - fsize;

    file.close();

    auto texel_count = static_cast<uint32_t>((size_t)fsize / get_byte_stride());

    size_t count_tiled = texel_count / _size_tile / _size_tile;

    size_t depth = QuadTree::get_depth_of_node(count_tiled - 1);

    return static_cast<uint16_t>(depth);
}

uint32_t VTContext::identify_size_index_texture() { return (uint32_t)std::pow(2, identify_depth()); }

scm::math::vec2ui VTContext::calculate_size_physical_texture()
{
    //TODO: wie würde Brute Force aussehen? berechnung effizienter machen
    //only nvidia support
    GLint nTotalMemoryInKB = 0;
    glGetIntegerv( GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX,
                   &nTotalMemoryInKB );

    GLint nCurAvailMemoryInKB = 0;
    glGetIntegerv( GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX,
                   &nCurAvailMemoryInKB );

    GLint nMaxTextureSize = 0;
    glGetIntegerv( GL_MAX_TEXTURE_SIZE,
                   &nMaxTextureSize );

    GLint nMaxTextureArrayLayers = 0;
    glGetIntegerv( GL_MAX_ARRAY_TEXTURE_LAYERS,
                   &nMaxTextureArrayLayers );

    //std::cout << "MAX TEXTURE_SIZE: " << nMaxTextureSize << std::endl;
    //std::cout << "MAX_TEXTURE_LAYERS: " << nMaxTextureArrayLayers << std::endl;

    std::cout << "total gpu memory: " << nTotalMemoryInKB << " KB   currently available gpu memory: " << nCurAvailMemoryInKB <<" KB "<< std::endl;

    uint64_t max_tiles_per_dim = (nMaxTextureSize * nMaxTextureSize) / (1024 * 1024 * 4);
    std::cout << "Max Tiles/Dim: " << max_tiles_per_dim << std::endl;

    uint64_t input_in_byte = (uint64_t)_size_physical_texture * 1024 * 1024;
    uint64_t tilesize = _size_tile*_size_tile*4;
    uint64_t total_amount_of_tiles = input_in_byte / tilesize;
    uint64_t tiles_per_dim_x = floor(sqrt(total_amount_of_tiles));
    uint64_t tiles_per_dim_y = total_amount_of_tiles / tiles_per_dim_x;
    std::cout << tiles_per_dim_x << " x " << tiles_per_dim_y << std::endl;

    if(tiles_per_dim_x && tiles_per_dim_y <= max_tiles_per_dim){
        return scm::math::vec3ui(tiles_per_dim_x, tiles_per_dim_y, 1);
    }
    else{
        _physical_texture_layers = ceil((float)(tiles_per_dim_x * tiles_per_dim_y) / (float)(max_tiles_per_dim * max_tiles_per_dim));
        if(_physical_texture_layers > nMaxTextureArrayLayers){
            std::cout << "Too many layers" << std::endl;
        }
        return scm::math::vec2ui(max_tiles_per_dim, max_tiles_per_dim);
    }

}

bool VTContext::EventHandler::isToggle_phyiscal_texture_image_viewer() const { return toggle_phyiscal_texture_image_viewer; }
uint32_t VTContext::get_size_index_texture() const { return _size_index_texture; }
uint32_t VTContext::get_size_physical_texture() const { return _size_physical_texture; }
VTRenderer *VTContext::get_vtrenderer() const { return _vtrenderer; }
VTContext::EventHandler *VTContext::get_event_handler() const { return _event_handler; }
void VTContext::set_event_handler(VTContext::EventHandler *_event_handler) { VTContext::_event_handler = _event_handler; }

void VTContext::EventHandler::on_error(int _err_code, const char *_err_msg) { throw std::runtime_error(_err_msg); }
void VTContext::EventHandler::on_window_resize(GLFWwindow *_window, int _width, int _height)
{
    auto _vtcontext(static_cast<VTContext *>(glfwGetWindowUserPointer(_window)));

    _vtcontext->_event_handler->_ref_width = _width;
    _vtcontext->_event_handler->_ref_height = _height;

    _vtcontext->get_vtrenderer()->resize(_width, _height);
}
void VTContext::EventHandler::on_window_key_press(GLFWwindow *_window, int _key, int _scancode, int _action, int _mods)
{
    if(!_action == GLFW_PRESS)
    {
        return;
    }

    auto _vtcontext(static_cast<VTContext *>(glfwGetWindowUserPointer(_window)));

    std::vector<uint8_t> cpu_idx_texture_buffer_state;
    switch(_key)
    {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(_window, 1);
        break;
    case GLFW_KEY_SPACE:
        _vtcontext->_event_handler->toggle_phyiscal_texture_image_viewer = !_vtcontext->_event_handler->toggle_phyiscal_texture_image_viewer;
        break;
    case GLFW_KEY_0:
        cpu_idx_texture_buffer_state = std::vector<uint8_t>(16 * 3, 0);
        _vtcontext->_vtrenderer->update_index_texture(cpu_idx_texture_buffer_state);
        break;
    case GLFW_KEY_1:
        cpu_idx_texture_buffer_state = {1, 0, 1, 0,
                                        1, 0, 1, 0,
                                        2, 0, 1, 0,
                                        2, 0, 1, 0,
                                        1, 0, 1, 0,
                                        1, 0, 1, 0,
                                        2, 0, 1, 0,
                                        2, 0, 1, 0,
                                        3, 0, 1, 0,
                                        3, 0, 1, 0,
                                        4, 0, 1, 0,
                                        4, 0, 1, 0,
                                        3, 0, 1, 0,
                                        3, 0, 1, 0,
                                        4, 0, 1, 0,
                                        4, 0, 1, 0};
        _vtcontext->_vtrenderer->update_index_texture(cpu_idx_texture_buffer_state);
        break;
    case GLFW_KEY_2:
        cpu_idx_texture_buffer_state = {5, 0, 2, 0,
                                        6, 0, 2, 0,
                                        2, 1, 2, 0,
                                        3, 1, 2, 0,
                                        0, 1, 2, 0,
                                        1, 1, 2, 0,
                                        4, 1, 2, 0,
                                        5, 1, 2, 0,
                                        6, 1, 2, 0,
                                        0, 2, 2, 0,
                                        3, 2, 2, 0,
                                        4, 2, 2, 0,
                                        1, 2, 2, 0,
                                        2, 2, 2, 0,
                                        5, 2, 2, 0,
                                        6, 2, 2, 0};
        _vtcontext->_vtrenderer->update_index_texture(cpu_idx_texture_buffer_state);
        break;
    case GLFW_KEY_3:
        cpu_idx_texture_buffer_state = {1, 0, 1, 0,
                                        1, 0, 1, 0,
                                        2, 0, 1, 0,
                                        2, 0, 1, 0,
                                        1, 0, 1, 0,
                                        1, 0, 1, 0,
                                        2, 0, 1, 0,
                                        2, 0, 1, 0,
                                        3, 0, 1, 0,
                                        3, 0, 1, 0,
                                        17, 0, 2, 0,
                                        18, 0, 2, 0,
                                        3, 0, 1, 0,
                                        3, 0, 1, 0,
                                        19, 0, 2, 0,
                                        20, 0, 2, 0};
        _vtcontext->_vtrenderer->update_index_texture(cpu_idx_texture_buffer_state);
        break;
    }
}
void VTContext::EventHandler::on_window_char(GLFWwindow *_window, unsigned int _codepoint)
{
    // TODO
}
void VTContext::EventHandler::on_window_button_press(GLFWwindow *_window, int _button, int _action, int _mods)
{
    auto _vtcontext(static_cast<VTContext *>(glfwGetWindowUserPointer(_window)));

    if(_button == GLFW_MOUSE_BUTTON_LEFT && _action == GLFW_PRESS)
    {
        _vtcontext->get_event_handler()->_mouse_button_state = MouseButtonState::LEFT;
    }
    else if(_button == GLFW_MOUSE_BUTTON_MIDDLE && _action == GLFW_PRESS)
    {
        _vtcontext->get_event_handler()->_mouse_button_state = MouseButtonState::WHEEL;
    }
    else if(_button == GLFW_MOUSE_BUTTON_RIGHT && _action == GLFW_PRESS)
    {
        _vtcontext->get_event_handler()->_mouse_button_state = MouseButtonState::RIGHT;
    }
    else
    {
        _vtcontext->get_event_handler()->_mouse_button_state = MouseButtonState::IDLE;
    }
}
void VTContext::EventHandler::on_window_move_cursor(GLFWwindow *_window, double _xpos, double _ypos)
{
    auto _vtcontext(static_cast<VTContext *>(glfwGetWindowUserPointer(_window)));

    EventHandler *_event_handler = _vtcontext->get_event_handler();

    _event_handler->_initx = 2.f * float(_xpos - (_event_handler->_ref_width / 2)) / float(_event_handler->_ref_width);
    _event_handler->_inity = 2.f * float(_event_handler->_ref_height - _ypos - (_event_handler->_ref_height / 2)) / float(_event_handler->_ref_height);
}
void VTContext::EventHandler::on_window_scroll(GLFWwindow *_window, double _xoffset, double _yoffset)
{
    // TODO
}
void VTContext::EventHandler::on_window_enter(GLFWwindow *_window, int _entered)
{
    // TODO
}
const scm::gl::trackball_manipulator &VTContext::EventHandler::get_trackball_manip() const { return _trackball_manip; }
void VTContext::EventHandler::set_trackball_manip(const scm::gl::trackball_manipulator &_trackball_manip) { this->_trackball_manip = _trackball_manip; }
float VTContext::EventHandler::get_initx() const { return _initx; }
void VTContext::EventHandler::set_initx(float _initx) { this->_initx = _initx; }
float VTContext::EventHandler::get_inity() const { return _inity; }
void VTContext::EventHandler::set_inity(float _inity) { this->_inity = _inity; }
VTContext::EventHandler::MouseButtonState VTContext::EventHandler::get_mouse_button_state() const { return _mouse_button_state; }
void VTContext::EventHandler::set_mouse_button_state(VTContext::EventHandler::MouseButtonState _mouse_button_state) { this->_mouse_button_state = _mouse_button_state; }
float VTContext::EventHandler::get_dolly_sensitivity() const { return _dolly_sensitivity; }
void VTContext::EventHandler::set_dolly_sensitivity(float _dolly_sensitivity) { this->_dolly_sensitivity = _dolly_sensitivity; }
VTContext::EventHandler::EventHandler()
{
    _initx = 0;
    _inity = 0;

    _mouse_button_state = MouseButtonState::IDLE;

    _dolly_sensitivity = 1.0f;

    _trackball_manip.dolly(2.5f);
}
}