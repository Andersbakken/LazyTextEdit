#include "textedit.h"
#include "textedit_p.h"
#include "textcursor_p.h"
#include "textdocument_p.h"

#ifndef QT_NO_DEBUG
bool doLog = true;
QString logFileName;
#endif

/*!
    Constructs an empty TextEdit with parent \a parent.
*/

TextEdit::TextEdit(QWidget *parent)
    : QAbstractScrollArea(parent), d(new TextEditPrivate(this))
{
    viewport()->setCursor(Qt::IBeamCursor);
#ifndef QT_NO_DEBUG
    if (logFileName.isEmpty())
        logFileName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz.log");
#endif
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), d, SLOT(onScrollBarValueChanged(int)));
    connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), d, SLOT(onScrollBarActionTriggered(int)));
    connect(verticalScrollBar(), SIGNAL(sliderReleased()), d, SLOT(updateScrollBar()));
    setDocument(new TextDocument(this));
    setViewportMargins(0, 0, 0, 0);
    struct {
        QString text;
        const char *member;
        QKeySequence::StandardKey key;
    } shortcuts[] = {
        { tr("Copy"), SLOT(copy()), QKeySequence::Copy },
        { tr("Paste"), SLOT(paste()), QKeySequence::Paste },
        { tr("Cut"), SLOT(cut()), QKeySequence::Cut },
        { tr("Undo"), SLOT(undo()), QKeySequence::Undo },
        { tr("Redo"), SLOT(redo()), QKeySequence::Redo },
        { tr("Select All"), SLOT(selectAll()), QKeySequence::SelectAll },
        { QString(), 0, QKeySequence::UnknownKey } };
    for (int i=0; shortcuts[i].member; ++i) {
        d->actions[i] = new QAction(shortcuts[i].text, this);
        d->actions[i]->setShortcut(QKeySequence(shortcuts[i].key));
        connect(d->actions[i], SIGNAL(triggered(bool)), this, shortcuts[i].member);
        addAction(d->actions[i]);
    }
    connect(this, SIGNAL(undoAvailableChanged(bool)), d->actions[UndoAction], SLOT(setEnabled(bool)));
    connect(this, SIGNAL(redoAvailableChanged(bool)), d->actions[RedoAction], SLOT(setEnabled(bool)));
    d->actions[UndoAction]->setEnabled(false);
    d->actions[RedoAction]->setEnabled(false);
    setContextMenuPolicy(Qt::ActionsContextMenu);
    setCursorVisible(true); // starts blinking
    connect(this, SIGNAL(selectionChanged()), viewport(), SLOT(update()));
    connect(this, SIGNAL(selectionChanged()), d, SLOT(updateCopyAndCutEnabled()));
    // ### could optimize and figure out what area to update. Not sure if it's worth it

    viewport()->setAttribute(Qt::WA_NoSystemBackground); // ### only here because I explicitly fill background in paintEvent because of weird bug
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent); // ### only here because I explicitly fill background in paintEvent because of weird bug
    if (qApp->clipboard()->supportsSelection()) {
        connect(this, SIGNAL(selectionChanged()), d, SLOT(onSelectionChanged()));
    }
}


/*!
    returns the current cursor position
*/

int TextEdit::cursorPosition() const
{
    return d->textCursor.position();
}

/*!
    ensures that \a cursor is visible with \a linesMargin lines of
    margin on each side (if possible)
*/

void TextEdit::ensureCursorVisible(const TextCursor &cursor, int linesMargin)
{
    Q_UNUSED(cursor);
    Q_UNUSED(linesMargin);
    qWarning("ensureCursorVisible not implemented");
}

/*!
    returns the QAction * for \a type.
*/

QAction *TextEdit::action(ActionType type) const
{
    return d->actions[type];
}

/*!
    Called when the textEdit is deleted
*/

TextEdit::~TextEdit()
{
    if (d->document) {
        disconnect(d->document, 0, this, 0);
        // to make sure we don't do anything drastic on shutdown
    }
    delete d;
}

/*!
    returns the text edit's document. document() always returns a
    valid TextDocument *
    \sa setDocument
*/


TextDocument *TextEdit::document() const
{
    return d->document;
}

/*!
    sets the text edit's document to \a doc. The previous document
    will be deleted if it's parented to the text edit.

    If \a doc is 0 a default TextDocument will be set.
    \sa document
*/

