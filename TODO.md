### Bugs:
- Unlit flat shader doesent render anything
- Depth buffer only goes from 0 - 0.01 according to renderdoc (seems fine in-game though so not sure it's a bug)

### Todo (features):
- Animation entity
- LOD support
- Water
- Write/Read pso cache off disk
- Fullscreen / Window mode toggle
- Move shader loading/compiling to resource_manager
- Add file watch to resource_manager to auto-reload changed files
- Create a hardcoded fallback for all resources when loading fail
- Compute shader support
- Change buildsystem to CMake

### Todo (editor)
- Draw outline around selected entity
- LOD generation for terrain
- Editor default window layout
- Hide/Show button in entity select window
- Editor function to pan camera to selected entity
- Warn user about unsaved progress before clearing the scene when creating new scene or loading a scene

### Todo (code changes):
- move profiler from core to systems(?) sence it uses graphics device or split CPU and GPU profiling and move the GPU part to graphics
- editor.cpp: bool vsync_enabled = true;      	// FIXME: initial value has to be manually synced with SwapChainDesc::vsync
    