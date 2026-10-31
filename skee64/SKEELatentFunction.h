#pragma once

#include "RE/N/NativeLatentFunction.h"
#include <string_view>
#include <type_traits>

// CommonLib 7a60f4de NativeLatentFunction constructs GetRawType<R> without
// invoking it. Keep the dependency unchanged and adapt only the two boolean
// latent registrations used here. Dispatch and argument marshalling remain in
// CommonLib's NativeFunction, as in its own latent wrapper.
namespace SKEE::detail
{
	template <class Base, class... Args>
	class NativeLatentBoolFunction final : public RE::BSScript::NativeFunction<
		true,
		RE::BSScript::LatentStatus(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, Base, Args...),
		bool, Base, Args...>
	{
		using Super = RE::BSScript::NativeFunction<
			true,
			RE::BSScript::LatentStatus(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, Base, Args...),
			bool, Base, Args...>;

	public:
		using Callback = typename Super::function_type;

		NativeLatentBoolFunction(std::string_view a_name, std::string_view a_className, Callback a_callback) :
			Super(a_name, a_className, a_callback)
		{
			this->_retType = RE::BSScript::GetRawType<bool>{}();
			this->_isLatent = true;
		}
	};
}

template <class Base, class... Args>
inline void SKEE_RegisterLatentBoolFunction(
	RE::BSScript::IVirtualMachine* a_vm, std::string_view a_name, std::string_view a_className,
	RE::BSScript::LatentStatus (*a_callback)(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, Base, Args...))
{
	static_assert(std::is_same_v<std::underlying_type_t<RE::BSScript::LatentStatus>, bool>);
	static_assert(RE::BSScript::is_valid_latent_long_sig_v<bool, RE::VMStackID, RE::BSScript::LatentStatus, Base, Args...>);
	// Preserve RegisterLatentFunction's default: not callable from tasklets.
	a_vm->BindNativeMethod(new SKEE::detail::NativeLatentBoolFunction<Base, Args...>(a_name, a_className, a_callback));
}
