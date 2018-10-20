#include <stdint.h>
#include <array>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <stb_image_write.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

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
	}
	else {
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

constexpr int STL_ELEM_SIZE = 3 * 4;
constexpr int STL_TRIANGLE_SIZE =
    4 * (STL_ELEM_SIZE /*normal*/ + 3 * STL_ELEM_SIZE /*verts*/) + 2 /*attribute*/;
constexpr int STL_MIN_SIZE = 4 + STL_TRIANGLE_SIZE;

std::optional<STLdata> read(const std::string& file) {
    if (std::ifstream fs = std::ifstream(file, std::ios_base::binary)) {
        STLdata data;
        std::streamsize sz = ::tell_file_size(fs);

        int64_t data_size = sz - 80 - 4;
        if (data_size < STL_MIN_SIZE) {
            puts("Not a binary STL...");
            return {};
        }

        fs.ignore(80);  // jump header

        uint32_t num_facets{0};
        fs.read(reinterpret_cast<char*>(&num_facets), 4);
        auto read_bytes = fs.gcount();
        if (read_bytes != 4) {
            puts("Failed reading num facets from file...");
            return {};
        }
        data.resize(num_facets);

        if (num_facets > 0x7FFFFFFF) {
            puts("Oh dear...");
            return {};
        }

        constexpr auto read_stl_elem = [](glm::vec3& to, std::ifstream& fs) -> bool {
            fs.read(reinterpret_cast<char*>(glm::value_ptr(to)), STL_ELEM_SIZE);
            return fs.gcount() == STL_ELEM_SIZE;
        };
        for (size_t i = 0; i < num_facets; ++i) {
            if (read_stl_elem(data[i].m_normal, fs) == false) {
                printf("Failed reading facet %llu from file...", i);
                return {};
            }
            for (int j = 0; j < 3; ++j) {
                if (read_stl_elem(data[i].m_vertices[j], fs) == false) {
                    printf("Failed reading facet %llu from file...", i);
                    return {};
                }
            }
            fs.read(reinterpret_cast<char*>(&data[i].m_attribute), 2);
            if (fs.gcount() != 2) {
                printf("Failed reading facet %llu from file...", i);
                return {};
            }
        }
        return data;
    } else {
        printf("Cannot open file, \"%s\"", file.c_str());
        return {};
    }
}
}  // namespace STL

