#include <vulkano/app/asset_importer.hpp>

#include <stdexcept>

namespace vulkano::app {
ImportedScene AssetImporter::load_scene(std::string_view path) const {
    (void)path;
    throw std::runtime_error {"AssetImporter::load_scene is not implemented yet"};
}
} // namespace vulkano::app
