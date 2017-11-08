#include <iostream>
#include <sstream>
#include <vector>

#include <boost/assign/list_of.hpp>


#include <scm/core.h>
#include <scm/gl_core.h>

#include <scm/gl_util/primitives/quad.h>


#include <GL/freeglut.h>

scm::shared_ptr<scm::gl::render_device>     _device;
scm::shared_ptr<scm::gl::render_context>    _context;

scm::gl::program_ptr        _shader_program;

scm::shared_ptr<scm::gl::quad_geometry>  _quad;

scm::gl::program_ptr                _pass_through_shader;

/*void
initialize() {
  std::string v_pass = "\
        #version 330\n\
        \
        uniform mat4 mvp;\
        out vec2 tex_coord;\
        layout(location = 0) in vec3 in_position;\
        layout(location = 2) in vec2 in_texture_coord;\
        void main()\
        {\
            gl_Position = mvp * vec4(in_position, 1.0);\
            tex_coord = in_texture_coord;\
        }\
        ";
    std::string f_pass = "\
        #version 330\n\
        \
        in vec2 tex_coord;\
        uniform sampler2D in_texture;\
        layout(location = 0) out vec4 out_color;\
        void main()\
        {\
            out_color = texelFetch(in_texture, ivec2(gl_FragCoord.xy), 0).rgba;\
        }\
        ";
        _pass_through_shader = _device->create_program(list_of(_device->create_shader(STAGE_VERTEX_SHADER, v_pass))
                                                          (_device->create_shader(STAGE_FRAGMENT_SHADER, f_pass)));

}*/

void
glut_display() {
  using namespace scm;
  using namespace scm::gl;
  using namespace scm::math;

  mat4f    view_matrix         = _trackball_manip.transform_matrix();
  mat4f    model_matrix        = mat4f::identity();
  //scale(model_matrix, 0.01f, 0.01f, 0.01f);
  mat4f    model_view_matrix   = view_matrix * model_matrix;
  mat4f    mv_inv_transpose    = transpose(inverse(model_view_matrix));

  _shader_program->uniform("projection_matrix", _projection_matrix);
  _shader_program->uniform("model_view_matrix", model_view_matrix);
  _shader_program->uniform("model_view_matrix_inverse_transpose", mv_inv_transpose);
  //_shader_program->uniform("color_texture_aniso", 0);
  //_shader_program->uniform("color_texture_nearest", 1);

  mat4f   pass_mvp = mat4f::identity();
  ortho_matrix(pass_mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
  _pass_through_shader->uniform_sampler("in_texture", 0);
  _pass_through_shader->uniform("mvp", pass_mvp);
  _context->set_default_frame_buffer();

// _context->set_depth_stencil_state(_depth_no_z);
// _context->set_blend_state(_no_blend);

  _context->bind_program(_pass_through_shader);

// _context->bind_texture(_color_buffer_resolved, _filter_nearest, 0);

_quad->draw(_context);
 glutSwapBuffers();
}



int main(int argc, char** argv) {

  using namespace scm;
  using namespace scm::gl;
  using namespace scm::math;
  using boost::assign::list_of;

  std::cout << "Hello Lamure Virtual Texture!\n";

  scm::shared_ptr<scm::core> scm_core(new scm::core(argc, argv));

  glutInit(&argc, argv);
  glutInitContextVersion(4, 4);
  glutInitContextProfile(GLUT_CORE_PROFILE);
  //glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);

  // init a double buffered framebuffer with depth buffer and 4 channels
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA | GLUT_ALPHA | GLUT_MULTISAMPLE);
  // create window with initial dimensions
  glutInitWindowSize(1024, 1024);
  glutCreateWindow("hello_schism");

  glutDisplayFunc(glut_display);
  _device.reset(new scm::gl::render_device());
  _context = _device->main_context();


  _shader_program = _device->create_program(list_of(_device->create_shader(STAGE_VERTEX_SHADER, vs_source))
                                                   (_device->create_shader(STAGE_FRAGMENT_SHADER, fs_source)));

  
  _quad.reset(new scm::gl::quad_geometry(_device, scm::math::vec2f(0.0f, 0.0f), scm::math::vec2f(1.0f, 1.0f)));

  std::string v_pass = "\
        #version 330\n\
        \
        uniform mat4 mvp;\
        out vec2 tex_coord;\
        layout(location = 0) in vec3 in_position;\
        layout(location = 2) in vec2 in_texture_coord;\
        void main()\
        {\
            gl_Position = mvp * vec4(in_position, 1.0);\
            tex_coord = in_texture_coord;\
        }\
        ";
  std::string f_pass = "\
        #version 330\n\
        \
        in vec2 tex_coord;\
        uniform sampler2D in_texture;\
        layout(location = 0) out vec4 out_color;\
        void main()\
        {\
            out_color = texelFetch(in_texture, ivec2(gl_FragCoord.xy), 0).rgba;\
        }\
        ";
  _pass_through_shader = _device->create_program(list_of(_device->create_shader(STAGE_VERTEX_SHADER, v_pass))
                                                          (_device->create_shader(STAGE_FRAGMENT_SHADER, f_pass)));


  glutMainLoop(); //g0!
  return 0;
}