void TextEdit::setDocument(TextDocument *doc)
{
    if (doc == d->document)
        return;

    if (d->document) {
        disconnect(d->document, 0, this, 0);
        disconnect(d->document, 0, d, 0);
        if (d->document->parent() == this)
            delete d->document;
    }
    if (!doc)
        doc = new TextDocument(this);

    d->document = doc;
    d->sectionHovered = d->sectionPressed = 0;
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->setMouseTracking(true);
    d->sectionCount = 0;

    d->textCursor = TextCursor(doc);
    d->textCursor.textEdit = this;
    connect(d->document->d, SIGNAL(undoRedoCommandInserted(DocumentCommand *)),
            d, SLOT(onDocumentCommandInserted(DocumentCommand *)));
    connect(d->document, SIGNAL(sectionAdded(TextSection *)),
            d, SLOT(onTextSectionAdded(TextSection *)));
    connect(d->document, SIGNAL(sectionRemoved(TextSection *)),
            d, SLOT(onTextSectionRemoved(TextSection *)));
    connect(d->document->d, SIGNAL(undoRedoCommandRemoved(DocumentCommand *)),
            d, SLOT(onDocumentCommandRemoved(DocumentCommand *)));
    connect(d->document->d, SIGNAL(undoRedoCommandTriggered(DocumentCommand *, bool)),
            d, SLOT(onDocumentCommandTriggered(DocumentCommand *, bool)));
    connect(d->document, SIGNAL(charactersAdded(int, int)),
            d, SLOT(onCharactersAddedOrRemoved(int, int)));
    connect(d->document, SIGNAL(charactersRemoved(int, int)),
            d, SLOT(onCharactersAddedOrRemoved(int, int)));

    connect(d->document, SIGNAL(undoAvailableChanged(bool)),
            this, SIGNAL(undoAvailableChanged(bool)));
    connect(d->document, SIGNAL(redoAvailableChanged(bool)),
            this, SIGNAL(redoAvailableChanged(bool)));

    connect(d->document, SIGNAL(documentSizeChanged(int)), d, SLOT(onDocumentSizeChanged(int)));
    connect(d->document, SIGNAL(destroyed(QObject*)), d, SLOT(onDocumentDestroyed()));
    connect(d->document->d, SIGNAL(sectionFormatChanged(TextSection *)),
            d, SLOT(onTextSectionFormatChanged(TextSection *)));
    connect(d->document->d, SIGNAL(sectionCursorChanged(TextSection *)),
            d, SLOT(onTextSectionCursorChanged(TextSection *)));

    d->onDocumentSizeChanged(d->document->documentSize());
    d->dirty(viewport()->width());
}

/*!
    returns the textCursor's width (in pixels)
    \sa setCursorWidth
*/

int TextEdit::cursorWidth() const
{
    return d->cursorWidth;
}

/*!
    sets the cursorWidth of the text edit to \a cw pixels.
    \a cw must be a valid integer larger than 0.

    \sa setCursorVisible
*/

void TextEdit::setCursorWidth(int cw)
{
    Q_ASSERT(d->cursorWidth > 0);
    d->cursorWidth = cw;
    viewport()->update();
}

/*!
    Loads the contenst of \a dev into the text edit's document.
    Equivalent to calling document()->load(\a dev, \a mode);

    \sa setCursorVisible
*/


bool TextEdit::load(QIODevice *dev, TextDocument::DeviceMode mode, QTextCodec *codec)
{
#ifndef QT_NO_DEBUG
    if (doLog) {
        QFile f(logFileName);
        f.open(QIODevice::WriteOnly);
        QDataStream ds(&f);
        if (QFile *ff = qobject_cast<QFile*>(dev)) {
            ds << ff->fileName();
        } else {
            ds << QString::fromLatin1(dev->metaObject()->className());
        }
    }
#endif
    return d->document->load(dev, mode, codec);
}

bool TextEdit::load(const QString &file, TextDocument::DeviceMode mode, QTextCodec *codec)
{
#ifndef QT_NO_DEBUG
    if (doLog) {
        QFile f(logFileName);
        f.open(QIODevice::WriteOnly);
        QDataStream ds(&f);
        ds << file;
    }
#endif
    return d->document->load(file, mode, codec);
}

void TextEdit::paintEvent(QPaintEvent *e)
{
    if (d->ensureCursorVisiblePending) {
        d->ensureCursorVisiblePending = false;
        ensureCursorVisible();
    }
    d->updateScrollBarPosition();
    d->relayoutByGeometry(viewport()->height());
    if (d->updateScrollBarPageStepPending) {
        d->updateScrollBarPageStepPending = false;
        d->updateScrollBarPageStep();
    }

    QPainter p(viewport());
    p.fillRect(rect(), viewport()->palette().brush(viewport()->backgroundRole()));
    // ### this is a weird bug where I get debris with large documents
    // ### on the first and last visible textlayout
    p.setFont(font());
    QVector<QTextLayout::FormatRange> selection;
    int textLayoutOffset = d->viewportPosition;
    const int min = qMin(d->textCursor.anchor(), d->textCursor.position());
    const int max = qMax(d->textCursor.anchor(), d->textCursor.position());

    const QTextLayout *cursorLayout = d->cursorVisible ? d->layoutForPosition(d->textCursor.position()) : 0;
    const QRect er = e->rect();
    foreach(QTextLayout *l, d->textLayouts) {
        const QRect r = l->boundingRect().toRect();
        if (r.intersects(er)) {
            const QBrush background = d->blockFormats.value(l).background();
            if (background.style() != Qt::NoBrush) {
                p.fillRect(r, background);
            }
            if (d->textCursor.hasSelection()
                && (!(min > textLayoutOffset + l->text().size() || max < textLayoutOffset))) {
                selection.append(QTextLayout::FormatRange());
                const int localMin = min - textLayoutOffset;
                const int localMax = max - textLayoutOffset;
                selection.last().start = qMax<int>(0, localMin);
                selection.last().length = qMin<int>(l->text().size(), localMax - selection.last().start);
                selection.last().format.setBackground(palette().highlight());
                selection.last().format.setForeground(palette().highlightedText());
            }
            l->draw(&p, QPoint(0, 0), selection);
            if (!selection.isEmpty())
                selection.clear();
            if (cursorLayout == l) {
                cursorLayout->drawCursor(&p, QPoint(0, 0), d->textCursor.position() - textLayoutOffset,
                                         d->cursorWidth);
            }
        } else if (r.top() > er.bottom()) {
            break;
        }
        textLayoutOffset += l->text().size() + 1;
    }
}

