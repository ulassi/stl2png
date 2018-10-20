#include <string>
#include <vector>

int render_stl(const std::string& stl, std::vector<std::string>& options) { 
	/*
	if (!read_stl(stl,...))
		return -1;

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

    // compile pak
    return render_stl(input.front(), options);
}