namespace Graphics {

void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

#pragma pack(1)
struct Vert {
	Vert(const glm::vec3& pos, const glm::vec3& normal) : x(pos[0]), y(pos[1]), z(pos[2]), nx(normal[0]), ny(normal[1]), nz(normal[2])
	{}
	float x=0.f, y = 0.f, z = 0.f;
	float nx = 0.f, ny = 0.f, nz = 0.f;
	static constexpr int position_offset = 0;
	static constexpr int position_elements = 3;
	static constexpr int position_type = GL_FLOAT;
	static constexpr int normal_offset = 3*sizeof(float);
	static constexpr int normal_elements = 3;
	static constexpr int normal_type = GL_FLOAT;
};

void fill_vertex_buffer(const STL::STLdata& data, std::vector<Vert>& vertices) {
	for (auto& f : data) {
		for (auto& v : f.m_vertices) {
			vertices.emplace_back(v, f.m_normal);
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
	const GLchar* shader_string[] = { nullptr };
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
		fprintf(stderr, "Shader compilation error: %s", errorLog.data());
		return false;
	}
	return true;
}

}
int render_stl(const std::string& stl, std::vector<std::string>& options) {
    if (auto data = STL::read(stl)) {
		GLFWwindow* window{ nullptr };
		glfwSetErrorCallback(Graphics::error_callback);

		if (!glfwInit()) {
			fprintf(stderr, "Failed to init glfw");
			return -1;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

		window = glfwCreateWindow(640, 480, "STL2PNG", NULL, NULL);
		if (!window)
		{
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
			printf("Failed to link shader program");
			return -1;
		}

		glGenBuffers(1, &vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		
		std::vector<Graphics::Vert> vertices;
		fill_vertex_buffer(*data, vertices);
		auto buffer_size = sizeof(vertices)*vertices.size();
		glBufferData(GL_ARRAY_BUFFER, buffer_size, reinterpret_cast<void*>(vertices.data()), GL_STATIC_DRAW);
		
		
		GLint mvp_location, vposition_location, vnormal_location;
		mvp_location = glGetUniformLocation(program, "MVP");
		vposition_location = glGetAttribLocation(program, "vPosition");
		vnormal_location = glGetAttribLocation(program, "vNormal");

        glEnableVertexAttribArray(vposition_location);
        glVertexAttribPointer(vposition_location, Graphics::Vert::position_elements,
                                Graphics::Vert::position_type, GL_FALSE, sizeof(Graphics::Vert),
								reinterpret_cast<void*>(Graphics::Vert::position_offset));
		if (vnormal_location >= 0) {
			glEnableVertexAttribArray(vnormal_location);
			glVertexAttribPointer(vnormal_location, Graphics::Vert::normal_elements,
				Graphics::Vert::normal_type, GL_FALSE, sizeof(Graphics::Vert),
				reinterpret_cast<const void*>(int(Graphics::Vert::normal_offset)));
		}
		glm::mat4 view = glm::lookAt(glm::vec3(5.f, 5.f, 5.f), glm::vec3(0.f), glm::vec3(0.f,1.f,0.f));
		glm::mat4 model = glm::mat4(1.f);
		

		
		while (!glfwWindowShouldClose(window))
		{
			float ratio;
			int width, height;

			glfwGetFramebufferSize(window, &width, &height);
			ratio = width / (float)height;
			glm::mat4 proj = glm::perspective(45.0f, ratio, 0.1f, 100.f);
			glm::mat4 mvp = proj * view * model;

			glViewport(0, 0, width, height);
			glClearColor(0.1f, 0.1f, 0.1f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glUseProgram(program);
			glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
			glDisable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

			glfwSwapBuffers(window);
			glfwPollEvents();
		}

#if 0
        struct View {
            glm::mat4 m_viewMat;
            glm::mat4 m_projMat;
            std::string m_viewName;
        };
        std::array<View, 7> render_views = {View{glm::mat4(1.f), glm::mat4(1.f), "px"},
                                            View{glm::mat4(1.f), glm::mat4(1.f), "nx"},
                                            View{glm::mat4(1.f), glm::mat4(1.f), "py"},
                                            View{glm::mat4(1.f), glm::mat4(1.f), "ny"},
                                            View{glm::mat4(1.f), glm::mat4(1.f), "pz"},
                                            View{glm::mat4(1.f), glm::mat4(1.f), "nz"},
                                            View{glm::mat4(1.f), glm::mat4(1.f), "ortho"}};
        int w = 1920;
        int h = 1080;
        int c = 4;
        int byte_per_channel = 1;
        for (auto view : render_views) {

			

            /*setup_view_matrices();

            draw_view();

            auto pixels = read_framebuffer();
            */
            std::vector<uint8_t> pixels;
            pixels.resize(w * h * c * byte_per_channel);
			memset(pixels.data(), 0x77, pixels.size());

            std::stringstream name;
            name << std::string("view_") << view.m_viewName << ".png";
            if (1 != stbi_write_png(name.str().c_str(), w, h, c, pixels.data(),
                                    w * c * byte_per_channel)) {
                printf("Failed to write image \"%s\"", name.str().c_str());
				return -1;
            }
        }
#endif
		glfwDestroyWindow(window);
		glfwTerminate();
        return 0;
    } else {
        return -1;
    }
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
        } else {
            // file
            input.emplace_back(arg);
        }
    }

    if (input.empty()) {
        puts("No STL file to process... \n");
        return 1;
    }

    try {
        return render_stl(input.front(), options);
    } catch (std::exception& e) {
        printf("Unexpected error: %s", e.what());
        return -1;
    }
}