void TextEdit::scrollContentsBy(int dx, int dy)
{
    Q_UNUSED(dx);
    Q_UNUSED(dy);
    viewport()->update();
//    viewport()->scroll(dx, dy); // seems to jitter more
}

int TextEdit::viewportPosition() const
{
    return d->viewportPosition;
}

bool TextEdit::isUndoAvailable() const
{
    return d->document->isUndoAvailable();
}

bool TextEdit::isRedoAvailable() const
{
    return d->document->isRedoAvailable();
}

void TextEdit::mousePressEvent(QMouseEvent *e)
{
    d->inMouseEvent = true;
    if (e->button() == Qt::LeftButton) {
#ifndef QT_NO_DEBUG
        if (doLog) {
            QFile file(logFileName);
            file.open(QIODevice::Append);
            QDataStream ds(&file);
            ds << int(e->type()) << e->pos() << e->button() << e->buttons() << e->modifiers();
        }
#endif
        if (d->tripleClickTimer.isActive()) {
            d->tripleClickTimer.stop();
            d->textCursor.movePosition(TextCursor::StartOfBlock);
            d->textCursor.movePosition(TextCursor::EndOfBlock, TextCursor::KeepAnchor);
        } else {
            clearSelection();
            int pos = textPositionAt(e->pos());
            if (pos == -1)
                pos = d->document->documentSize() - 1;
            d->sectionPressed = d->document->d->sectionAt(pos, this);
            setCursorPosition(pos, e->modifiers() & Qt::ShiftModifier
                              ? TextCursor::KeepAnchor
                              : TextCursor::MoveAnchor);
        }
        e->accept();
    } else if (e->button() == Qt::MidButton && qApp->clipboard()->supportsSelection()) {
        int pos = textPositionAt(e->pos());
        if (pos == -1)
            pos = d->document->documentSize() - 1;
        paste(pos, QClipboard::Selection);
        e->accept();
    } else {
        QAbstractScrollArea::mousePressEvent(e);
    }
}

void TextEdit::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
#ifndef QT_NO_DEBUG
        if (doLog) {
            QFile file(logFileName);
            file.open(QIODevice::Append);
            QDataStream ds(&file);
            ds << int(e->type()) << e->pos() << e->button() << e->buttons() << e->modifiers();
        }
#endif
        const int pos = textPositionAt(e->pos());
        if (pos == d->textCursor.position()) {
            d->tripleClickTimer.start(qApp->doubleClickInterval(), d);
            if (TextDocumentPrivate::isWord(d->textCursor.cursorCharacter())) { // ### this is not quite right
                d->textCursor.movePosition(TextCursor::StartOfWord);
                d->textCursor.movePosition(TextCursor::EndOfWord, TextCursor::KeepAnchor);
                return;
            }
        }
        mousePressEvent(e);
    }
}

void TextEdit::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() == Qt::NoButton) {
        d->updateCursorPosition(e->pos());
    } else if (e->buttons() == Qt::LeftButton && d->document->documentSize()) {
#ifndef QT_NO_DEBUG
        if (doLog) {
            QFile file(logFileName);
            file.open(QIODevice::Append);
            QDataStream ds(&file);
            ds << int(e->type()) << e->pos() << e->button() << e->buttons() << e->modifiers();
        }
#endif
        const QRect r = viewport()->rect();
        d->lastMouseMove = e->pos();
        e->accept();
        if (e->y() < r.top()) {
            if (d->atBeginning()) {
                d->updateHorizontalPosition();
                d->autoScrollTimer.stop();
                return;
            }
        } else if (e->y() > r.bottom()) {
            if (d->atEnd()) {
                d->updateHorizontalPosition();
                d->autoScrollTimer.stop();
                return;
            }
        } else {
            int pos = textPositionAt(QPoint(qBound(0, d->lastMouseMove.x(), r.right()), d->lastMouseMove.y()));
            if (pos == -1)
                pos = d->document->documentSize();
            d->autoScrollTimer.stop();
            setCursorPosition(pos, TextCursor::KeepAnchor);
            return;
        }
        const int distance = qMax(r.top() - d->lastMouseMove.y(), d->lastMouseMove.y() - r.bottom());
        Q_ASSERT(distance != 0);
        enum { MinimumTimeOut = 3 };
        int timeout = qMax<int>(MinimumTimeOut, 100 - distance);
        enum { Margin = 3 };
        if (qApp->desktop()->screenGeometry(this).bottom() - mapToGlobal(d->lastMouseMove).y() <= Margin) {
            timeout = MinimumTimeOut;
        }
        d->autoScrollLines = 1 + ((100 - timeout) / 30);
        if (d->autoScrollTimer.isActive()) {
            d->pendingTimeOut = timeout;
        } else {
            d->pendingTimeOut = -1;
            d->autoScrollTimer.start(timeout, d);
        }
    } else {
        QAbstractScrollArea::mouseMoveEvent(e);
    }
}

