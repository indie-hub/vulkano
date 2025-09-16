MoltenVK Notes
---------------

- Install Vulkan SDK for macOS, which includes MoltenVK.
- Ensure the Vulkan loader can find the MoltenVK ICD JSON.
  If needed, export:

```
export VK_ICD_FILENAMES="/usr/local/share/vulkan/icd.d/MoltenVK_icd.json"
```

- Shader compilation uses `glslc` from the SDK. If missing, install the SDK or use `shaderc` via package manager.
- Validation layers are enabled in Debug if available; fallback is graceful when unavailable.

