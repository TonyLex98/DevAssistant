#include "codeeditor.h"
#include <QTextBlock>
#include <QPainterPath>
#include <QScrollBar>
#include <QDebug>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    m_lineNumberArea = new LineNumberArea(this);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(10);
    setFont(f);
    setLineWrapMode(QPlainTextEdit::NoWrap);   // code keeps its lines; scroll sideways instead

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
}

int CodeEditor::lineNumberAreaWidth() const {
    // Calculate width based on max visible line number.
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * (digits + 1); // +1 for fold marker room
    return space + 6;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    if (isReadOnly()) return;
    QList<QTextEdit::ExtraSelection> extra;
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(QColor(0,0,0,20));
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    sel.cursor = textCursor();
    sel.cursor.clearSelection();
    extra.append(sel);

    setExtraSelections(extra);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(245,245,245));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();

    const int w = m_lineNumberArea->width();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString num = QString::number(blockNumber + 1);
            painter.setPen(Qt::gray);
            painter.drawText(0, top, w - 4, fontMetrics().height(),
                             Qt::AlignRight, num);

            // Draw fold marker if block has a fold start
            if (auto *bd = static_cast<BlockData*>(block.userData())) {
                if (bd->foldStart && bd->foldEndLine >= blockNumber) {
                    QRect triRect(2, top + 3, fontMetrics().height()-6, fontMetrics().height()-6);
                    QPainterPath path;
                    if (bd->collapsed) {
                        // Draw right-pointing triangle
                        path.moveTo(triRect.left(), triRect.top());
                        path.lineTo(triRect.right(), triRect.center().y());
                        path.lineTo(triRect.left(), triRect.bottom());
                        path.closeSubpath();
                    } else {
                        // Draw down-pointing triangle
                        path.moveTo(triRect.left(), triRect.top());
                        path.lineTo(triRect.right(), triRect.top());
                        path.lineTo(triRect.center().x(), triRect.bottom());
                        path.closeSubpath();
                    }
                    painter.fillPath(path, QColor(120,120,120));
                    painter.setPen(QPen(QColor(120,120,120)));
                    painter.drawPath(path);
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + (int) blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void CodeEditor::setFoldRanges(const QVector<FoldRange> &ranges) {
    // Clear previous
    clearFolds();

    // Map start line -> end line
    for (const auto &fr : ranges) {
        if (fr.startLine < 0 || fr.endLine < fr.startLine) continue;
        QTextBlock block = document()->findBlockByNumber(fr.startLine);
        if (!block.isValid()) continue;
        auto *bd = block.userData() ? static_cast<BlockData*>(block.userData()) : new BlockData();
        bd->foldStart = true;
        bd->foldEndLine = fr.endLine;
        block.setUserData(bd);
    }
    m_lineNumberArea->update();
}

void CodeEditor::clearFolds() {
    for (QTextBlock b = document()->firstBlock(); b.isValid(); b = b.next()) {
        if (auto *bd = static_cast<BlockData*>(b.userData())) {
            delete bd;
            b.setUserData(nullptr);
        }
        b.setVisible(true);
        b.setLineCount(b.lineCount());
    }
    viewport()->update();
    m_lineNumberArea->update();
}

void CodeEditor::toggleFoldAt(int startLine) {
    QTextBlock block = document()->findBlockByNumber(startLine);
    if (!block.isValid()) return;
    auto *bd = static_cast<BlockData*>(block.userData());
    if (!bd || !bd->foldStart || bd->foldEndLine < startLine) return;
    applyFold(bd, startLine, !bd->collapsed);
}

void CodeEditor::applyFold(BlockData *bd, int startLine, bool collapse) {
    const int endLine = bd->foldEndLine;
    QTextBlock startBlock = document()->findBlockByNumber(startLine);
    QTextBlock block = startBlock.next(); // collapse content after the start line
    while (block.isValid() && block.blockNumber() <= endLine) {
        block.setVisible(!collapse);
        block.setLineCount(collapse ? 0 : qMax(1, block.lineCount()));
        block = block.next();
    }
    bd->collapsed = collapse;
    viewport()->update();
    m_lineNumberArea->update();
    emit foldToggled(startLine, collapse);
}

bool CodeEditor::isPointInGutter(const QPoint &pt) const {
    return pt.x() < lineNumberAreaWidth();
}

void CodeEditor::gotoLineColumn(int line, int column) {
    line = qMax(0, line);
    QTextBlock b = document()->findBlockByNumber(line);
    if (!b.isValid()) return;
    QTextCursor c(b);
    c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    setTextCursor(c);
    centerCursor();
}

/*
void CodeEditor::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Tab) {
        textCursor().insertText(QString(" ").repeated(4));
        return;
    }
    QPlainTextEdit::keyPressEvent(e);
}
*/

void CodeEditor::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Tab) {
        textCursor().insertText("\t"); // Use tab character.
        return;
    }
    if (e->modifiers() & Qt::ControlModifier && e->key() == Qt::Key_S) {
        emit requestSave();   // Ctrl+S trigger.
        return;               // Stop default handling.
    }
    QPlainTextEdit::keyPressEvent(e);
}
