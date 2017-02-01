// requires:
// defining IMGUI_INCLUDE_IMGUI_USER_H and IMGUI_INCLUDE_IMGUI_USER_INL
// at the project level


//----------------------------------------------------------------------------

std::shared_ptr<ImGuiContext> GImGuiGlobalSharedPointer;

struct ImGuiContextInitializer
{
	ImGuiContextInitializer() { GImGuiGlobalSharedPointer = std::make_shared<ImGuiContext>(); }
	~ImGuiContextInitializer() {}
};

ImGuiContextInitializer s_imGuiContextIniializer;


#pragma warning (push)
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#pragma warning (disable: 4800)

#include "addons/imgui_user.inl"

#pragma warning (pop)
