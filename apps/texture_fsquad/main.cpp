#include <algorithm>

#include <lamure/vt/common.h>
#include <lamure/vt/pre/Preprocessor.h>

using namespace vt;

char *get_cmd_option(char **begin, char **end, const std::string &option)
{
    char **it = std::find(begin, end, option);
    if(it != end && ++it != end)
        return *it;
    return nullptr;
}

bool cmd_option_exists(char **begin, char **end, const std::string &option) { return std::find(begin, end, option) != end; }

int main(int argc, char *argv[])
{
    if(argc == 1 || !cmd_option_exists(argv, argv + argc, "-c"))
    {
        std::cout << "Preprocessing in-core: " << argv[0] << " <flags> -p <raster> -c <config>" << std::endl;
        std::cout << "Preprocessing out-of-core: " << argv[0] << " <flags> -m -c <config>" << std::endl;
        std::cout << "Texturing context: " << argv[0] << " <flags> -c <config>" << std::endl;
        return -1;
    }

    std::string file_config = std::string(get_cmd_option(argv, argv + argc, "-c"));

    VTContext context = VTContext::Builder().with_path_config((file_config.c_str()))->with_event_handler(new VTContext::EventHandler())->build();

    if(get_cmd_option(argv, argv + argc, "-m") != nullptr || get_cmd_option(argv, argv + argc, "-p") != nullptr)
    {
        Preprocessor preprocessor(context);

        if(get_cmd_option(argv, argv + argc, "-p") != nullptr && get_cmd_option(argv, argv + argc, "-c") != nullptr)
        {
            std::string file_raster = std::string(get_cmd_option(argv, argv + argc, "-p"));

            preprocessor.prepare_raster(file_raster.c_str());
        }

        preprocessor.prepare_mipmap();
    }
    else
    {
        context.start();
    }

    return EXIT_SUCCESS;
}