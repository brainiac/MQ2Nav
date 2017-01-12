#include "imguiemscripten.h"
#include <emscripten.h>



namespace ImGui	{

bool EmscriptenFileSystemHelper::inited = false;
static EmscriptenFileSystemHelper gEmscriptemFileSystemHelper;

void EmscriptenFileSystemHelper::Init()	 {
    if (!inited)	{
        inited=true;
        EM_ASM(
            FS.mkdir('/persistent_folder');
            FS.mount(IDBFS, {}, '/persistent_folder');

            // sync from persisted state into memory and then
            // run the 'test' function
            Module.syncdone = 0;
            FS.syncfs(true, function (err) {                      
                      assert(!err);
                      Module.syncdone = 1;
                  });
        );
    }
}

void EmscriptenFileSystemHelper::Sync() {
  // sync from memory state to persisted and then
  // run 'success'
  EM_ASM(
	Module.syncdone = 0;  
    FS.syncfs(function (err) {
      assert(!err);
      Module.syncdone = 1;
    });
  );
}
// ccall('EmscriptemFileSystemHelperSuccess', 'v');

bool EmscriptenFileSystemHelper::IsInSync() {
    return (emscripten_run_script_int("Module.syncdone") == 1);
}

} // namespace ImGui

