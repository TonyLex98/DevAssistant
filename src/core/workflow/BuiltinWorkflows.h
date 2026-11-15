#pragma once
#include "core/workflow/PromptSequence.h"

class SolutionTemplateManager;

// The sequences and templates the program ships with.
namespace BuiltinWorkflows {

// "Default": clarify -> collect minimal context -> draft edits -> questions -> final commands.
PromptSequence defaultSequence();

// Writes the "Universal Agent" template (analysis, context, code generation, testing) into the
// templates folder, unless the same version is already there.
bool installUniversalAgent(SolutionTemplateManager& templates);

} // namespace BuiltinWorkflows
