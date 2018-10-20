#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <optional>
#include <stdint.h>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>

struct STLfacet {
	glm::vec3	m_normal;
	glm::vec3	m_vertices[3];
	uint16_t	m_attribute;
};

typedef std::vector<STLfacet> STLdata;

namespace {
	std::streamsize tell_file_size(std::ifstream& fs) {
		std::streamsize at = fs.tellg();
		fs.ignore(std::numeric_limits<std::streamsize>::max());
		std::streamsize sz = fs.gcount();
		fs.seekg(at);
		return at + sz;
	}
	constexpr int STL_ELEM_SIZE = 3 * 4;
	constexpr int STL_TRIANGLE_SIZE = 4 * (STL_ELEM_SIZE/*normal*/ + 3 * STL_ELEM_SIZE/*verts*/) + 2/*attribute*/;
	constexpr int STL_MIN_SIZE = 4 + STL_TRIANGLE_SIZE;
}

std::optional<STLdata> read_stl(const std::string& file) {
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

        for (size_t i = 0; i < num_facets; ++i) {
            fs.read(reinterpret_cast<char*>(glm::value_ptr(data[i].m_normal)), ::STL_ELEM_SIZE);
            if (fs.gcount() != ::STL_ELEM_SIZE) {
                printf("Failed reading facet %d from file...", i);
                return {};
            }
            for (int j = 0; j < 3; ++j) {
                fs.read(reinterpret_cast<char*>(glm::value_ptr(data[i].m_vertices[j])),
                        ::STL_ELEM_SIZE);
                if (fs.gcount() != ::STL_ELEM_SIZE) {
                    printf("Failed reading facet %d from file...", i);
                    return {};
                }
            }
			fs.read(reinterpret_cast<char*>(&data[i].m_attribute), 2);
			if (fs.gcount() != 2) {
				printf("Failed reading facet %d from file...", i);
				return {};
			}
        }
        return data;
    } else {
        printf("Cannot open file, \"%s\"", file.c_str());
        return {};
    }
}

int render_stl(const std::string& stl, std::vector<std::string>& options) { 
	if ( auto data = read_stl(stl)){ 
		
		/*
		setup_opengl_context();

		bind_opengl_resources();

		auto render_views = {

		};

		for ( auto view : render_views ) {
			setup_view_matrices();

			draw_view();

			auto pixels = read_framebuffer();

			write_png( "xxx.png", pixels);
		}
		*/
		return 0; 
	}
	else {
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
	}
	catch (std::exception& e) {
		printf("Unexpected error: %s", e.what());
		return -1;
	}
}