#pragma once
#include "RmlSystemInterface.h"
#include "RmlFileInterface.h"

#include <EtGUI/Rendering/RmlFontEngineInterface.h>
#include <EtGUI/Rendering/RmlRenderInterface.h>
#include <EtGUI/Rendering/FontEffects.h>


namespace et {
namespace gui {


//---------------------------------
// RmlGlobal
//
// Singleton that manages global access into the RmlUi library
//
class RmlGlobal final
{
	// definitions
	//-------------
private:
	friend class RefPtr<RmlGlobal>;
	friend class Create<RmlGlobal>;

	static RefPtr<RmlGlobal> s_Instance;

	// singleton access
	//------------------
public:
	static RefPtr<RmlGlobal> GetInstance();
	static void Destroy();
	static bool IsInitialized();

	// construct destruct
	//--------------------
private:
	RmlGlobal();
	~RmlGlobal();

	// functionality
	//---------------
public:
	void SetCursorShapeManager(Ptr<core::I_CursorShapeManager> const cursorMan);
	void SetClipboardController(Ptr<core::I_ClipboardController> const clipboardController);

	void SetGraphicsContext(Ptr<render::I_GraphicsContextApi> const graphicsContext);
	void SetRIShader(AssetPtr<render::ShaderData> const shader, AssetPtr<render::ShaderData> const textShader);
	void SetRIView(ivec2 const dim, mat4 const& viewProj);

	// Data
	///////
	
private:
	RmlSystemInterface m_SystemInterface;
	RmlFileInterface m_FileInterface;
	RmlFontEngineInterface m_FontEngineInterface;
	RmlRenderInterface m_RenderInterface;

	FontEffectGlow::Instancer m_GlowInstancer;
	FontEffectOutline::Instancer m_OutlineInstancer;
	FontEffectShadow::Instancer m_ShadowInstancer;
	FontEffectBlur::Instancer m_BlurInstancer;
};


} // namespace gui
} // namespace et
