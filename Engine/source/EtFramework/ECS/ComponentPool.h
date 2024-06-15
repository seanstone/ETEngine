#pragma once
#include "ComponentRegistry.h"


namespace et {
namespace fw {


//---------------
// ComponentPool
//
// Contains a dynamic list of components of a certain type, with the actual type being erased
//
class ComponentPool final
{
	// construct destruct
	//--------------------
public:
	ComponentPool(T_CompTypeIdx const typeIdx) : m_ComponentType(typeIdx) {}

	// accessors
	//-----------
	template<typename TComponentType>
	TComponentType& Get(size_t const idx);
	template<typename TComponentType>
	TComponentType const& Get(size_t const idx) const;

	void* At(size_t const idx);
	void const* At(size_t const idx) const;

	size_t GetSize() const;
	T_CompTypeIdx GetType() const { return m_ComponentType; }

	// functionality
	//---------------
	template<typename TComponentType>
	void Append(TComponentType const& component); // add to back
	void Append2(void const* const componentData);

	void Erase(size_t const idx); // swap with last element and pop_back
	void Clear();

	// Data
	///////

private:
	T_CompTypeIdx const m_ComponentType;
	std::vector<uint8> m_Buffer;
};


} // namespace fw
} // namespace et


#include "ComponentPool.inl"
