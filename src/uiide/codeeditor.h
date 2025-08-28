#pragma once
#include <QPlainTextEdit>
#include <QWidget>
#include <QPainter>
#include <QTextBlock>
#include <QVector>
#include <QFontDatabase>
#include <QMouseEvent>


// --- Internal per-block data for folding ---
class BlockData : public QTextBlockUserData {
public:
    bool foldStart = false;
    int  foldEndLine = -1;      // inclusive
    bool collapsed = false;
};


class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    struct FoldRange { int startLine; int endLine; }; // 0-based inclusive
    explicit CodeEditor(QWidget *parent = nullptr);

    int lineNumberAreaWidth() const;
    void setFoldRanges(const QVector<FoldRange> &ranges);
    void clearFolds();
    void gotoLineColumn(int line, int column);

    // Expose for dialog to know if click was in gutter
    bool isPointInGutter(const QPoint &pt) const;

signals:
    void foldToggled(int startLine, bool collapsed);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void updateLineNumberAreaWidth(int /* newBlockCount */);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    void toggleFoldAt(int startLine);
    void applyFold(BlockData *bd, int startLine, bool collapse);

    QWidget *m_lineNumberArea;

    friend class LineNumberArea;
};


class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    CodeEditor *codeEditor;
};


// ---- Inline impls for LineNumberArea ----
inline QSize LineNumberArea::sizeHint() const { return QSize(((CodeEditor*)codeEditor)->lineNumberAreaWidth(), 0); }

inline void LineNumberArea::paintEvent(QPaintEvent *event) { ((CodeEditor*)codeEditor)->lineNumberAreaPaintEvent(event); }

inline void LineNumberArea::mousePressEvent(QMouseEvent *event) {
    CodeEditor *ed = (CodeEditor*)codeEditor;
    // Map y to block line
    int y = event->pos().y();
    QTextBlock block = ed->firstVisibleBlock();
    int top = (int)ed->blockBoundingGeometry(block).translated(ed->contentOffset()).top();
    int bottom = top + (int)ed->blockBoundingRect(block).height();
    int line = block.blockNumber();
    while (block.isValid() && top <= y) {
        if (y <= bottom) break;
        block = block.next();
        top = bottom;
        bottom = top + (int)ed->blockBoundingRect(block).height();
        ++line;
    }
    ed->toggleFoldAt(line);
}
