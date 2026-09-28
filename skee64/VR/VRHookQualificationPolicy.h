#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

namespace SKEE::VR::HookQualification
{
	enum class EntryBranchKind
	{
		kRelativeJump,
		kShortJump,
		kRipIndirectJump,
		kAbsoluteRaxJump,
		kAbsoluteR11Jump
	};

	struct EntryBranch
	{
		EntryBranchKind kind;
		std::uintptr_t target;
	};

	[[nodiscard]] inline bool AddSignedOffset(
		std::uintptr_t a_base,
		std::int64_t a_offset,
		std::uintptr_t& a_result) noexcept
	{
		if (a_offset >= 0) {
			const auto offset = static_cast<std::uintptr_t>(a_offset);
			if (offset > std::numeric_limits<std::uintptr_t>::max() - a_base) {
				return false;
			}
			a_result = a_base + offset;
			return true;
		}

		const auto magnitude = static_cast<std::uintptr_t>(-(a_offset + 1)) + 1;
		if (magnitude > a_base) {
			return false;
		}
		a_result = a_base - magnitude;
		return true;
	}

	template <class ReadPointer>
	[[nodiscard]] std::optional<EntryBranch> DecodeEntryBranch(
		std::uintptr_t a_entry,
		std::span<const std::uint8_t> a_bytes,
		ReadPointer&& a_readPointer) noexcept
	{
		if (a_bytes.size() >= 5 && a_bytes[0] == 0xE9) {
			std::int32_t displacement{};
			std::memcpy(&displacement, a_bytes.data() + 1, sizeof(displacement));
			std::uintptr_t target{};
			std::uintptr_t nextInstruction{};
			if (AddSignedOffset(a_entry, 5, nextInstruction) &&
				AddSignedOffset(nextInstruction, displacement, target)) {
				return EntryBranch{ EntryBranchKind::kRelativeJump, target };
			}
		}

		if (a_bytes.size() >= 2 && a_bytes[0] == 0xEB) {
			const auto displacement = static_cast<std::int8_t>(a_bytes[1]);
			std::uintptr_t target{};
			std::uintptr_t nextInstruction{};
			if (AddSignedOffset(a_entry, 2, nextInstruction) &&
				AddSignedOffset(nextInstruction, displacement, target)) {
				return EntryBranch{ EntryBranchKind::kShortJump, target };
			}
		}

		if (a_bytes.size() >= 6 && a_bytes[0] == 0xFF && a_bytes[1] == 0x25) {
			std::int32_t displacement{};
			std::memcpy(&displacement, a_bytes.data() + 2, sizeof(displacement));
			std::uintptr_t slot{};
			std::uintptr_t target{};
			std::uintptr_t nextInstruction{};
			if (AddSignedOffset(a_entry, 6, nextInstruction) &&
				AddSignedOffset(nextInstruction, displacement, slot) &&
				a_readPointer(slot, target)) {
				return EntryBranch{ EntryBranchKind::kRipIndirectJump, target };
			}
		}

		if (a_bytes.size() >= 12 && a_bytes[0] == 0x48 && a_bytes[1] == 0xB8 &&
			a_bytes[10] == 0xFF && a_bytes[11] == 0xE0) {
			std::uintptr_t target{};
			std::memcpy(&target, a_bytes.data() + 2, sizeof(target));
			return EntryBranch{ EntryBranchKind::kAbsoluteRaxJump, target };
		}

		if (a_bytes.size() >= 13 && a_bytes[0] == 0x49 && a_bytes[1] == 0xBB &&
			a_bytes[10] == 0x41 && a_bytes[11] == 0xFF && a_bytes[12] == 0xE3) {
			std::uintptr_t target{};
			std::memcpy(&target, a_bytes.data() + 2, sizeof(target));
			return EntryBranch{ EntryBranchKind::kAbsoluteR11Jump, target };
		}

		return std::nullopt;
	}
}
