// Copyright 2009 Anders Bakken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QtGui>
#include "textdocument.h"
#include "textcursor.h"
#include "textsection.h"
#include "syntaxhighlighter.h"

class TextEditPrivate;
class TextEdit : public QAbstractScrollArea
{
    Q_OBJECT
    Q_PROPERTY(int cursorWidth READ cursorWidth WRITE setCursorWidth)
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly)
    Q_PROPERTY(bool cursorVisible READ cursorVisible WRITE setCursorVisible)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(bool undoAvailable READ isUndoAvailable)
    Q_PROPERTY(bool redoAvailable READ isRedoAvailable)
    Q_PROPERTY(int maximumSizeCopy READ maximumSizeCopy WRITE setMaximumSizeCopy)
    Q_PROPERTY(bool lineBreaking READ lineBreaking WRITE setLineBreaking)

public:
    TextEdit(QWidget *parent = 0);
    ~TextEdit();

    TextDocument *document() const;
    void setDocument(TextDocument *doc);

    int cursorWidth() const;
    void setCursorWidth(int cc);

    struct ExtraSelection
    {
        TextCursor cursor;
        QTextCharFormat format;
    };

    void setExtraSelections(const QList<ExtraSelection> &selections);
    QList<ExtraSelection> extraSelections() const;

    void setSyntaxHighlighter(SyntaxHighlighter *highlighter);
    SyntaxHighlighter *syntaxHighlighter() const;

    bool load(QIODevice *device, TextDocument::DeviceMode mode = TextDocument::Sparse, QTextCodec *codec = 0);
    bool load(const QString &fileName, TextDocument::DeviceMode mode = TextDocument::Sparse, QTextCodec *codec = 0);
    void paintEvent(QPaintEvent *e);
    void scrollContentsBy(int dx, int dy);

    bool moveCursorPosition(TextCursor::MoveOperation op, TextCursor::MoveMode = TextCursor::MoveAnchor, int n = 1);
    void setCursorPosition(int pos, TextCursor::MoveMode mode = TextCursor::MoveAnchor);

    int viewportPosition() const;
    int cursorPosition() const;

    int textPositionAt(const QPoint &pos) const;

    bool readOnly() const;
    void setReadOnly(bool rr);

    bool lineBreaking() const;
    void setLineBreaking(bool lb);

    int maximumSizeCopy() const;
    void setMaximumSizeCopy(int max);

    QRect cursorBlockRect(const TextCursor &cursor) const;
    QRect cursorRect(const TextCursor &cursor) const;

    int lineNumber(int position) const;
    int columnNumber(int position) const;
    int lineNumber(const TextCursor &cursor) const;
    int columnNumber(const TextCursor &cursor) const;

    bool cursorVisible() const;
    void setCursorVisible(bool cc);

    QString selectedText() const;
    bool hasSelection() const;

    bool save(QIODevice *device);
    bool save();
    bool save(const QString &file);

    void setText(const QString &text);
    QString read(int pos, int size) const;
    QChar readCharacter(int index) const;

    void insert(int pos, const QString &text);
    void remove(int from, int size);

    TextCursor &textCursor();
    const TextCursor &textCursor() const;
    void setTextCursor(const TextCursor &textCursor);

    TextCursor cursorForPosition(const QPoint &pos) const;

    TextSection *sectionAt(const QPoint &pos) const;

    QList<TextSection*> sections(int from = 0, int size = -1, TextSection::TextSectionOptions opt = 0) const;
    inline TextSection *sectionAt(int pos) const { return sections(pos, 1, TextSection::IncludePartial).value(0); }
    TextSection *insertTextSection(int pos, int size, const QTextCharFormat &format = QTextCharFormat(),
                                   const QVariant &data = QVariant());

    void ensureCursorVisible(const TextCursor &cursor, int linesMargin = 0);
    bool isUndoAvailable() const;
    bool isRedoAvailable() const;

    enum ActionType {
        CopyAction,
        PasteAction,
        CutAction,
        UndoAction,
        RedoAction,
        SelectAllAction
    };
    QAction *action(ActionType type) const;
public slots:
    void ensureCursorVisible();
    void append(const QString &text);
    void removeSelectedText();
    void copy(QClipboard::Mode mode = QClipboard::Clipboard);
    void paste(QClipboard::Mode mode = QClipboard::Clipboard);
    void cut();
    void undo();
    void redo();
    void selectAll();
    void clearSelection();
signals:
    void selectionChanged();
    void cursorPositionChanged(int pos);
    void sectionClicked(TextSection *section, const QPoint &pos);
    void undoAvailableChanged(bool on);
    void redoAvailableChanged(bool on);
protected:
    virtual void paste(int position, QClipboard::Mode mode);
    virtual void changeEvent(QEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void keyReleaseEvent(QKeyEvent *e);
    virtual void wheelEvent(QWheelEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void resizeEvent(QResizeEvent *e);
#if 0 // ### not done yet
    virtual void dragEnterEvent(QDragEnterEvent *e);
    virtual void dragMoveEvent(QDragMoveEvent *e);
    virtual void dropEvent(QDropEvent *e);
#endif
private:
    TextEditPrivate *d;
    friend class TextLayoutCacheManager;
    friend class TextEditPrivate;
    friend class TextCursor;
};

#endif
