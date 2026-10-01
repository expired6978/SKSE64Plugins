#pragma once
#include <string>
namespace RE { class GFxMovie; class GFxValue; }

namespace SKEE::MenuAppearance
{
	void Configure(const std::string& background, const std::string& text, const std::string& runtimeDirectory);
	void Register(RE::GFxMovie* view, RE::GFxValue* root);
}
