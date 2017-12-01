#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

#include <boost/assign/list_of.hpp>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <scm/core.h>
#include <scm/log.h>
#include <scm/core/pointer_types.h>
#include <scm/core/io/tools.h>
#include <scm/core/time/accum_timer.h>
#include <scm/core/time/high_res_timer.h>

#include <scm/gl_core.h>

#include <scm/gl_util/data/imaging/texture_loader.h>
#include <scm/gl_util/manipulators/trackball_manipulator.h>
#include <scm/gl_util/primitives/box.h>
#include <scm/gl_util/primitives/quad.h>
#include <scm/gl_util/primitives/wavefront_obj.h>


//#include <GL/gl.h>
#include <GL/freeglut.h>
//#include <GL/gl3.h>

//window size
static uint32_t winx = 800;
static uint32_t winy = 800;

scm::shared_ptr<scm::gl::render_context> _context;
scm::gl::texture_2d_ptr                  _physical_texture;
scm::gl::texture_2d_ptr                  _index_texture;


class demo_app {
public:
    demo_app() {
        _initx = 0;
        _inity = 0;

        //für die maustasten, click false
        _lb_down = false;
        _mb_down = false;
        _rb_down = false;

        _dolly_sens = 1.0f;

        _projection_matrix = scm::math::mat4f::identity();
    }
    virtual ~demo_app();

    bool initialize(uint32_t tile_size, uint32_t max_quadtree_level, uint32_t index_texture_size,
                    uint32_t physical_texture_size);
    void initialize_physical_texture();
    void display();
    void resize(int w, int h);
    void mousefunc(int button, int state, int x, int y);
    void mousemotion(int x, int y);
    void keyboard(unsigned char key, int x, int y);
    void calc_x_and_y_offset_id(unsigned & x_offset_id, unsigned &  y_offset_id, unsigned const& on_id_local_child_id );
    void tileloader(uint32_t level);
    void initialize_index_texture();
    void update_index_texture(const std::vector<uint8_t> &cpu_buffer);
    void physical_texture_test_layout();
    void toggle_view();
    void calculate_best_physical_texture_size(uint32_t size_in_mb);
    void update_physical_texture_blockwise(char *buffer, uint32_t x, uint32_t y);

private:
    uint32_t _tile_size;
    uint32_t _max_quadtree_level;

    scm::math::vec2ui _index_texture_dimension;
    scm::math::vec2ui _physical_texture_dimension;

    bool toggle_phyiscal_texture_image_viewer = true;

    //trackball -> mouse and x+y coord.
    scm::gl::trackball_manipulator _trackball_manip;
    float _initx;
    float _inity;

    //mouse button state
    bool _lb_down;
    bool _mb_down;
    bool _rb_down;

    //intensity of zoom
    float _dolly_sens;

    //GraKa Stuff
    scm::shared_ptr<scm::gl::render_device>          _device;
    //_context

    scm::gl::program_ptr                             _shader_program;

    scm::gl::buffer_ptr                              _index_buffer;
    scm::gl::vertex_array_ptr                        _vertex_array;

    scm::math::mat4f                                 _projection_matrix;

    scm::shared_ptr<scm::gl::wavefront_obj_geometry> _obj;
    scm::gl::depth_stencil_state_ptr                 _dstate_less;

    scm::gl::sampler_state_ptr                       _filter_nearest;
    scm::gl::sampler_state_ptr                       _filter_linear;

    scm::gl::rasterizer_state_ptr                    _ms_no_cull;



}; // class demo_app

namespace  {

scm::scoped_ptr<demo_app> _application;

} // namespace


demo_app::~demo_app() {
    _shader_program.reset();
    _index_buffer.reset();
    _vertex_array.reset();

    _obj.reset();

    _filter_nearest.reset();
    _filter_linear.reset();

    _ms_no_cull.reset();

    _context.reset();
    _device.reset();
}

