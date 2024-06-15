#pragma once
#include <EtCore/Content/AssetPointer.h>

#include <EtRendering/GraphicsTypes/FrameBuffer.h>
#include <EtRendering/GraphicsTypes/PostProcessingSettings.h>


namespace et {
namespace render {


class ShaderData;
class TextureData;
class I_OverlayRenderer;


static const int32 NUM_BLOOM_DOWNSAMPLES = 5;

class PostProcessingRenderer 
{
public:
	PostProcessingRenderer();
	~PostProcessingRenderer();

	void Initialize();

	void EnableInput();
	void Draw(T_FbLoc const FBO, PostProcessingSettings const& settings, std::function<void(T_FbLoc const)> const& onDrawOverlaysFn);

	T_FbLoc GetTargetFBO() { return m_CollectFBO; }
	TextureData* GetTargetTexture() { return m_CollectTex; }

private:

	void DeleteFramebuffers();
	void GenerateFramebuffers();
	void ResizeFBTextures();

	AssetPtr<ShaderData> m_pDownsampleShader;
	AssetPtr<ShaderData> m_pGaussianShader;
	AssetPtr<ShaderData> m_pPostProcShader;
	AssetPtr<ShaderData> m_pFXAAShader;

	T_FbLoc m_CollectFBO;
	TextureData* m_CollectTex = nullptr;
	T_RbLoc m_CollectRBO;

	T_FbLoc m_HDRoutFBO;
	TextureData* m_ColorBuffers[2];

	T_FbLoc m_PingPongFBO[2];
	TextureData* m_PingPongTexture[2];

	T_FbLoc m_DownSampleFBO[NUM_BLOOM_DOWNSAMPLES];
	TextureData* m_DownSampleTexture[NUM_BLOOM_DOWNSAMPLES];
	T_FbLoc m_DownPingPongFBO[NUM_BLOOM_DOWNSAMPLES];
	TextureData* m_DownPingPongTexture[NUM_BLOOM_DOWNSAMPLES];
};


} // namespace render
} // namespace et
