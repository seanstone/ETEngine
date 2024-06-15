#pragma once


namespace et {
namespace fw {


//=================================
// ECS Controller inline functions
//=================================


namespace detail {

	//-------------------------------
	// EcsController::AddToEcs
	//
	// variadic template recursively adds components to a raw component pointer list and finally create an entity from that component list
	// adding the entity in the last recursion call prevents components from going out of scope for some reason
	//
	template<typename TComponentType>
	T_EntityId AddToEcs(EcsController& ecs, T_EntityId const parent, std::vector<RawComponentPtr>& list, TComponentType& component)
	{
		list.emplace_back(MakeRawComponent(component));
		return ecs.AddEntityBatched(parent, list);
	}

	template<typename TComponentType, typename... Args>
	T_EntityId AddToEcs(EcsController& ecs, T_EntityId const parent, std::vector<RawComponentPtr>& list, TComponentType& component1, Args... args)
	{
		list.emplace_back(MakeRawComponent(component1));
		return AddToEcs(ecs, parent, list, args...);
	}

	//-------------------------------
	// EcsController::DupeInEcs
	//
	// variadic template recursively adds components to a raw component pointer list and finallyduplicate an entity with that component list
	//
	template<typename TComponentType>
	T_EntityId DupeInEcs(EcsController& ecs, T_EntityId const dupe, std::vector<RawComponentPtr>& list, TComponentType& component)
	{
		list.emplace_back(MakeRawComponent(component));
		return ecs.DuplicateEntityAddComponents(dupe, list);
	}