bool demo_app::initialize(uint32_t tile_size,
                          uint32_t max_quadtree_level,
                          uint32_t index_texture_size,
                          uint32_t physical_texture_size) {
    using namespace scm;
    using namespace scm::gl;
    using namespace scm::math;
    using boost::assign::list_of;

    this -> _tile_size = tile_size;
    this -> _max_quadtree_level = max_quadtree_level;
    this -> _index_texture_dimension = scm::math::vec2ui(index_texture_size, index_texture_size);

    calculate_best_physical_texture_size(physical_texture_size);

    // Load shader programs from file
    std::string vs_source;
    std::string fs_source;

    if (   !io::read_text_file("../../apps/texture_fsquad/shaders/virtual_texturing.glslv", vs_source)
        || !io::read_text_file("../../apps/texture_fsquad/shaders/virtual_texturing.glslf", fs_source)) {
        scm::err() << "error reading shader files" << log::end;
        return (false);
    }

    _device.reset(new scm::gl::render_device());
    _context = _device->main_context();

    _shader_program = _device->create_program(list_of(_device->create_shader(STAGE_VERTEX_SHADER, vs_source))
                                                     (_device->create_shader(STAGE_FRAGMENT_SHADER, fs_source)));

    if (!_shader_program) {
        scm::err() << "error creating shader program" << log::end;
        return (false);
    }

    _dstate_less    = _device->create_depth_stencil_state(true, true, COMPARISON_LESS);

    _obj.reset(new wavefront_obj_geometry(_device, "../../apps/texture_fsquad/geometry/quad.obj"));
    _filter_nearest = _device->create_sampler_state(FILTER_MIN_MAG_NEAREST, WRAP_CLAMP_TO_EDGE);
    _filter_linear  = _device->create_sampler_state(FILTER_MIN_MAG_LINEAR, WRAP_CLAMP_TO_EDGE);


    // TODO: define phyiscal texture size,as huge as possible
    initialize_physical_texture();
    // TODO: define index texture size, tile space of vt
    initialize_index_texture();

    _ms_no_cull = _device->create_rasterizer_state(FILL_SOLID, CULL_NONE, ORIENT_CCW, true);

    _trackball_manip.dolly(2.5f);

    return (true);
}


void demo_app::display() {
    using namespace scm::gl;
    using namespace scm::math;

    // clear the color and depth buffer
    //glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    mat4f    view_matrix         = _trackball_manip.transform_matrix();
    mat4f    model_matrix        = mat4f::identity();
    //scale(model_matrix, 0.01f, 0.01f, 0.01f);

             model_matrix        = scm::math::make_translation(0.0f, 0.0f, -2.0f);
    mat4f    model_view_matrix   = /*view_matrix **/ model_matrix;
    mat4f    mv_inv_transpose    = transpose(inverse(model_view_matrix));

    _shader_program->uniform("projection_matrix", _projection_matrix);
    _shader_program->uniform("model_view_matrix", model_view_matrix);
    _shader_program->uniform("model_view_matrix_inverse_transpose", mv_inv_transpose);

    // upload necessary information to vertex shader
    _shader_program->uniform("in_physical_texture_dim", _physical_texture_dimension);
    _shader_program->uniform("in_index_texture_dim", _index_texture_dimension);
    _shader_program->uniform("in_max_level", _max_quadtree_level);
    _shader_program->uniform("in_toggle_view", toggle_phyiscal_texture_image_viewer);

    _context->clear_default_color_buffer(FRAMEBUFFER_BACK, vec4f(.2f, .2f, .2f, 1.0f));
    _context->clear_default_depth_stencil_buffer();

    _context->reset();

    { // multi sample pass
        context_state_objects_guard csg(_context);
        context_texture_units_guard tug(_context);
        context_framebuffer_guard   fbg(_context);

        _context->set_viewport(viewport(vec2ui(0, 0), 1 * vec2ui(winx, winy)));

        _context->set_depth_stencil_state(_dstate_less);

        //don't perform backface culling
        _context->set_rasterizer_state(_ms_no_cull);

        _context->bind_program(_shader_program);

        //bind our texture and tell the graphics card to filter the samples linearly
        // TODO physical texture later with linear filter
        _context->bind_texture(_physical_texture, _filter_nearest, 0);
        _context->bind_texture(_index_texture,    _filter_nearest, 1);

        _obj->draw(_context, geometry::MODE_SOLID);
    }

    _context->reset();

    // swap the back and front buffer, so that the drawn stuff can be seen
    glutSwapBuffers();
}

