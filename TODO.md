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
- Create a hardcoded fallback for all resources when loading fail
- Compute shader support

### Todo (smaller stuff):
- change XMFLOAT4X4 TransformComponent::world to XMMATRIX

### Todo (editor)
- Undo for delete action
- SceneGraphView generates a new list every single frame, wich makes it VERY inefficent
- in terrain_generator.cpp:void ColorizeMountains(): generate new vertices when face  has both ground and rock vertices for cleaner seperation 
- LOD generation for terrain
- Editor default window layout
- Hide/Show button in entity select window
- Editor function to pan camera to selected entity
- Warn user about unsaved progress before clearing the scene when creating new scene or loading a scene
