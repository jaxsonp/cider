#pragma once

#include <map>
#include <string>

#include "backend/Target.hpp"

/// @brief An artifact the compiler can be asked to produce.
/// Declaration order matches pipeline order — the compiler runs only as far as
/// the furthest kind it was asked for.
enum class EmitKind
{
	Ast,
	Ir,
	Asm,
	Exe,
};

/// @brief Where one emitted artifact should be written
struct EmitTarget
{
	/// Output path, unused when writing to stdout
	std::string path;
	bool to_stdout = false;
};

struct CompileSettings
{
	Target target;
	/// Artifacts to produce, keyed by kind (so lookups and "furthest stage" stay cheap)
	std::map<EmitKind, EmitTarget> emits;

	explicit CompileSettings(Target target) : target(target) {}

	/// @brief Whether the given artifact was requested
	bool wants(EmitKind kind) const { return this->emits.contains(kind); }

	/// @brief The latest pipeline stage that was requested. Throws if nothing was requested
	EmitKind furthest() const;
};

void compile(const std::string &, const CompileSettings &);