void TextEdit::mouseReleaseEvent(QMouseEvent *e)
{
    d->inMouseEvent = false;
    if (e->button() == Qt::LeftButton) {
#ifndef QT_NO_DEBUG
        if (doLog) {
            QFile file(logFileName);
            file.open(QIODevice::Append);
            QDataStream ds(&file);
            ds << int(e->type()) << e->pos() << e->button() << e->buttons() << e->modifiers();
        }
#endif
        d->autoScrollTimer.stop();
        d->pendingTimeOut = -1;
        e->accept();
        if (d->sectionPressed && sectionAt(e->pos()) == d->sectionPressed) {
            emit sectionClicked(d->sectionPressed, e->pos());
        }
        d->sectionPressed = 0;
    } else {
        QAbstractScrollArea::mouseReleaseEvent(e);
    }
}

void TextEdit::resizeEvent(QResizeEvent *e)
{
#ifndef QT_NO_DEBUG
    if (doLog) {
        QFile file(logFileName);
        file.open(QIODevice::Append);
        QDataStream ds(&file);
        ds << int(e->type()) << e->size();
    }
#endif
    QAbstractScrollArea::resizeEvent(e);
    d->updateScrollBarPageStepPending = true;
    d->dirty(viewport()->width());
}

#if 0
void TextEdit::dragEnterEvent(QDragEnterEvent *e)
{
    if (!d->readOnly && d->canInsertFromMimeData(e->mimeData())) {
        e->acceptProposedAction();
    } else {
        e->ignore();
    }
}

void TextEdit::dragMoveEvent(QDragMoveEvent *e)
{
    if (d->dragOverrideCursor.isValid()) {
        const QRect r = d->cursorRect(d->dragOverrideCursor);
        if (!r.isNull())
            viewport()->update(r);
        d->dragOverrideCursor = TextCursor();
    }

    if (!d->readOnly && d->canInsertFromMimeData(e->mimeData())) {
        int pos;
        if (e->pos().y() > viewport()->rect().bottom()) {
            pos = d->document->documentSize();
        } else {
            pos = textPositionAt(e->pos());
        }
        if (pos != -1) {
            e->acceptProposedAction();
            d->dragOverrideCursor = TextCursor(this);
            d->dragOverrideCursor.setPosition(pos);
            const QRect r = d->cursorRect(d->dragOverrideCursor);
            if (!r.isNull())
                viewport()->update(r);
            return;
        }
    }
    e->ignore();
}

void TextEdit::dropEvent(QDropEvent *e)
{
    if (!d->readOnly && d->canInsertFromMimeData(e->mimeData())) {
        int pos;
        if (e->pos().y() > viewport()->rect().bottom()) {
            pos = d->document->documentSize();
        } else {
            pos = textPositionAt(e->pos());
        }
        if (pos != -1) {
            e->acceptProposedAction();
            d->dragOverrideCursor = TextCursor(this);
            d->dragOverrideCursor.setPosition(pos);
            const QRect r = d->cursorRect(d->dragOverrideCursor);
            if (!r.isNull())
                viewport()->update(r);
            return;
        }
    }
    e->ignore();
}
#endif

int TextEdit::textPositionAt(const QPoint &pos) const
{
    if (!viewport()->rect().contains(pos))
        return -1;
    return d->textPositionAt(pos);
}

bool TextEdit::readOnly() const
{
    return d->readOnly;
}

void TextEdit::setReadOnly(bool rr)
{
    d->readOnly = rr;
    setCursorVisible(!rr);
    d->actions[PasteAction]->setEnabled(!rr);
    d->actions[CutAction]->setEnabled(!rr);
}

int TextEdit::maximumSizeCopy() const
{
    return d->maximumSizeCopy;
}

void TextEdit::setMaximumSizeCopy(int max)
{
    d->maximumSizeCopy = qMax(0, max);
    d->updateCopyAndCutEnabled();
}

QRect TextEdit::cursorBlockRect() const
{
    return d->cursorRect(d->textCursor);
}

void TextEdit::wheelEvent(QWheelEvent *e)
{
    if (e->orientation() == Qt::Vertical) {
        d->scrollLines(3 * (e->delta() > 0 ? -1 : 1));
    } else {
        QAbstractScrollArea::wheelEvent(e);
    }
}

void TextEdit::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange) {
        d->font = font();
        foreach(QTextLayout *l, (d->textLayouts + d->unusedTextLayouts)) {
            l->setFont(d->font);
        }
        d->dirty(viewport()->width());
    }
}