void demo_app::resize(int w, int h) {
    // safe the new dimensions
    winx = w;
    winy = h;

    // set the new viewport into which now will be rendered
    using namespace scm::gl;
    using namespace scm::math;

    _context->set_viewport(viewport(vec2ui(0, 0), vec2ui(w, h)));

    scm::math::perspective_matrix(_projection_matrix, 60.f, float(w)/float(h), 0.01f, 1000.0f);
}

// linke Maustaste: Left Button Down prüfen -> wenn ja, dann rotate
// rechte Maustase: right button down -> wenn ja, zoom in
// mausrad: middle button -> wenn ja, translate
void demo_app::mousefunc(int button, int state, int x, int y) {
    switch (button) {
        case GLUT_LEFT_BUTTON:
            {
                _lb_down = (state == GLUT_DOWN) ? true : false;
            }break;
        case GLUT_MIDDLE_BUTTON:
            {
                _mb_down = (state == GLUT_DOWN) ? true : false;
            }break;
        case GLUT_RIGHT_BUTTON:
            {
                _rb_down = (state == GLUT_DOWN) ? true : false;
            }break;
    }

    _initx = 2.f * float(x - (winx/2))/float(winx);
    _inity = 2.f * float(winy - y - (winy/2))/float(winy);
}

void demo_app::mousemotion(int x, int y) {
    float nx = 2.f * float(x - (winx/2))/float(winx);
    float ny = 2.f * float(winy - y - (winy/2))/float(winy);

    if (_lb_down) {
        _trackball_manip.rotation(_initx, _inity, nx, ny);
    }
    if (_rb_down) {
        _trackball_manip.dolly(_dolly_sens * (ny - _inity));
    }
    if (_mb_down) {
        _trackball_manip.translation(nx - _initx, ny - _inity);
    }

    _inity = ny;
    _initx = nx;
}

void demo_app::keyboard(unsigned char key, int x, int y) {
}

void glut_display() {
    if (_application)
        _application->display();
}

void glut_resize(int w, int h) {
    if (_application)
        _application->resize(w, h);
}

void glut_mousefunc(int button, int state, int x, int y) {
    if (_application)
        _application->mousefunc(button, state, x, y);
}

void glut_mousemotion(int x, int y) {
    if (_application)
        _application->mousemotion(x, y);
}

void glut_idle() {
    glutPostRedisplay();
}

void glut_keyboard(unsigned char key, int x, int y) {
    std::vector<uint8_t> cpu_idx_texture_buffer_state;
    switch (key) {
        // ESC key
        case 27:
            _application.reset();
            std::cout << "reset application" << std::endl;
            exit (0);
        case 'f':
            glutFullScreenToggle();
            break;
        case ' ':
            _application -> toggle_view();
            break;
        case '0':
            cpu_idx_texture_buffer_state = std::vector<uint8_t>(16*3, 0);
            _application -> update_index_texture(cpu_idx_texture_buffer_state);
            break;
        case '1':
            cpu_idx_texture_buffer_state = {1, 0, 1, 1, 0, 1, 2, 0, 1, 2, 0, 1,
                                            1, 0, 1, 1, 0, 1, 2, 0, 1, 2, 0, 1,
                                            3, 0, 1, 3, 0, 1, 4, 0, 1, 4, 0, 1,
                                            3, 0, 1, 3, 0, 1, 4, 0, 1, 4, 0, 1};
            _application -> update_index_texture(cpu_idx_texture_buffer_state);
            break;
        case '2':
            cpu_idx_texture_buffer_state = {5, 0, 2, 6, 0, 2, 2, 1, 2, 3, 1, 2,
                                            0, 1, 2, 1, 1, 2, 4, 1, 2, 5, 1, 2,
                                            6, 1, 2, 0, 2, 2, 3, 2, 2, 4, 2, 2,
                                            1, 2, 2, 2, 2, 2, 5, 2, 2, 6, 2, 2};
            _application -> update_index_texture(cpu_idx_texture_buffer_state);
            break;
        case '3':
            cpu_idx_texture_buffer_state = {1, 0, 1, 1, 0, 1, 2, 0, 1, 2, 0, 1,
                                            1, 0, 1, 1, 0, 1, 2, 0, 1, 2, 0, 1,
                                            3, 0, 1, 3, 0, 1, 3, 2, 2, 4, 2, 2,
                                            3, 0, 1, 3, 0, 1, 5, 2, 2, 6, 2, 2};
            _application -> update_index_texture(cpu_idx_texture_buffer_state);
            break;
        default:
            _application->keyboard(key, x, y);
    }
}

