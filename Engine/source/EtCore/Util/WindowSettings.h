#pragma once


namespace et {
namespace core {


//---------------------------------
// WindowSettings
//
// Configuration for window settings
//
struct WindowSettings
{
	WindowSettings() = default;

	// accessors
	//-----------
	ivec2 GetSize() const;

	// Settings loaded from JSON
	//---------------------------
	std::string m_Title = "ETEngine";
	bool m_Fullscreen = false;
	std::vector<ivec2> m_Resolutions;
	size_t m_FullscreenRes;
	size_t m_WindowedRes;
};


} // namespace core
} // namespace et