void TextEdit::keyPressEvent(QKeyEvent *e)
{
#ifndef QT_NO_DEBUG
    if (doLog) {
        QFile file(logFileName);
        file.open(QIODevice::Append);
        QDataStream ds(&file);
        ds << int(e->type()) << e->key() << int(e->modifiers()) << e->text() << e->isAutoRepeat() << e->count();
    }
#endif

    Q_ASSERT(d->textCursor.textEdit == this);
    if (d->readOnly) {
        d->cursorMoveKeyEvent(e);
        return;
    } else if (d->textCursor.cursorMoveKeyEvent(e)) {
        e->accept();
        return;
    }
    TextCursor::MoveOperation operation = TextCursor::NoMove;
    bool restoreCursorPosition = false;
    if (e->key() == Qt::Key_Backspace && !(e->modifiers() & ~Qt::ShiftModifier)) {
        operation = TextCursor::Left;
    } else {
        enum { Count = 4 };
        struct {
            QKeySequence::StandardKey standardKey;
            TextCursor::MoveOperation operation;
            bool restoreCursorPosition;
        } commands[Count] = {
            { QKeySequence::Delete, TextCursor::Right, true },
            { QKeySequence::DeleteStartOfWord, TextCursor::StartOfWord, false },
            { QKeySequence::DeleteEndOfWord, TextCursor::EndOfWord, true },
            { QKeySequence::DeleteEndOfLine, TextCursor::EndOfLine, true },
        };
        for (int i=0; i<Count; ++i) {
            if (e->matches(commands[i].standardKey)) {
                operation = commands[i].operation;
                restoreCursorPosition = commands[i].restoreCursorPosition;
                break;
            }
        }
    }
    if (operation != TextCursor::NoMove) {
        if (hasSelection()) {
            const int old = qMin(d->textCursor.anchor(), d->textCursor.position());
            removeSelectedText();
            setCursorPosition(old, TextCursor::MoveAnchor);
        } else {
            const int old = d->textCursor.position();
            if (moveCursorPosition(operation, TextCursor::KeepAnchor)) {
                removeSelectedText();
                if (restoreCursorPosition) {
                    setCursorPosition(old, TextCursor::MoveAnchor);
                }
            }
        }
    } else if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
        ensureCursorVisible();
        d->textCursor.insertText(QLatin1String("\n"));
    } else if (!e->text().isEmpty() && !(e->modifiers() & ~Qt::ShiftModifier)) {
        ensureCursorVisible();
        d->textCursor.insertText(e->text().toLocal8Bit());
    } else {
        e->ignore();
//        QAbstractScrollArea::keyPressEvent(e); // this causes some
//        weird unrecognized key to go through to the scrollBar and raise hell
        return;
    }
}

void TextEdit::keyReleaseEvent(QKeyEvent *e)
{
#ifndef QT_NO_DEBUG
    if (doLog) { // ### does it make any sense to replay these? Probably not
        QFile file(logFileName);
        file.open(QIODevice::Append);
        QDataStream ds(&file);
        ds << int(e->type()) << e->key() << int(e->modifiers()) << e->text() << e->isAutoRepeat() << e->count();
    }
#endif
    QAbstractScrollArea::keyReleaseEvent(e);
}


void TextEdit::setCursorPosition(int pos, TextCursor::MoveMode mode)
{
    d->textCursor.setPosition(pos, mode);
}

bool TextEdit::moveCursorPosition(TextCursor::MoveOperation op, TextCursor::MoveMode mode, int n)
{
    return d->textCursor.movePosition(op, mode, n);
}

void TextEdit::copy(QClipboard::Mode mode)
{
    if (d->textCursor.selectionSize() <= d->maximumSizeCopy) {
        QApplication::clipboard()->setText(selectedText(), mode);
    }
}

void TextEdit::paste(QClipboard::Mode mode)
{
    paste(cursorPosition(), mode);
}

void TextEdit::paste(int pos, QClipboard::Mode mode)
{
    if (d->readOnly)
        return;
    textCursor().setPosition(pos);
    textCursor().insertText(qApp->clipboard()->text(mode));
}

bool TextEdit::cursorVisible() const
{
    return d->cursorBlinkTimer.isActive();
}

void TextEdit::setCursorVisible(bool cc)
{
    if (cc == d->cursorBlinkTimer.isActive())
        return;

    d->cursorVisible = cc;
    if (cc) {
        d->cursorBlinkTimer.start(QApplication::cursorFlashTime(), d);
    } else {
        d->cursorBlinkTimer.stop();
    }
    viewport()->update(cursorBlockRect());
}

void TextEdit::clearSelection()
{
    if (!d->textCursor.hasSelection())
        return;

    d->textCursor.clearSelection();
}

QString TextEdit::selectedText() const
{
    return d->textCursor.selectedText();
}

bool TextEdit::hasSelection() const
{
    return d->textCursor.hasSelection();
}

