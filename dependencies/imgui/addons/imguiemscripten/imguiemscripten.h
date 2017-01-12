#ifndef IMGUIEMSCRIPTEN_H_
#define IMGUIEMSCRIPTEN_H_



namespace ImGui {

// This class adds the "/persistent_folder" to the emscripten file system
// Files inside it persist when the html page is reloaded.

// USAGE:
/*
if (ImGui::EmscriptemFileSystemHelper::IsInSync())  {
      Load or save something in "/persistent_folder".
      // After saving something in "/persistent_folder":
      EmscriptemFileSystemHelper::Sync();   // To finalize all the changes inside  "/persistent_folder" on disk.
      // After the call above for some time ImGui::EmscriptemFileSystemHelper::IsInSync()==false.
}
*/
class EmscriptenFileSystemHelper {
protected:
static bool inited;

public:
EmscriptenFileSystemHelper() {Init();}
~EmscriptenFileSystemHelper() {Destroy();}

static void Init();
inline static bool IsInited() {return inited;}
static void Destroy() {}    // TODO (optionally...): unmount if possible

static bool IsInSync();
static void Sync();

};

}	// namespace ImGui

#endif //IMGUIEMSCRIPTEN_H_

