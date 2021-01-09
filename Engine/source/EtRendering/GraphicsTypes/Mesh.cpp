#include "stdafx.h"
#include "Mesh.h"

#include <EtBuild/EngineVersion.h>

#include <EtCore/Content/AssetRegistration.h>
#include <EtCore/Reflection/Registration.h>
#include <EtCore/IO/BinaryReader.h>

#include <EtRendering/MaterialSystem/MaterialData.h>


namespace et {
namespace render {


//==============
// Mesh Surface
//==============


//---------------------------------
// MeshSurface::c-tor
//
// Construct a surface from a mesh and material combination
//
MeshSurface::MeshSurface(MeshData const* const mesh, render::Material const* const material)
	: m_Material(material)
{
	ET_ASSERT(mesh != nullptr);
	ET_ASSERT(m_Material != nullptr);

	I_GraphicsContextApi* const api = ContextHolder::GetRenderContext();

	// create a new vertex array
	m_VertexArray = api->CreateVertexArray();
	api->BindVertexArray(m_VertexArray);

	// link it to the mesh's buffer
	api->BindBuffer(E_BufferType::Vertex, mesh->GetVertexBuffer());
	api->BindBuffer(E_BufferType::Index, mesh->GetIndexBuffer());

	//Specify Input Layout
	AttributeDescriptor::DefineAttributeArray(mesh->GetSupportedFlags(), m_Material->GetLayoutFlags(), m_Material->GetAttributeLocations());

	api->BindVertexArray(0u);
}

//---------------------------------
// MeshSurface::d-tor
//
// Free GPU data
//
MeshSurface::~MeshSurface()
{
	ContextHolder::GetRenderContext()->DeleteVertexArray(m_VertexArray);
}


//===================
// Surface Container
//===================


//---------------------------------
// SurfaceContainer::d-tor
//
// Delete all surface lists
//
SurfaceContainer::~SurfaceContainer()
{
	for (MeshSurface* surface: m_Surfaces)
	{
		SafeDelete(surface);
	}
}

//---------------------------------
// SurfaceContainer::GetSurface
//
// Returns a pointer to a surface for the material in question. If none is found we create it
//
MeshSurface const* SurfaceContainer::GetSurface(MeshData const* const mesh, render::Material const* const material)
{
	// try finding the existing surface
	auto surfaceIt = std::find_if(m_Surfaces.begin(), m_Surfaces.end(), [material](MeshSurface* surface)
		{
			ET_ASSERT(surface != nullptr);
			return (surface->GetMaterial() == material);
		});

	// if it isn't found, create one and set the mesh
	if (surfaceIt == m_Surfaces.cend())
	{
		m_Surfaces.emplace_back(new MeshSurface(mesh, material));
		surfaceIt = std::prev(m_Surfaces.end());
	}

	return *surfaceIt;
}


//==============
// Mesh Data
//==============


//---------------------------------
// MeshData::c-tor
//
MeshData::MeshData()
	: m_Surfaces(new SurfaceContainer())
{ }

//---------------------------------
// MeshData::d-tor
//
// Free the GPU buffers for this mesh
//
MeshData::~MeshData()
{
	I_GraphicsContextApi* const api = ContextHolder::GetRenderContext();

	api->DeleteBuffer(m_VertexBuffer);
	api->DeleteBuffer(m_IndexBuffer);

	delete m_Surfaces;
}

//---------------------------------
// MeshData::GetSurface
//
// Retrieves (and potentially creates) a new surface for the material in question
//
MeshSurface const* MeshData::GetSurface(render::Material const* const material) const
{
	return m_Surfaces->GetSurface(this, material);
}


//===================
// Mesh Asset
//===================


// reflection
RTTR_REGISTRATION
{
	BEGIN_REGISTER_CLASS_ASSET(MeshData, "mesh data")
	END_REGISTER_CLASS(MeshData);

	BEGIN_REGISTER_CLASS(MeshAsset, "mesh asset")
	END_REGISTER_CLASS_POLYMORPHIC(MeshAsset, core::I_Asset);
}
DEFINE_FORCED_LINKING(MeshAsset) // force the shader class to be linked as it is only used in reflection


// static
std::string const MeshAsset::s_Header("ETMESH");


//---------------------------------
// MeshAsset::LoadFromMemory
//
// Load mesh data from binary asset content, and place it on the GPU
//
bool MeshAsset::LoadFromMemory(std::vector<uint8> const& data)
{
	m_Data = new MeshData();

	core::BinaryReader reader;
	reader.Open(data);
	ET_ASSERT(reader.Exists());

	// read header
	//-------------
	if (reader.ReadString(s_Header.size()) != s_Header)
	{
		ET_ASSERT(false, "Incorrect binary mesh file header");
		return false;
	}

	std::string const writerVersion = reader.ReadNullString();
	if (writerVersion != build::Version::s_Name)
	{
		LOG(FS("Mesh data was writter by a different engine version: %s", writerVersion.c_str()));
	}

	// read mesh info
	//----------------
	uint64 const indexCount = reader.Read<uint64>();
	m_Data->m_IndexCount = static_cast<size_t>(indexCount);

	uint64 const vertexCount = reader.Read<uint64>();
	m_Data->m_VertexCount = static_cast<size_t>(vertexCount);

	m_Data->m_IndexDataType = reader.Read<E_DataType>();
	m_Data->m_SupportedFlags = reader.Read<T_VertexFlags>();
	m_Data->m_BoundingSphere.pos = reader.ReadVector<3, float>();
	m_Data->m_BoundingSphere.radius = reader.Read<float>();

	uint64 const iBufferSize = indexCount * static_cast<uint64>(render::DataTypeInfo::GetTypeSize(m_Data->m_IndexDataType));
	uint64 const vBufferSize = vertexCount * static_cast<uint64>(render::AttributeDescriptor::GetVertexSize(m_Data->m_SupportedFlags));

	// setup buffers
	//---------------
	uint8 const* const indexData = reader.GetCurrentDataPointer();
	reader.MoveBufferPosition(static_cast<size_t>(iBufferSize));

	uint8 const* const vertexData = reader.GetCurrentDataPointer();

	I_GraphicsContextApi* const api = ContextHolder::GetRenderContext();

	// vertex buffer
	m_Data->m_VertexBuffer = api->CreateBuffer();
	api->BindBuffer(E_BufferType::Vertex, m_Data->m_VertexBuffer);
	api->SetBufferData(E_BufferType::Vertex, static_cast<int64>(vBufferSize), reinterpret_cast<void const*>(vertexData), E_UsageHint::Static);

	// index buffer 
	m_Data->m_IndexBuffer = api->CreateBuffer();
	api->BindBuffer(E_BufferType::Index, m_Data->m_IndexBuffer);
	api->SetBufferData(E_BufferType::Index, static_cast<int64>(iBufferSize), reinterpret_cast<void const*>(indexData), E_UsageHint::Static);

	return true;
}


} // namespace render
} // namespace et
