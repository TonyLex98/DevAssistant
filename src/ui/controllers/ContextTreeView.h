#pragma once

class ContextManager;
class QTreeWidget;

// Shows the command history in a tree: sequences with their commands, each command colored
// by its state (in context, invalidated, removed) with its result folded underneath.
namespace ContextTreeView {
void populate(QTreeWidget* tree, const ContextManager& history);
}