	template<typename TComponentType, typename... Args>
	T_EntityId DupeInEcs(EcsController& ecs, T_EntityId const dupe, std::vector<RawComponentPtr>& list, TComponentType& component1, Args... args)
	{
		list.emplace_back(MakeRawComponent(component1));
		return DupeInEcs(ecs, dupe, list, args...);
	}

} // namespace detail


//--------------------------
// EcsController::AddEntity
//
template<typename TComponentType, typename... Args>
T_EntityId EcsController::AddEntity(TComponentType& component1, Args... args)
{
	return AddEntityChild(INVALID_ENTITY_ID, component1, args...);
}

template<typename TComponentType, typename... Args>
T_EntityId EcsController::AddEntity(TComponentType&& component1, Args... args)
{
	auto comp = component1;
	return AddEntity(comp, args...);
}

//-------------------------------
// EcsController::AddEntityChild
//
template<typename TComponentType, typename... Args>
T_EntityId EcsController::AddEntityChild(T_EntityId const parent, TComponentType& component1, Args... args)
{
	std::vector<RawComponentPtr> components;
	return detail::AddToEcs(*this, parent, components, component1, args...);
}

template<typename TComponentType, typename... Args>
T_EntityId EcsController::AddEntityChild(T_EntityId const parent, TComponentType&& component1, Args... args)
{
	auto comp = component1;
	return AddEntityChild(parent, comp, args...);
}

//--------------------------------
// EcsController::DuplicateEntity
//
template<typename TComponentType, typename... Args>
T_EntityId EcsController::DuplicateEntity(T_EntityId const dupe, TComponentType& component1, Args... args)
{
	std::vector<RawComponentPtr> components;
	return detail::DupeInEcs(*this, dupe, components, component1, args...);
}

template<typename TComponentType, typename... Args>
T_EntityId EcsController::DuplicateEntity(T_EntityId const dupe, TComponentType&& component1, Args... args)
{
	auto comps = component1;
	return DuplicateEntity(dupe, comps, args...);
}

//------------------------------
// EcsController::AddComponents
//
template<typename TComponentType, typename... Args>
void EcsController::AddComponents(T_EntityId const entity, TComponentType& component1, Args... args)
{
	std::vector<RawComponentPtr> components;
	detail::GenCompPtrList(components, component1, args...);

	AddComponents(entity, components);
}

template<typename TComponentType, typename... Args>
void EcsController::AddComponents(T_EntityId const entity, TComponentType&& component1, Args... args)
{
	auto comp = component1;
	AddComponents(entity, component1, args...);
}

//---------------------------------
// EcsController::RemoveComponents
//
template<typename TComponentType, typename... Args>
void EcsController::RemoveComponents(T_EntityId const entity)
{
	RemoveComponents(entity, GenCompTypeList<TComponentType, Args...>());
}

//-----------------------------------------
// EcsController::RegisterOnComponentAdded
//
template<typename TComponentType>
T_CompEventId EcsController::RegisterOnComponentAdded(T_CompEventFn<TComponentType> const& fn)
{
	return m_ComponentEvents[TComponentType::GetTypeIndex()].Register(detail::E_EcsEvent::Added, detail::T_ComponentEventCallbackInternal(
		[this, fn](detail::T_EcsEvent const flags, detail::ComponentEventData const* const evnt) -> void
		{
			ET_UNUSED(flags);
			fn(*evnt->controller, *static_cast<TComponentType*>(evnt->component), evnt->entity);
		}));
}

//-------------------------------------------
// EcsController::RegisterOnComponentRemoved
//
// Deinit components
//
template<typename TComponentType>
T_CompEventId EcsController::RegisterOnComponentRemoved(T_CompEventFn<TComponentType> const& fn)
{
	return m_ComponentEvents[TComponentType::GetTypeIndex()].Register(detail::E_EcsEvent::Removed, detail::T_ComponentEventCallbackInternal(
		[this, fn](detail::T_EcsEvent const flags, detail::ComponentEventData const* const evnt) -> void
		{
			ET_UNUSED(flags);
			fn(*evnt->controller, *static_cast<TComponentType*>(evnt->component), evnt->entity);
		}));
}

//-----------------------------------------
// EcsController::UnregisterComponentEvent
//
// Not required to be called unless the event is a member function that gets deleted before the ecs controller does
//
template<typename TComponentType>
void EcsController::UnregisterComponentEvent(T_CompEventId& callbackId)
{
	m_ComponentEvents[TComponentType::GetTypeIndex()].Unregister(callbackId);
}

//-----------------------------
// EcsController::HasComponent
//
template<typename TComponentType>
bool EcsController::HasComponent(T_EntityId const entity) const
{
	return HasComponent(entity, TComponentType::GetTypeIndex());
}

//-----------------------------
// EcsController::GetComponent
//
template<typename TComponentType>
TComponentType& EcsController::GetComponent(T_EntityId const entity)
{
	return *static_cast<TComponentType*>(GetComponentData(entity, TComponentType::GetTypeIndex()));
}

//-----------------------------
// EcsController::GetComponent
//
template<typename TComponentType>
TComponentType const& EcsController::GetComponent(T_EntityId const entity) const
{
	return *static_cast<TComponentType const*>(GetComponentData(entity, TComponentType::GetTypeIndex()));
}

//-------------------------------
// EcsController::RegisterSystem
//
// Add the system to the update graph - it will iterate over all entities that have components matching its view
//
template<typename TSystemType, typename... Args>
void EcsController::RegisterSystem(Args... args)
{
	ET_ASSERT(!IsSystemRegistered<TSystemType>());
	RegisterSystemInternal(new TSystemType(args...));
}

//---------------------------------
// EcsController::UnregisterSystem
//
// Remove the system from the update graph
//
template<typename TSystemType>
void EcsController::UnregisterSystem()
{
	ET_ASSERT(IsSystemRegistered<TSystemType>());
	UnregisterSystemInternal(rttr::type::get<TSystemType>().get_id());
}

//-----------------------------------
// EcsController::IsSystemRegistered
//
template<typename TSystemType>
bool EcsController::IsSystemRegistered() const
{
	T_SystemType const typeId = rttr::type::get<TSystemType>().get_id();
	return (std::find_if(m_Systems.cbegin(), m_Systems.cend(), [typeId](RegisteredSystem const* const sys) 
		{ 
			return (sys->system->GetTypeId() == typeId); 
		}) != m_Systems.cend());
}

//---------------------------------
// EcsController::UnregisterSystem
//
// Remove the system from the update graph
//
template<typename TViewType>
void EcsController::ProcessViewOneShot(T_OneShotProcess<TViewType> const& processFn)
{
	ComponentSignature const signature = SignatureFromView<TViewType>();
	for (ArchetypeContainer& hierarchyLevel : m_HierachyLevels)
	{
		for (std::pair<T_Hash const, Archetype*>& arch : hierarchyLevel.archetypes)
		{
			// go layer wise to ensure correct hierachy dependency resolution
			if ((arch.second->GetSize() > 0u) && arch.second->GetSignature().Contains(signature))
			{
				processFn(ComponentRange<TViewType>(this, arch.second, 0u, arch.second->GetSize()));
			}
		}
	}
}


} // namespace fw
} // namespace et
