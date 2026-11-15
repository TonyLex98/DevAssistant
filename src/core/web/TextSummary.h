#pragma once
#include <QString>

namespace Web {

// An extractive summary made without the model: the (at most) three sentences of `content`
// that mention the words of `query` most, in their original order, under a heading that
// names the query.
QString summarizeContent(const QString& content, const QString& query);

} // namespace Web