void demo_app::calc_x_and_y_offset_id(unsigned & x_offset_id,
                                      unsigned & y_offset_id,
                                      unsigned const& on_id_local_child_id){
	int digit_on_id_counter = 0;

	int tmp_id = on_id_local_child_id;

	while(tmp_id != 0){
		int current_front_bit = tmp_id % 2;
		int write_in_y        = digit_on_id_counter % 2;
		int write_id_in_var   = digit_on_id_counter / 2;

		if(write_in_y == 1){
			y_offset_id = (y_offset_id) | (current_front_bit << write_id_in_var);
		}
		else{
			x_offset_id = (x_offset_id) | (current_front_bit << write_id_in_var);
		}

		tmp_id = tmp_id >> 1;

		++digit_on_id_counter;
	}
}


void demo_app::tileloader(uint32_t level) {
    int tilesize = 256*256*4;

	std::ifstream is ("../../apps/texture_fsquad/datatiles/numbered_tiles_w256_h256_t8x8_RGBA8.data", std::ios::binary);

    //TODO tileset beginn dynamisch übergeben

    // calulate begin and end depending on the LOD
    double tiles_per_level = pow(4, level);
    double begin = 0;

    for (int i = 0; i < level; ++i) {
        begin += pow(4, i);
    }

    double end = begin + tiles_per_level;

    int offsetbeg = tilesize * begin;
    int offsetend = tilesize * end;

	if (is) {
        is.seekg(offsetend);
        is.seekg(0,is.cur);
        int length = offsetend - offsetbeg;
        is.seekg(offsetbeg);

        //allocate memory
        auto* buffer = new char [length];

        //read data as a block
        is.read(buffer, length);

        for (unsigned j = 0; j < pow(4, level); ++j) {
            unsigned offset_x = 0;
            unsigned offset_y = 0;
            calc_x_and_y_offset_id(offset_x, offset_y, j);
            _context->update_sub_texture(_physical_texture,
                             scm::gl::texture_region( scm::math::vec3ui(offset_x*256, offset_y*256, 0),
                             scm::math::vec3ui(256,256, 1)),
                             0,
                             scm::gl::FORMAT_RGBA_8,
                             &buffer[tilesize*j]
                            );
        }

		delete[] buffer;
    };
}

void demo_app::physical_texture_test_layout() {
    int tilesize = _tile_size * _tile_size * 4;

//    std::ifstream is ("../../apps/texture_fsquad/datatiles/numbered_tiles_w256_h256_t8x8_RGBA8.data", std::ios::binary);
    std::ifstream is ("../../apps/texture_fsquad/datatiles/test.data", std::ios::binary);

    int offset_beg = 0;
    int offset_end = tilesize * _physical_texture_dimension.x * _physical_texture_dimension.y;

    if (is) {
        int length = offset_end - offset_beg;

        //allocate memory
        auto* buffer = new char [length];

        //read data as a block
        is.read(buffer, length);
        int counter = 0;
        for (unsigned y = 0; y < _physical_texture_dimension.y ; ++y) {
            for (unsigned x = 0; x < _physical_texture_dimension.x; ++x) {
                _context->update_sub_texture(_physical_texture,
                                             scm::gl::texture_region(scm::math::vec3ui(x*_tile_size, y*_tile_size, 0),
                                                                     scm::math::vec3ui(_tile_size,_tile_size, 1)),
                                             0,
                                             scm::gl::FORMAT_RGBA_8,
                                             &buffer[counter]
                );
                counter += tilesize;
            }
        }

        delete[] buffer;
    };
}

