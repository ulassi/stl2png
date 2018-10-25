#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <stb_image_write.h>
#include <stdint.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::streamsize tell_file_size(std::ifstream& fs) {
    std::streamsize at = fs.tellg();
    fs.ignore(std::numeric_limits<std::streamsize>::max());
    std::streamsize sz = fs.gcount();
    fs.seekg(at);
    return at + sz;
}
bool file_to_string(const std::string& file, std::string& str) {
    if (std::ifstream fs = std::ifstream(file)) {
        str = std::string((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
        return true;
    } else {
        return false;
    }
}
}  // namespace

namespace STL {
struct STLfacet {
    glm::vec3 m_normal;
    glm::vec3 m_vertices[3];
    uint16_t m_attribute;
};

typedef std::vector<STLfacet> STLdata;

const int STL_ELEM_SIZE = 3 * 4;
const int STL_TRIANGLE_SIZE = 4 * (STL_ELEM_SIZE /*normal*/ + 3 * STL_ELEM_SIZE /*verts*/) + 2 /*attribute*/;
const int STL_MIN_SIZE = 4 + STL_TRIANGLE_SIZE;

std::optional<STLdata> read(const std::string& file) {
    if (std::ifstream fs = std::ifstream(file, std::ios_base::binary)) {
        STLdata data;
        std::streamsize sz = ::tell_file_size(fs);

        if (sz < 80) {
            std::fputs("Not a binary STL...", stderr);
            // TODO: try parse ascii instead?
            return {};
        }

        // try read header
        char header[81];
        fs.read(header, 80);
        if (fs.gcount() != 80) {
            std::fputs("Failed reading file...", stderr);
            return {};
        }
        std::string header_str(header, 80);
        if (header_str.find("solid") != std::string::npos) {
            std::fputs("Not a binary STL...", stderr);
            // TODO: try parse ascii instead?
            return {};
        }

        int64_t data_size = sz - 80 - 4;
        if (data_size < STL_MIN_SIZE) {
            std::fputs("Invalid binary STL...", stderr);
            return {};
        }

        uint32_t num_facets{0};
        fs.read(reinterpret_cast<char*>(&num_facets), 4);
        auto read_bytes = fs.gcount();
        if (read_bytes != 4) {
            fputs("Failed reading num facets from file...", stderr);
            return {};
        }
        data.resize(num_facets);

        constexpr auto read_stl_elem = [](glm::vec3& to, std::ifstream& fs) -> bool {
            fs.read(reinterpret_cast<char*>(glm::value_ptr(to)), STL_ELEM_SIZE);
            return fs.gcount() == STL_ELEM_SIZE;
        };
        for (auto& face : data) {
            if (read_stl_elem(face.m_normal, fs) == false) {
                fputs("Failed reading facet from file...", stderr);
                return {};
            }
            for (auto& v : face.m_vertices) {
                if (read_stl_elem(v, fs) == false) {
                    fputs("Failed reading vertex from file...", stderr);
                    return {};
                }
            }
            fs.read(reinterpret_cast<char*>(&face.m_attribute), 2);
            if (fs.gcount() != 2) {
                fputs("Failed reading facet from file...", stderr);
                return {};
            }
        }
        return data;
    } else {
        fprintf(stderr, "Cannot open file, \"%s\"", file.c_str());
        return {};
    }
}
}  // namespace STL

namespace Graphics {

void error_callback(int error, const char* description) { fprintf(stderr, "Error: %s\n", description); }

#pragma pack(1)
struct Vert {
    Vert(const glm::vec3& pos, const glm::vec3& normal)
        : x(pos[0]), y(pos[1]), z(pos[2]), nx(normal[0]), ny(normal[1]), nz(normal[2]) {}
    float x = 0.f, y = 0.f, z = 0.f;
    float nx = 0.f, ny = 0.f, nz = 0.f;
    static const int position_offset = 0;
    static const int position_elements = 3;
    static const int position_type = GL_FLOAT;
    static const int normal_offset = 3 * sizeof(float);
    static const int normal_elements = 3;
    static const int normal_type = GL_FLOAT;
};

void fill_vertex_buffer(const STL::STLdata& data, std::vector<Vert>& vertices, glm::vec3& vmin, glm::vec3& vmax,
                        glm::vec3& centroid) {
    using namespace glm;
    vmin = vec3(FLT_MAX);
    vmax = vec3(-FLT_MAX);
    centroid = vec3(0.f);
    for (auto& f : data) {
        vec3 fn = f.m_normal;
		if (length(fn) < 1e-5f) {
			fn = normalize(cross(f.m_vertices[1] - f.m_vertices[0], f.m_vertices[2] - f.m_vertices[0]));
		}
		else {
			fn = normalize(f.m_normal);
		}
		for (auto& v : f.m_vertices) {
            vertices.emplace_back(v, fn);
            vmin = glm::min(vmin, v);
            vmax = glm::max(vmax, v);
            centroid += (v / (data.size() * 3.f));
        }
    }
}

bool compileGLSLShaderFromFile(const std::string& file, GLint type, GLuint& shader_object) {
    std::string shader_code;
    if (::file_to_string(file, shader_code) == false) {
        fprintf(stderr, "Failed to read %s", file.c_str());
        return false;
    }
    GLint shader_code_len = static_cast<GLint>(shader_code.size());
    const GLchar* shader_string[] = {nullptr};
    shader_string[0] = shader_code.data();
    shader_object = glCreateShader(type);
    glShaderSource(shader_object, 1, shader_string, &shader_code_len);
    glCompileShader(shader_object);

    GLint params = GL_FALSE;
    glGetShaderiv(shader_object, GL_COMPILE_STATUS, &params);
    if (params != GL_TRUE) {
        GLint maxLength = 0;
        glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(shader_object, maxLength, &maxLength, &errorLog[0]);
        fprintf(stderr, "(%s) Shader compilation error: %s", file.c_str(), errorLog.data());
        return false;
    }
    return true;
}
}  // namespace Graphics

int render_stl(const std::string& stl, bool windowed) {
    using glm::mat4;
    using glm::vec3;
    if (auto data = STL::read(stl)) {
        GLFWwindow* window{nullptr};
        glfwSetErrorCallback(Graphics::error_callback);

        if (!glfwInit()) {
            fprintf(stderr, "Failed to init glfw");
            return -1;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_VISIBLE, windowed ? GLFW_TRUE : GLFW_FALSE);
        window = glfwCreateWindow(640, 480, "STL2PNG", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            fprintf(stderr, "Failed to create glfw window");
            return -1;
        }

        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        glfwSwapInterval(1);

        GLuint vertex_buffer, vertex_shader, fragment_shader, program;
        if (Graphics::compileGLSLShaderFromFile("vertex.glsl", GL_VERTEX_SHADER, vertex_shader) == false) {
            return -1;
        }
        if (Graphics::compileGLSLShaderFromFile("fragment.glsl", GL_FRAGMENT_SHADER, fragment_shader) == false) {
            return -1;
        }
        program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);
        GLint params = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &params);
        if (params != GL_TRUE) {
            fputs("Failed to link shader program", stderr);
            return -1;
        }

        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

        std::vector<Graphics::Vert> vertices;
        vec3 vert_min, vert_max, model_center;
        fill_vertex_buffer(*data, vertices, vert_min, vert_max, model_center);
        auto buffer_size = sizeof(vertices) * vertices.size();
        glBufferData(GL_ARRAY_BUFFER, buffer_size, reinterpret_cast<void*>(vertices.data()), GL_STATIC_DRAW);

        GLint mvp_location, vposition_location, vnormal_location, eye_location, model_location;
        mvp_location = glGetUniformLocation(program, "MVP");
        eye_location = glGetUniformLocation(program, "Eye");
        model_location = glGetUniformLocation(program, "M");
        vposition_location = glGetAttribLocation(program, "vPosition");
        vnormal_location = glGetAttribLocation(program, "vNormal");

        glEnableVertexAttribArray(vposition_location);
        glVertexAttribPointer(vposition_location, Graphics::Vert::position_elements, Graphics::Vert::position_type,
                              GL_FALSE, sizeof(Graphics::Vert),
                              reinterpret_cast<void*>(Graphics::Vert::position_offset));
        if (vnormal_location >= 0) {
            glEnableVertexAttribArray(vnormal_location);
            glVertexAttribPointer(vnormal_location, Graphics::Vert::normal_elements, Graphics::Vert::normal_type,
                                  GL_FALSE, sizeof(Graphics::Vert),
                                  reinterpret_cast<const void*>(int(Graphics::Vert::normal_offset)));
        }

        // Figure out a model to world matrix that normalizes the model scale and center
        float scale = 2.f / glm::compMax(vert_max - vert_min);
        vec3 translate = -model_center;
        mat4 model_T = glm::translate(mat4(1.f), translate);
        mat4 model_S = glm::scale(mat4(1.f), vec3(scale));
        mat4 model = model_S * model_T;

        struct View {
            mat4 m_viewMat;
            mat4 m_modelMat;
            vec3 m_eyeVec;
            bool m_perspective = true;
            std::string m_viewName;
        };

        float vd = 4.f;
        using glm::lookAt;
        std::array<View, 7> render_views = {
            View{lookAt(vec3(vd, 0.f, 0.f), vec3(0.f), vec3(0.f, 1.f, 0.f)), model, vec3(vd, 0.f, 0.f),  true, "px"},
            View{lookAt(vec3(-vd, 0.f, 0.f),vec3(0.f), vec3(0.f, 1.f, 0.f)), model, vec3(-vd, 0.f, 0.f), true, "nx"},
            View{lookAt(vec3(0.f, vd, 0.f), vec3(0.f), vec3(0.f, 0.f, 1.f)), model, vec3(0.f, vd, 0.f),  true, "py"},
            View{lookAt(vec3(0.f, -vd, 0.f),vec3(0.f), vec3(0.f, 0.f, 1.f)), model, vec3(0.f, -vd, 0.f), true, "ny"},
            View{lookAt(vec3(0.f, 0.f, vd), vec3(0.f), vec3(0.f, 1.f, 0.f)), model, vec3(0.f, 0.f, vd),  true, "pz"},
            View{lookAt(vec3(0.f, 0.f, -vd),vec3(0.f), vec3(0.f, 1.f, 0.f)), model, vec3(0.f, 0.f, -vd), true, "nz"},
            View{lookAt(vec3(vd, vd, vd),   vec3(0.f), vec3(0.f, 1.f, 0.f)), model, vec3(vd, vd, vd),   false, "or"},
        };

        auto draw_gl_view = [](const View& view, int width, int height, GLuint program, GLuint mvp_loc, GLuint eye_loc,
                               GLuint model_loc, GLsizei verts) {
            float ratio = width / (float)height;
            mat4 proj;
            if (view.m_perspective) {
                proj = glm::perspective(45.0f, ratio, 0.1f, 100.f);
            } else {
                float os = 2.5;
                proj = glm::ortho(-os * ratio, os * ratio, -os, os, 0.f, 100.f);
            }
            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.1f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(program);
            glm::mat4 mvp = proj * view.m_viewMat * view.m_modelMat;
            glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3fv(eye_loc, 1, glm::value_ptr(view.m_eyeVec));
            glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(view.m_modelMat));
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDrawArrays(GL_TRIANGLES, 0, verts);
        };
        if (windowed) {
            // show a window cycling through the views, showing each for a set number of frames
            signed count = 0;
            signed frames_per_view = 100;
            while (!glfwWindowShouldClose(window)) {
                int width{0}, height{0};
                glfwGetFramebufferSize(window, &width, &height);
                draw_gl_view(render_views[count / frames_per_view], width, height, program, mvp_location, eye_location,
                             model_location, static_cast<GLsizei>(vertices.size()));
                glfwSwapBuffers(window);
                glfwPollEvents();
                ++count;
                count %= (frames_per_view * render_views.size());
            }
        } else {
            // headless render to framebuffer and write out png files
            int channels = 4;
            int bytes_per_channel = 1;
            glfwSetWindowSize(window, 1920, 1080);
            int width{0}, height{0};
            glfwGetFramebufferSize(window, &width, &height);
            float ratio = width / (float)height;
            for (const auto& view : render_views) {
                draw_gl_view(view, width, height, program, mvp_location, eye_location, model_location,
                             static_cast<GLsizei>(vertices.size()));

                std::vector<uint8_t> pixels;
                pixels.resize(width * height * channels * bytes_per_channel);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

                std::stringstream name;
                name << std::string("view_") << view.m_viewName << ".png";
                int stride = width * channels * bytes_per_channel;
                if (stbi_write_png(name.str().c_str(), width, height, channels, pixels.data(), stride) != 1) {
                    fprintf(stderr, "Failed to write image \"%s\"", name.str().c_str());
                    return -1;
                }
            }
        }
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } else {
        return -1;
    }
}

void print_usage() {
    std::fputs(R"(
Usage:	
	stl2png [-window] file.stl
	
	Given an STL binary file renders 7 views and outputs as view_xx.png in same current directory.
	Needs fragment.glsl and vertex.glsl in current directory.

		-window		option will open a renderwindow and draw the object
)",
               stdout);
}

int main(int argc, const char** argv) {
    using namespace std;

    vector<string> input;
    vector<string> options;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg.empty()) continue;

        if (arg[0] == '-') {
            // option
            options.emplace_back(arg.substr(1));
            std::transform(std::begin(options.back()), std::end(options.back()), std::begin(options.back()),
                           std::tolower);
        } else {
            // file
            input.emplace_back(arg);
        }
    }

    if (input.empty()) {
        fprintf(stderr, "No STL file to process... \n");
        print_usage();
        return 1;
    }
    bool window = std::find(std::begin(options), std::end(options), "window") != std::end(options);
    try {
        return render_stl(input.front(), window);
    } catch (std::exception& e) {
        fprintf(stderr, "Unexpected error: %s", e.what());
        return -1;
    }
}
