#pragma once

#include <EtCore/Input/KeyCodes.h>

namespace et {
namespace edit {

namespace GtkUtil {

	E_MouseButton GetButtonFromGtk(uint32 const buttonCode);
	E_KbdKey GetKeyFromGtk(uint32 const keyCode);
	core::T_KeyModifierFlags GetModifiersFromGtk(uint32 const modifierState);

} // namespace GtkUtil

} // namespace edit
} // namespace et