void TextEdit::insert(int pos, const QString &text)
{
    Q_ASSERT(d->document);
    d->document->insert(pos, text);
}

void TextEdit::remove(int from, int size)
{
    if (d->readOnly)
        return;
    Q_ASSERT(d->document);
    d->document->remove(from, size);
}

void TextEdit::append(const QString &text)
{
    Q_ASSERT(d->document);
    enum { Margin = 1 };
    const int diff = verticalScrollBar()->maximum() - verticalScrollBar()->value();
    d->document->append(text);
    if (diff <= Margin)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum() - diff);
}

void TextEdit::removeSelectedText()
{
    if (d->readOnly)
        return;
    d->textCursor.removeSelectedText();
}

void TextEdit::cut()
{
    if (d->readOnly || !hasSelection())
        return;
    copy(QClipboard::Clipboard);
    removeSelectedText();
}

void TextEdit::undo()
{
    if (!d->readOnly)
        d->document->undo();
}

void TextEdit::redo()
{
    if (!d->readOnly)
        d->document->redo();
}

void TextEdit::selectAll()
{
    TextCursor cursor(d->document);
    Q_ASSERT(cursor.position() == 0);
    cursor.movePosition(TextCursor::End, TextCursor::KeepAnchor);
    setTextCursor(cursor);
    emit selectionChanged();
}

bool TextEdit::save(QIODevice *device)
{
    return d->document->save(device);
}

bool TextEdit::save(const QString &file)
{
    return d->document->save(file);
}

bool TextEdit::save()
{
    return d->document->save();
}

void TextEditPrivate::onDocumentSizeChanged(int size)
{
    textEdit->verticalScrollBar()->setRange(0, size - 1);
    updateScrollBarPageStepPending = true;
}

void TextEditPrivate::updateCopyAndCutEnabled()
{
    const bool enable = qAbs(textCursor.position() - textCursor.anchor()) <= maximumSizeCopy;
    actions[TextEdit::CopyAction]->setEnabled(enable);
    actions[TextEdit::CutAction]->setEnabled(enable);
}

void TextEdit::setSyntaxHighlighter(SyntaxHighlighter *highlighter)
{
    if (d->syntaxHighlighter == highlighter)
        return;

    if (d->syntaxHighlighter) {
        if (d->syntaxHighlighter->parent() == this) {
            delete d->syntaxHighlighter;
        } else {
            d->syntaxHighlighter->d->textEdit = 0;
            d->syntaxHighlighter->d->textLayout = 0;
        }
    }

    d->syntaxHighlighter = highlighter;
    if (d->syntaxHighlighter) {
        d->syntaxHighlighter->d->textEdit = this;
        d->syntaxHighlighter->d->textLayout = d;
    }
    d->dirty(viewport()->width());
}

SyntaxHighlighter * TextEdit::syntaxHighlighter() const
{
    return d->syntaxHighlighter;
}

static inline bool compareExtraSelection(const TextEdit::ExtraSelection &left, const TextEdit::ExtraSelection &right)
{
    return left.cursor < right.cursor;
}

void TextEdit::setExtraSelections(const QList<ExtraSelection> &selections)
{
    qWarning() << __FUNCTION__ << "This is currently not used";
    d->extraSelections = selections;
    qSort(d->extraSelections.begin(), d->extraSelections.end(), compareExtraSelection);
    d->dirty(viewport()->width());
}

QList<TextEdit::ExtraSelection> TextEdit::extraSelections() const
{
    qWarning() << __FUNCTION__ << "This is currently not used";
    return d->extraSelections;
}

void TextEditPrivate::onTextSectionRemoved(TextSection *section)
{
    Q_ASSERT(section);
    if (!dirtyForSection(section))
        return;

    sectionsDirty = true;
    if (section == sectionPressed) {
        sectionPressed = 0;
    }
    if (section == sectionHovered) {
        if (sectionHovered->hasCursor())
            textEdit->viewport()->setCursor(Qt::IBeamCursor);
        sectionHovered = 0;
    }
}

void TextEditPrivate::onTextSectionAdded(TextSection *section)
{
    dirtyForSection(section);
    updateCursorPosition(lastHoverPos);
    sectionsDirty = true;
}

void TextEditPrivate::onScrollBarValueChanged(int value)
{
    if (blockScrollBarUpdate || value == requestedScrollBarPosition || value == viewportPosition)
        return;
    requestedScrollBarPosition = value;
    dirty(textEdit->viewport()->width());
}

void TextEditPrivate::onScrollBarActionTriggered(int action)
{
    switch (action) {
    case QAbstractSlider::SliderSingleStepAdd: scrollLines(1); requestedScrollBarPosition = -1; break;
    case QAbstractSlider::SliderSingleStepSub: scrollLines(-1); requestedScrollBarPosition = -1; break;
    default: break;
    }
}

void TextEditPrivate::updateScrollBar()
{
    Q_ASSERT(!textEdit->verticalScrollBar()->isSliderDown());
    if (pendingScrollBarUpdate) {
        const bool old = blockScrollBarUpdate;
        blockScrollBarUpdate = true;
        textEdit->verticalScrollBar()->setValue(viewportPosition);
        blockScrollBarUpdate = old;
        pendingScrollBarUpdate  = false;
    }
}