void update_physical_texture_blockwise(char *buffer, uint32_t x, uint32_t y) {
    /*int tilesize = _tile_size * _tile_size * 4;

    std::ifstream is ("../../apps/texture_fsquad/datatiles/numbered_tiles_w256_h256_t8x8_RGBA8.data", std::ios::binary);
   // std::ifstream is ("../../apps/texture_fsquad/datatiles/test.data", std::ios::binary);


    if (is) {
        int length = offset_end - offset_beg;

        //allocate memory
        auto* buffer = new char [length];
        is.read(buffer, length);

        _context->update_sub_texture(_physical_texture,
                                     scm::gl::texture_region(scm::math::vec3ui(x*_tile_size, y*_tile_size, 0),
                                     scm::math::vec3ui(_tile_size,_tile_size, 1)),
                                     0,
                                     scm::gl::FORMAT_RGBA_8,
                                     &buffer[*buffer]
                                    );
        delete[] buffer;
    };*/
}

void demo_app::initialize_physical_texture() {
    using namespace scm::gl;
    using namespace scm::math;
    _physical_texture = _device->create_texture_2d(_physical_texture_dimension * _tile_size, FORMAT_RGBA_8);
    physical_texture_test_layout();
}


void demo_app::update_index_texture(std::vector<uint8_t> const &cpu_buffer){
    _context->update_sub_texture(_index_texture,
                                 scm::gl::texture_region(scm::math::vec3ui(0, 0, 0),
                                                         scm::math::vec3ui(_index_texture_dimension, 1)),
                                 0,
                                 scm::gl::FORMAT_RGB_8UI,
                                 &cpu_buffer[0] );
}

void demo_app::initialize_index_texture(){
    using namespace scm::gl;
    using namespace scm::math;
    int img_size = _index_texture_dimension.x * _index_texture_dimension.y * 3;
    _index_texture = _device->create_texture_2d(_index_texture_dimension, FORMAT_RGB_8UI);

    // create img_size elements in vector with value 0
    std::vector<uint8_t> cpu_index_buffer(img_size, 0);
    update_index_texture(cpu_index_buffer);
}

void demo_app::calculate_best_physical_texture_size(uint32_t size_in_mb) {
    // TODO: calculate based on the given size the optimal width and height
    _physical_texture_dimension = scm::math::vec2ui(7,3);
}

void demo_app::toggle_view() {
    toggle_phyiscal_texture_image_viewer ^= 1;
}

/**
 * Initialize and run application.
 * Commandline arguments in this order:
 *   <tile_size>             // Size of a tile in pixel (width and height are equal)
 *   <max_quadtree_level>    // Number of the leaf level of the quadtree
 *   <index_texture_size>    // Size of max(leaf_tiles_in_x, leaf_tiles_in_y). Index texture is quadratic.
 *   <physical_texture_size> // Size of the physical texture in MB (application will determine the optimal number of tiles per row and column)
 *
 *   Example cmd line input: 256 2 4 512
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv) {
    scm::shared_ptr<scm::core> scm_core(new scm::core(1, argv));
    _application.reset(new demo_app());

    if (argc != 5) {
        return 0;
    }

    // parse command line arguments
    uint32_t tile_size, max_quadtree_level, index_texture_size, physical_texture_size;
    tile_size             = atoi(argv[1]);
    max_quadtree_level    = atoi(argv[2]);
    index_texture_size    = atoi(argv[3]);
    physical_texture_size = atoi(argv[4]);

    // the stuff that has to be done
    glutInit(&argc, argv);
    glutInitContextVersion(4, 4);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    //glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);

    // init a double buffered framebuffer with depth buffer and 4 channels
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA | GLUT_ALPHA | GLUT_MULTISAMPLE);
    // create window with initial dimensions
    glutInitWindowSize(winx, winy);
    glutCreateWindow("simple_glut");

    // init the GL context
    if (!_application->initialize(tile_size, max_quadtree_level, index_texture_size, physical_texture_size)) {
        std::cout << "error initializing gl context" << std::endl;
        return (-1);
    }
 
    // set the callbacks for resize, draw and idle actions
    glutReshapeFunc(glut_resize);
    glutDisplayFunc(glut_display);
    glutKeyboardFunc(glut_keyboard);
    glutMouseFunc(glut_mousefunc);
    glutMotionFunc(glut_mousemotion);
    glutIdleFunc(glut_idle);

    // and finally start the event loop
    glutMainLoop();
    return (0);
}
