[x] Add sampler anisotropy feature check and enable in device creation
[] Add Texture helpers: image creation, staging upload, mipmap blits
[] Procedural checkerboard albedo (sRGB) generation and upload
[] Procedural blue-noise-ish normal map (UNORM) generation and upload
[] Create samplers (albedo sRGB + anisotropy, normal UNORM)
[] Extend descriptor set layouts: set1 for albedo/normal; allocate per-primitive
[] Update shaders for albedo + normal mapping, normal strength, toggles via PC
[] Bind per-primitive textures and updated push constants in draw loop
[] Extend ImGui UI: per-primitive toggles, strength, UV tiling, base color; global texture stats
[] stb_image loading for external textures with mipmaps and sRGB/UNORM formats
[] Add unit tests for procedural generators; update README with new controls
