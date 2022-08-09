#include "stdafx.h"
#include "ShadedSceneRenderer.h"

#include <EtCore/Content/ResourceManager.h>

#include <EtRHI/Util/PrimitiveRenderer.h>

#include <EtRendering/GlobalRenderingSystems/GlobalRenderingSystems.h>
#include <EtRendering/SceneStructure/RenderScene.h>
#include <EtRendering/GraphicsTypes/EnvironmentMap.h>
#include <EtRendering/MaterialSystem/MaterialData.h>
#include <EtRendering/PlanetTech/StarField.h>


namespace et {
namespace render {


// reflection
//////////////
RTTR_REGISTRATION
{
	rttr::registration::enumeration<E_RenderMode>("E_RenderMode") (
		rttr::value("Shaded", E_RenderMode::Shaded),
		rttr::value("Wireframe", E_RenderMode::Wireframe));
}


//=======================
// Shaded Scene Renderer
//=======================


//---------------------------------
// ShadedSceneRenderer::GetCurrent
//
// Utility function to retrieve the scene renderer for the currently active viewport
//
ShadedSceneRenderer* ShadedSceneRenderer::GetCurrent()
{
	rhi::Viewport* const viewport = rhi::Viewport::GetCurrentViewport();
	if (viewport == nullptr)
	{
		return nullptr;
	}

	rhi::I_ViewportRenderer* const viewRenderer = viewport->GetViewportRenderer();
	if (viewRenderer == nullptr)
	{
		return nullptr;
	}

	ET_ASSERT(viewRenderer->GetType() == rttr::type::get<render::ShadedSceneRenderer>());

	return static_cast<ShadedSceneRenderer*>(viewRenderer);
}

//--------------------------------------------------------------------------


//---------------------------------
// ShadedSceneRenderer::c-tor
//
ShadedSceneRenderer::ShadedSceneRenderer(render::Scene* const renderScene)
	: I_ViewportRenderer()
	, m_RenderScene(renderScene)
{ }

//---------------------------------
// ShadedSceneRenderer::d-tor
//
// make sure all the singletons this system requires are uninitialized
//
ShadedSceneRenderer::~ShadedSceneRenderer()
{
	RenderingSystems::RemoveReference();
}

//-------------------------------------------
// ShadedSceneRenderer::InitRenderingSystems
//
// Create required buffers and subrendering systems etc
//
void ShadedSceneRenderer::InitRenderingSystems()
{
	RenderingSystems::AddReference();

	m_ShadowRenderer.Initialize();
	m_PostProcessing.Initialize();

	m_GBuffer.Initialize();
	m_GBuffer.Enable(true);

	m_SSR.Initialize();

	m_ClearColor = vec3(200.f / 255.f, 114.f / 255.f, 200.f / 255.f)*0.0f;

	m_SkyboxShader = core::ResourceManager::Instance()->GetAssetData<rhi::ShaderData>(core::HashString("Shaders/FwdSkyboxShader.glsl"));

#if ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)
	m_DebugRenderer.Initialize();
#endif

	m_IsInitialized = true;
}

//---------------------------------
// ShadedSceneRenderer::OnResize
//
void ShadedSceneRenderer::OnResize(ivec2 const dim)
{
	m_Dimensions = dim;

	if (!m_IsInitialized)
	{
		return;
	}

	m_PostProcessing.~PostProcessingRenderer();
	new(&m_PostProcessing) PostProcessingRenderer();
	m_PostProcessing.Initialize();

	m_SSR.~ScreenSpaceReflections();
	new(&m_SSR) ScreenSpaceReflections();
	m_SSR.Initialize();
}

//---------------------------------
// ShadedSceneRenderer::OnRender
//
// Main scene drawing function
//
void ShadedSceneRenderer::OnRender(rhi::T_FbLoc const targetFb)
{
	rhi::I_RenderDevice* const device = rhi::ContextHolder::GetRenderDevice();

	// Global variables for all rendering systems
	//********************************************
	Camera const& camera = GetCamera();
	RenderingSystems::Instance()->GetSharedVarController().UpdataData(camera);

	//Shadow Mapping
	//**************
	device->DebugPushGroup("shadow map generation");

	device->SetDepthEnabled(true);
	device->SetCullEnabled(true);

	auto lightIt = m_RenderScene->GetDirectionalLightsShaded().begin();
	auto shadowIt = m_RenderScene->GetDirectionalShadowData().begin();
	while ((lightIt != m_RenderScene->GetDirectionalLightsShaded().end()) && (shadowIt != m_RenderScene->GetDirectionalShadowData().end()))
	{
		mat4 const& transform = m_RenderScene->GetNodes()[lightIt->m_NodeId];
		m_ShadowRenderer.MapDirectional(transform, *shadowIt, this);

		lightIt++;
		shadowIt++;
	}

	device->DebugPopGroup();

	//Deferred Rendering
	//******************
	device->DebugPushGroup("deferred render pass");
	//Step one: Draw the data onto gBuffer
	m_GBuffer.Enable();

	//reset viewport
	device->SetViewport(ivec2(0), m_Dimensions);

	device->DebugPushGroup("clear previous pass");
	device->SetClearColor(vec4(m_ClearColor, 1.f));
	device->Clear(rhi::E_ClearFlag::CF_Color | rhi::E_ClearFlag::CF_Depth);
	device->DebugPopGroup();

	// fill mode
	rhi::E_PolygonMode const polyMode = Get3DPolyMode();
	device->SetPolygonMode(rhi::E_FaceCullMode::FrontBack, polyMode);

	// draw terrains
	device->DebugPushGroup("terrains");
	device->SetCullEnabled(false);
	Patch& patch = RenderingSystems::Instance()->GetPatch();
	for (Planet& planet : m_RenderScene->GetTerrains())
	{
		if (planet.GetTriangulator().Update(m_RenderScene->GetNodes()[planet.GetNodeId()], camera))
		{
			planet.GetTriangulator().GenerateGeometry();
		}

		//Bind patch instances
		patch.BindInstances(planet.GetTriangulator().GetPositions());
		patch.UploadDistanceLUT(planet.GetTriangulator().GetDistanceLUT());
		patch.Draw(planet, m_RenderScene->GetNodes()[planet.GetNodeId()]);
	}
	device->DebugPopGroup();

	// render opaque objects to GBuffer
	device->DebugPushGroup("opaque objects");
	device->SetCullEnabled(true);
	DrawMaterialCollectionGroup(m_RenderScene->GetOpaqueRenderables());
	device->DebugPopGroup();

	device->DebugPushGroup("extensions");
	m_Events.Notify(E_RenderEvent::RE_RenderDeferred, new RenderEventData(this, m_GBuffer.Get()));
	device->DebugPopGroup();

	device->SetPolygonMode(rhi::E_FaceCullMode::FrontBack, rhi::E_PolygonMode::Fill);

	device->DebugPopGroup();

	device->DebugPushGroup("lighting pass");
	// render ambient IBL
	device->DebugPushGroup("image based lighting");
	device->SetFaceCullingMode(rhi::E_FaceCullMode::Back);
	device->SetCullEnabled(false);

	m_SSR.EnableInput();
	m_GBuffer.Draw();
	device->DebugPopGroup();

	//copy Z-Buffer from gBuffer
	device->DebugPushGroup("blit");
	device->BindReadFramebuffer(m_GBuffer.Get());
	device->BindDrawFramebuffer(m_SSR.GetTargetFBO());
	device->CopyDepthReadToDrawFbo(m_Dimensions, m_Dimensions);
	device->DebugPopGroup();

	// Render Light Volumes
	device->DebugPushGroup("light volumes");
	device->SetDepthEnabled(false);
	device->SetBlendEnabled(true);
	device->SetBlendEquation(rhi::E_BlendEquation::Add);
	device->SetBlendFunction(rhi::E_BlendFactor::One, rhi::E_BlendFactor::One);

	device->SetCullEnabled(true);
	device->SetFaceCullingMode(rhi::E_FaceCullMode::Front);

	// pointlights
	device->DebugPushGroup("point lights");
	for (Light const& pointLight : m_RenderScene->GetPointLights())
	{
		mat4 const& transform = m_RenderScene->GetNodes()[pointLight.m_NodeId];
		float const scale = math::length(math::decomposeScale(transform));
		vec3 const pos = math::decomposePosition(transform);

		RenderingSystems::Instance()->GetPointLightVolume().Draw(pos, scale, pointLight.m_Color, m_GBuffer);
	}
	device->DebugPopGroup();
	
	// direct
	device->DebugPushGroup("directional lights");
	for (Light const& dirLight : m_RenderScene->GetDirectionalLights())
	{
		mat4 const& transform = m_RenderScene->GetNodes()[dirLight.m_NodeId];
		vec3 const dir = (transform * vec4(vec3::FORWARD, 1.f)).xyz;
		RenderingSystems::Instance()->GetDirectLightVolume().Draw(dir, dirLight.m_Color, m_GBuffer);
	}
	device->DebugPopGroup();

	// direct with shadow
	device->DebugPushGroup("directional lights shadowed");
	lightIt = m_RenderScene->GetDirectionalLightsShaded().begin();
	shadowIt = m_RenderScene->GetDirectionalShadowData().begin();
	while ((lightIt != m_RenderScene->GetDirectionalLightsShaded().end()) && (shadowIt != m_RenderScene->GetDirectionalShadowData().end()))
	{
		Light const& dirLight = *lightIt;
		DirectionalShadowData const& shadow = *shadowIt;

		mat4 const& transform = m_RenderScene->GetNodes()[dirLight.m_NodeId];
		vec3 const dir = (transform * vec4(vec3::FORWARD, 1.f)).xyz;

		RenderingSystems::Instance()->GetDirectLightVolume().DrawShadowed(dir, dirLight.m_Color, shadow, m_GBuffer);

		lightIt++;
		shadowIt++;
	}
	device->DebugPopGroup();

	device->SetFaceCullingMode(rhi::E_FaceCullMode::Back);
	device->SetBlendEnabled(false);

	device->SetCullEnabled(false);
	device->DebugPopGroup(); // light volumes

	device->DebugPushGroup("extensions");
	m_Events.Notify(E_RenderEvent::RE_RenderLights, new RenderEventData(this, m_SSR.GetTargetFBO()));
	device->DebugPopGroup(); 

	// draw SSR
	device->DebugPushGroup("reflections");
	m_PostProcessing.EnableInput();
	m_SSR.Draw(m_GBuffer);
	device->DebugPopGroup();
	// copy depth again
	device->DebugPushGroup("blit");
	device->BindReadFramebuffer(m_SSR.GetTargetFBO());
	device->BindDrawFramebuffer(m_PostProcessing.GetTargetFBO());
	device->CopyDepthReadToDrawFbo(m_Dimensions, m_Dimensions);
	device->DebugPopGroup();

	device->DebugPopGroup(); // lighting

	//Forward Rendering
	//******************
	device->DebugPushGroup("forward render pass");
	device->SetDepthEnabled(true);

	device->SetPolygonMode(rhi::E_FaceCullMode::FrontBack, polyMode);

	// draw skybox
	device->DebugPushGroup("skybox");
	Skybox const& skybox = m_RenderScene->GetSkybox();
	if (skybox.m_EnvironmentMap != nullptr)
	{
		device->SetShader(m_SkyboxShader.get());
		m_SkyboxShader->Upload("skybox"_hash, skybox.m_EnvironmentMap->GetRadiance());

		m_SkyboxShader->Upload("numMipMaps"_hash, skybox.m_EnvironmentMap->GetNumMipMaps());
		m_SkyboxShader->Upload("roughness"_hash, skybox.m_Roughness);

		device->SetDepthFunction(rhi::E_DepthFunc::LEqual);
		rhi::PrimitiveRenderer::Instance().Draw<rhi::primitives::Cube>();
	}
	device->DebugPopGroup();

	// draw stars
	device->DebugPushGroup("stars");
	StarField const* const starfield = m_RenderScene->GetStarfield();
	if (starfield != nullptr)
	{
		starfield->Draw(camera);
	}
	device->DebugPopGroup();

	// forward rendering
	device->DebugPushGroup("forward renderables");
	device->SetCullEnabled(true);
	DrawMaterialCollectionGroup(m_RenderScene->GetForwardRenderables());
	device->DebugPopGroup();

	device->DebugPushGroup("extensions");
	m_Events.Notify(E_RenderEvent::RE_RenderForward, new RenderEventData(this, m_PostProcessing.GetTargetFBO()));
	device->DebugPopGroup();
	
	// draw atmospheres
	device->DebugPushGroup("atmospheres");
	if ((m_RenderScene->GetAtmosphereInstances().size() > 0u)
#if ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)
		&& !RenderingSystems::Instance()->GetDebugVars().AtmospheresHidden()
#endif
		)
	{
		device->SetFaceCullingMode(rhi::E_FaceCullMode::Front);
		device->SetDepthEnabled(false);
		device->SetBlendEnabled(true);
		device->SetBlendEquation(rhi::E_BlendEquation::Add);
		device->SetBlendFunction(rhi::E_BlendFactor::One, rhi::E_BlendFactor::One);

		for (AtmosphereInstance const& atmoInst : m_RenderScene->GetAtmosphereInstances())
		{
			vec3 const pos = math::decomposePosition(m_RenderScene->GetNodes()[atmoInst.nodeId]);

			ET_ASSERT(atmoInst.lightId != core::INVALID_SLOT_ID);
			Light const& sun = m_RenderScene->GetLight(atmoInst.lightId);
			vec3 const sunDir = math::normalize((m_RenderScene->GetNodes()[sun.m_NodeId] * vec4(vec3::FORWARD, 1.f)).xyz);

			m_RenderScene->GetAtmosphere(atmoInst.atmosphereId).Draw(pos, atmoInst.height, atmoInst.groundRadius, sunDir, m_GBuffer);
		}

		device->SetFaceCullingMode(rhi::E_FaceCullMode::Back);
		device->SetBlendEnabled(false);
		device->SetDepthEnabled(true);
	}
	device->DebugPopGroup();

	device->SetPolygonMode(rhi::E_FaceCullMode::FrontBack, rhi::E_PolygonMode::Fill);

	device->DebugPopGroup(); // forward render pass

	// Post Scene rendering
	device->DebugPushGroup("post processing pass");

	device->DebugPushGroup("extensions");
	m_Events.Notify(E_RenderEvent::RE_RenderWorldGUI, new RenderEventData(this, m_PostProcessing.GetTargetFBO()));
	device->DebugPopGroup(); // extensions

	// debug stuff
#if ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)
	device->DebugPushGroup("debug");

	DrawDebugVisualizations();
	m_DebugRenderer.Draw(camera);

	device->DebugPopGroup(); // debug
#endif

	// post processing
	device->SetCullEnabled(false);
	m_PostProcessing.Draw(targetFb, GetPostProcessingSettings(), 
		std::function<void(rhi::T_FbLoc const)>([this, device](rhi::T_FbLoc const targetFb)
		{
			device->DebugPushGroup("overlay extensions");
			m_Events.Notify(E_RenderEvent::RE_RenderOverlay, new RenderEventData(this, targetFb));
			device->DebugPopGroup(); // overlay extensions
		}));

	device->DebugPopGroup(); // post processing pass
}

//--------------------------------
// ShadedSceneRenderer::GetCamera
//
Camera const& ShadedSceneRenderer::GetCamera() const
{
	return m_RenderScene->GetCameras()[m_CameraId];
}

//---------------------------------
// ShadedSceneRenderer::DrawShadow
//
// Render the scene to the depth buffer of the current framebuffer
//
void ShadedSceneRenderer::DrawShadow(I_Material const* const nullMaterial)
{
	rhi::I_RenderDevice* const device = rhi::ContextHolder::GetRenderDevice();

	// No need to set shaders or upload material parameters as that is the calling functions responsibility
	for (MaterialCollection::Mesh const& mesh : m_RenderScene->GetShadowCasters().m_Meshes)
	{
		device->BindVertexArray(mesh.m_VAO);
		for (T_NodeId const node : mesh.m_Instances)
		{
			// #todo: collect a list of transforms and draw this instanced
			mat4 const& transform = m_RenderScene->GetNodes()[node];
			math::Sphere instSphere = math::Sphere((transform * vec4(mesh.m_BoundingVolume.pos, 1.f)).xyz,
				math::length(math::decomposeScale(transform)) * mesh.m_BoundingVolume.radius);

			if (true) // #todo: light frustum check
			{
				nullMaterial->GetBaseMaterial()->GetShader()->Upload("model"_hash, transform);
				device->DrawElements(rhi::E_DrawMode::Triangles, mesh.m_IndexCount, mesh.m_IndexDataType, 0);
			}
		}
	}
}

//------------------------------------
// ShadedSceneRenderer::Get3DPolyMode
//
// What poly mode to use for 3D rendering based on the render mode
//
rhi::E_PolygonMode ShadedSceneRenderer::Get3DPolyMode() const
{
	E_RenderMode renderMode = m_RenderMode;
#if ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)
	RenderingSystems::Instance()->GetDebugVars().OverrideMode(renderMode);
#endif

	return (renderMode == E_RenderMode::Wireframe) ? rhi::E_PolygonMode::Line : rhi::E_PolygonMode::Fill;
}

//------------------------------------------------
// ShadedSceneRenderer::GetPostProcessingSettings
//
// For wireframe rendering we override post processing settings
//
PostProcessingSettings const& ShadedSceneRenderer::GetPostProcessingSettings()
{
	E_RenderMode renderMode = m_RenderMode;
#if ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)
	RenderingSystems::Instance()->GetDebugVars().OverrideMode(renderMode);
#endif

	if (renderMode == E_RenderMode::Shaded)
	{
		return m_RenderScene->GetPostProcessingSettings();
	}

	m_OverrideSettings = m_RenderScene->GetPostProcessingSettings();
	m_OverrideSettings.bloomMult = 0.f;
	m_OverrideSettings.exposure *= 5.f;
	return m_OverrideSettings;
}

//--------------------------------------------------
// ShadedSceneRenderer::DrawMaterialCollectionGroup
//
// Draws all meshes in a list of shaders
//
void ShadedSceneRenderer::DrawMaterialCollectionGroup(core::slot_map<MaterialCollection> const& collectionGroup)
{
	rhi::I_RenderDevice* const device = rhi::ContextHolder::GetRenderDevice();

	Camera const& camera = GetCamera();

	for (MaterialCollection const& collection : collectionGroup)
	{
		device->SetShader(collection.m_Shader.get());
		for (MaterialCollection::MaterialInstance const& material : collection.m_Materials)
		{
			ET_ASSERT(collection.m_Shader.get() == material.m_Material->GetBaseMaterial()->GetShader());

			collection.m_Shader->UploadParameterBlock(material.m_Material->GetParameters());
			for (MaterialCollection::Mesh const& mesh : material.m_Meshes)
			{
				device->BindVertexArray(mesh.m_VAO);
				for (T_NodeId const node : mesh.m_Instances)
				{
					// #todo: collect a list of transforms and draw this instanced
					mat4 const& transform = m_RenderScene->GetNodes()[node];
					math::Sphere instSphere = math::Sphere((transform * vec4(mesh.m_BoundingVolume.pos, 1.f)).xyz,
						math::length(math::decomposeScale(transform)) * mesh.m_BoundingVolume.radius);

					if (camera.GetFrustum().ContainsSphere(instSphere) != VolumeCheck::OUTSIDE)
					{
						collection.m_Shader->Upload("model"_hash, transform);
						device->DrawElements(rhi::E_DrawMode::Triangles, mesh.m_IndexCount, mesh.m_IndexDataType, 0);
					}
				}
			}
		}
	}
}

#if ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)

//--------------------------------------------------
// ShadedSceneRenderer::DrawDebugVisualizations
//
void ShadedSceneRenderer::DrawDebugVisualizations()
{
	// Debug visualization for where the camera frustum was when it was frozen
	if (RenderingSystems::Instance()->GetDebugVars().IsFrustumFrozen())
	{
		FrustumCorners const& corners = GetCamera().GetFrustum().GetCorners();

		vec4 const lineCol(vec3(10.f), 1.f);

		m_DebugRenderer.DrawLine(corners.na, corners.nb, lineCol);
		m_DebugRenderer.DrawLine(corners.nd, corners.nb, lineCol);
		m_DebugRenderer.DrawLine(corners.nd, corners.nc, lineCol);
		m_DebugRenderer.DrawLine(corners.na, corners.nc, lineCol);

		m_DebugRenderer.DrawLine(corners.fa, corners.fb, lineCol);
		m_DebugRenderer.DrawLine(corners.fd, corners.fb, lineCol);
		m_DebugRenderer.DrawLine(corners.fd, corners.fc, lineCol);
		m_DebugRenderer.DrawLine(corners.fa, corners.fc, lineCol);

		m_DebugRenderer.DrawLine(corners.na, corners.fa, lineCol);
		m_DebugRenderer.DrawLine(corners.nb, corners.fb, lineCol);
		m_DebugRenderer.DrawLine(corners.nc, corners.fc, lineCol);
		m_DebugRenderer.DrawLine(corners.nd, corners.fd, lineCol);
	}
}

#endif // ET_CT_IS_ENABLED(ET_CT_DBG_UTIL)


} // namespace render
} // namespace et