void TextEditPrivate::onCharactersAddedOrRemoved(int from, int count)
{
    Q_ASSERT(count >= 0);
    Q_UNUSED(count);
    if (from > layoutEnd) {
        return;
    }
    buffer.clear();
    dirty(textEdit->viewport()->width());
}

void TextEdit::ensureCursorVisible()
{
    if (d->textCursor.position() < d->viewportPosition) {
        d->updatePosition(qMax(0, d->textCursor.position() - 1), TextLayout::Backward);
    } else {
        const QRect r = viewport()->rect();
        const QRect cursorRect = cursorBlockRect();
        if (!r.contains(cursorRect)) {
            if (r.intersects(cursorRect)) {
                d->scrollLines(d->autoScrollLines);
            } else {
                d->updatePosition(d->textCursor.position(), TextLayout::Backward);
            }
        }
    }
}

TextCursor &TextEdit::textCursor()
{
    return d->textCursor;
}

const TextCursor &TextEdit::textCursor() const
{
    return d->textCursor;
}

void TextEdit::setTextCursor(const TextCursor &textCursor)
{
    const bool doEmit = (d->textCursor != textCursor);
    d->textCursor = textCursor;
    d->textCursor.textEdit = this;
    if (doEmit) {
        viewport()->update();
        emit cursorPositionChanged(textCursor.position());
    }
}

TextSection *TextEdit::sectionAt(const QPoint &pos) const
{
    Q_ASSERT(d->document);
    const int textPos = textPositionAt(pos);
    if (textPos == -1)
        return 0;
    return d->document->d->sectionAt(textPos, this);
}

QList<TextSection*> TextEdit::sections(int from, int size, TextSection::TextSectionOptions opt) const
{
    Q_ASSERT(d->document);
    QList<TextSection*> sections = d->document->d->getSections(from, size, opt, this);
    return sections;
}

TextSection *TextEdit::insertTextSection(int pos, int size, const QTextCharFormat &format, const QVariant &data)
{
    Q_ASSERT(d->document);
    TextSection *section = d->document->insertTextSection(pos, size, format, data);
    if (section)
        section->d.textEdit = this;
    return section;
}

void TextEditPrivate::updateHorizontalPosition()
{
    const QRect r = textEdit->viewport()->rect();
    const QPoint p(qBound(0, lastMouseMove.x(), r.right()),
                   qBound(0, lastMouseMove.y(), r.bottom()));
    int pos = textPositionAt(p);
    if (pos == -1)
        pos = document->documentSize() - 1;
    textEdit->setCursorPosition(pos, TextCursor::KeepAnchor);
}

void TextEditPrivate::updateScrollBarPosition()
{
    if (requestedScrollBarPosition == -1) {
        return;
    } else if (requestedScrollBarPosition == viewportPosition) {
        requestedScrollBarPosition = -1;
        return;
    }

    const int req = requestedScrollBarPosition;
    requestedScrollBarPosition = -1;

    Direction direction = Forward;
    if (lastRequestedScrollBarPosition != -1 && lastRequestedScrollBarPosition != req) {
        if (lastRequestedScrollBarPosition > req) {
            direction = Backward;
        }
    } else if (req < viewportPosition) {
        direction = Backward;
    }

    lastRequestedScrollBarPosition = req;

    updatePosition(req, direction);
}

void TextEditPrivate::updateScrollBarPageStep()
{
    if (lines.isEmpty()) {
        textEdit->verticalScrollBar()->setPageStep(1);
        return;
    }
    const int visibleCharacters = lines.at(qMin(visibleLines, lines.size() - 1)).first - lines.at(0).first;
    textEdit->verticalScrollBar()->setPageStep(visibleCharacters);
}

void TextEditPrivate::onDocumentDestroyed()
{
    document = 0;
    textEdit->setDocument(new TextDocument(this)); // there should always be a document
}

void TextEditPrivate::onDocumentCommandInserted(DocumentCommand *cmd)
{
    CursorData &data = undoRedoCommands[cmd].first;
    data.position = textCursor.position();
    data.anchor = textCursor.anchor();
    // this happens before the actual insert/remove so the pos/anchor is not changed yet
}

void TextEditPrivate::onDocumentCommandFinished(DocumentCommand *cmd)
{
    Q_ASSERT(undoRedoCommands.contains(cmd));
    CursorData &data = undoRedoCommands[cmd].second;
    data.position = textCursor.position();
    data.anchor = textCursor.anchor();
    // this happens after the actual insert/remove
}

void TextEditPrivate::onDocumentCommandRemoved(DocumentCommand *cmd)
{
    undoRedoCommands.take(cmd);
}

void TextEditPrivate::onDocumentCommandTriggered(DocumentCommand *cmd, bool undo)
{
    if (undoRedoCommands.contains(cmd)) {
        const CursorData &ec = undo
                               ? undoRedoCommands.value(cmd).first
                               : undoRedoCommands.value(cmd).second;
        if (ec.position != ec.anchor) {
            textCursor.setPosition(ec.anchor, TextCursor::MoveAnchor);
            textCursor.setPosition(ec.position, TextCursor::KeepAnchor);
        } else {
            textCursor.setPosition(ec.position, TextCursor::MoveAnchor);
        }
    }
}

void TextEditPrivate::scrollLines(int lines)
{
    int pos = viewportPosition;
    const Direction d = (lines < 0 ? Backward : Forward);
    const int add = lines < 0 ? -1 : 1;
    while (pos + add >= 0 && pos + add < document->documentSize()) {
        pos += add;
        QChar c = bufferReadCharacter(pos);
        if (bufferReadCharacter(pos) == '\n') {
            if ((lines -= add) == 0) {
                break;
            }
        }
    }
    updatePosition(pos, d);
}

void TextEditPrivate::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == tripleClickTimer.timerId()) {
        tripleClickTimer.stop();
    } else if (e->timerId() == autoScrollTimer.timerId()) {
        if (pendingTimeOut != -1) {
            autoScrollTimer.start(pendingTimeOut, this);
            pendingTimeOut = -1;
        }
        const QRect r = textEdit->viewport()->rect();
        Q_ASSERT(!r.contains(lastMouseMove));
        enum { LineCount = 1 };
        if (lastMouseMove.y() < r.top()) {
            scrollLines(-LineCount);
            relayoutByGeometry(r.height());
            if (atBeginning())
                autoScrollTimer.stop();
        } else {
            Q_ASSERT(lastMouseMove.y() > r.bottom());
            scrollLines(LineCount);
            relayoutByGeometry(r.height());
            if (atEnd())
                autoScrollTimer.stop();
        }
        // called relayoutByGeometry to force the relaying to happen
        // immediately to make sure textPositionAt returns a valid
        // position
        const QPoint p(qBound(0, lastMouseMove.x(), r.right()),
                       qBound(0, lastMouseMove.y(), r.bottom()));
        int pos = textPositionAt(p);
        if (pos == -1)
            pos = document->documentSize() - 1;
        textEdit->setCursorPosition(pos, TextCursor::KeepAnchor);
    } else if (e->timerId() == cursorBlinkTimer.timerId()) {
        cursorVisible = !cursorVisible;
        textEdit->viewport()->update(textEdit->cursorBlockRect());
    } else {
        Q_ASSERT(0);
    }
}

void TextEditPrivate::updateCursorPosition(const QPoint &pos)
{
    lastHoverPos = pos;
    sectionHovered = textEdit->sectionAt(pos);
    textEdit->viewport()->setCursor((sectionHovered && sectionHovered->hasCursor())
                                    ? sectionHovered->cursor()
                                    : Qt::IBeamCursor);
}

bool TextEditPrivate::isSectionOnScreen(const TextSection *section) const
{
    return (section->document() == document
            && ::matchSection(section, textEdit)
            && section->position() <= layoutEnd
            && section->position() + section->size() >= viewportPosition);
}

void TextEditPrivate::cursorMoveKeyEvent(QKeyEvent *e)
{
    if (e == QKeySequence::MoveToNextLine) {
        scrollLines(1);
    } else if (e == QKeySequence::MoveToPreviousLine) {
        scrollLines(-1);
    } else if (e == QKeySequence::MoveToStartOfLine || e == QKeySequence::MoveToStartOfDocument) {
        textEdit->verticalScrollBar()->setValue(0);
    } else if (e == QKeySequence::MoveToEndOfLine || e == QKeySequence::MoveToEndOfDocument) {
        textEdit->verticalScrollBar()->setValue(textEdit->verticalScrollBar()->maximum());
    } else if (e == QKeySequence::MoveToNextPage) {
        scrollLines(qMax(1, visibleLines - 1));
    } else if (e == QKeySequence::MoveToPreviousPage) {
        scrollLines(-qMax(1, visibleLines));
    } else {
        e->ignore();
        return;
    }
    e->accept();
    textCursor.setPosition(viewportPosition);
}



bool TextEditPrivate::dirtyForSection(TextSection *section)
{
    if (isSectionOnScreen(section)) {
        dirty(textEdit->viewport()->width());
        return true;
    } else {
        return false;
    }
}

void TextEditPrivate::onTextSectionFormatChanged(TextSection *section)
{
    dirtyForSection(section);
}

void TextEditPrivate::onTextSectionCursorChanged(TextSection *section)
{
    if (isSectionOnScreen(section)) {
        updateCursorPosition(lastHoverPos);
    }
}

void TextEditPrivate::onSelectionChanged()
{
    if (inMouseEvent && textCursor.hasSelection()) {
        textEdit->copy(QClipboard::Selection);
    }
}

bool TextEditPrivate::canInsertFromMimeData(const QMimeData *data) const
{
    return data->hasText();
}

QRect TextEditPrivate::cursorRect(const TextCursor &cursor) const
{
    if (const QTextLayout *l = layoutForPosition(cursor.position())) {
        return l->boundingRect().toRect();
    }
    return QRect();
}

