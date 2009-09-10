#include "textlayout_p.h"
#include "textedit_p.h"
#include "textdocument.h"
#include "textedit.h"

void TextLayout::dirty(int width)
{
    viewport = width;
    layoutDirty = true;
    if (textEdit)
        textEdit->viewport()->update();
}

int TextLayout::viewportWidth() const
{
    return textEdit ? textEdit->viewport()->width() : viewport;
}

int TextLayout::doLayout(int index, QList<TextSection*> *sections) // index is in document coordinates
{
    QTextLayout *textLayout = 0;
    if (!unusedTextLayouts.isEmpty()) {
        textLayout = unusedTextLayouts.takeLast();
        textLayout->clearAdditionalFormats();
    } else {
        textLayout = new QTextLayout;
        textLayout->setFont(font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textLayout->setTextOption(option);
    }
    textLayouts.append(textLayout);
    if (index != 0 && bufferReadCharacter(index - 1) != '\n') {
        qWarning() << index << viewportPosition << document->read(index - 1, 20)
                   << bufferReadCharacter(index - 1);
    }
    Q_ASSERT(index == 0 || bufferReadCharacter(index - 1) == '\n');
    const int max = bufferPosition + buffer.size();
    const int lineStart = index;
    while (index < max && bufferReadCharacter(index) != '\n')
        ++index;

    const QString string = buffer.mid(lineStart - bufferPosition, index - lineStart);
    Q_ASSERT(string.size() == index - lineStart);
    Q_ASSERT(!string.contains('\n'));
    if (index < max)
        ++index; // for the newline
    textLayout->setText(string);

    QList<QTextLayout::FormatRange> formats;
    if (sections) {
        do {
            Q_ASSERT(!sections->isEmpty());
            TextSection *l = sections->first();
            Q_ASSERT(l->position() + l->size() >= lineStart);
            if (l->position() >= index) {
                break;
            }
            // section is in this QTextLayout
            QTextLayout::FormatRange range;
            range.start = qMax(0, l->position() - lineStart); // offset in QTextLayout
            range.length = qMin(l->position() + l->size(), index) - lineStart - range.start;
            range.format = l->format();
            formats.append(range);
            if (l->position() + l->size() >= index) { // > ### ???
                // means section didn't end here. It continues in the next QTextLayout
                break;
            }
            sections->removeFirst();
        } while (!sections->isEmpty());
    }

    int leftMargin = LeftMargin;
    int rightMargin = 0;
    int topMargin = 0;
    int bottomMargin = 0;
    if (syntaxHighlighter) {
        syntaxHighlighter->d->currentBlockPosition = lineStart;
        syntaxHighlighter->d->formatRanges.clear();
        syntaxHighlighter->d->currentBlock = string;
        syntaxHighlighter->highlightBlock(string);
        syntaxHighlighter->d->currentBlock.clear();
        if (syntaxHighlighter->d->blockFormat.isValid()) {
            blockFormats[textLayout] = syntaxHighlighter->d->blockFormat;
            if (syntaxHighlighter->d->blockFormat.hasProperty(QTextFormat::BlockLeftMargin))
                leftMargin = syntaxHighlighter->d->blockFormat.leftMargin();
            if (syntaxHighlighter->d->blockFormat.hasProperty(QTextFormat::BlockRightMargin))
                rightMargin = syntaxHighlighter->d->blockFormat.rightMargin();
            if (syntaxHighlighter->d->blockFormat.hasProperty(QTextFormat::BlockTopMargin))
                topMargin = syntaxHighlighter->d->blockFormat.topMargin();
            if (syntaxHighlighter->d->blockFormat.hasProperty(QTextFormat::BlockBottomMargin))
                bottomMargin = syntaxHighlighter->d->blockFormat.bottomMargin();

        }
        syntaxHighlighter->d->previousBlockState = syntaxHighlighter->d->currentBlockState;
    }

    textLayout->beginLayout();

    forever {
        QTextLine line = textLayout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(viewportWidth() - (leftMargin + rightMargin));
        // ### support blockformat margins etc
        int y = topMargin + lastBottomMargin;
        if (!lines.isEmpty()) {
            y += int(lines.last().second.rect().bottom());
            // QTextLine doesn't seem to get its rect() update until a
            // new line has been created (or presumably in endLayout)
        }
        line.setPosition(QPoint(leftMargin, y));
        lines.append(qMakePair(lineStart + line.textStart(), line));
    }
    if (syntaxHighlighter && !syntaxHighlighter->d->formatRanges.isEmpty()) {
        formats += syntaxHighlighter->d->formatRanges;
    }
    textLayout->setAdditionalFormats(formats);
    lastBottomMargin = bottomMargin;

    textLayout->endLayout();
#ifndef QT_NO_DEBUG
    for (int i=1; i<lines.size(); ++i) {
        Q_ASSERT(lines.at(i).first - (lines.at(i - 1).first + lines.at(i - 1).second.textLength()) <= 1);
    }
#endif

    contentRect |= textLayout->boundingRect().toRect();
    Q_ASSERT(contentRect.right() <= viewportWidth() + LeftMargin);

    if (syntaxHighlighter) {
        syntaxHighlighter->d->formatRanges.clear();
        syntaxHighlighter->d->blockFormat = QTextBlockFormat();
        syntaxHighlighter->d->currentBlockPosition = -1;
    }

    return index;
}

int TextLayout::textPositionAt(const QPoint &p) const
{
    QPoint pos = p;
    if (pos.x() >= 0 && pos.x() < LeftMargin)
        pos.rx() = LeftMargin; // clicking in the margin area should count as the first characters

    int textLayoutOffset = viewportPosition;
    foreach(const QTextLayout *l, textLayouts) {
        if (l->boundingRect().toRect().contains(pos)) {
            const int lineCount = l->lineCount();
            for (int i=0; i<lineCount; ++i) {
                const QTextLine line = l->lineAt(i);
                if (line.y() <= pos.y() && pos.y() <= line.height() + line.y()) { // ### < ???
                    return textLayoutOffset + line.xToCursor(qMax<int>(LeftMargin, pos.x()));
                }
            }
        }
        textLayoutOffset += l->text().size() + 1; // + 1 for newlines which aren't in the QTextLayout
    }
    return -1;
}

QList<TextSection*> TextLayout::relayoutCommon()
{
    Q_ASSERT(layoutDirty);
    layoutDirty = false;
    Q_ASSERT(document);
    lines.clear();
    unusedTextLayouts = textLayouts;
    textLayouts.clear();
    contentRect = QRect();
    visibleLines = lastVisibleCharacter = -1;

    if (syntaxHighlighter) {
        syntaxHighlighter->d->previousBlockState = syntaxHighlighter->d->currentBlockState = -1;
    }

    if (viewportPosition < bufferPosition
        || (bufferPosition + buffer.size() < document->documentSize()
            && buffer.size() - bufferOffset() < MinimumBufferSize)) {
        bufferPosition = qMax(0, viewportPosition - MinimumBufferSize);
        buffer = document->read(bufferPosition, int(MinimumBufferSize * 2.5));
        sections = document->sections(bufferPosition, buffer.size(), TextSection::IncludePartial);
    } else if (sectionsDirty) {
        sections = document->sections(bufferPosition, buffer.size(), TextSection::IncludePartial);
    }
    sectionsDirty = false;
    QList<TextSection*> l = sections;
    while (!l.isEmpty() && l.first()->position() + l.first()->size() < viewportPosition)
        l.takeFirst(); // could cache these as well
    return l;
}

void TextLayout::relayoutByGeometry(int height)
{
    if (!layoutDirty)
        return;

    QList<TextSection*> l = relayoutCommon();

    const int max = viewportPosition + buffer.size() - bufferOffset(); // in document coordinates
    Q_ASSERT(viewportPosition == 0 || bufferReadCharacter(viewportPosition - 1) == '\n');

    static const int extraLines = qMax(2, qgetenv("LAZYTEXTEDIT_EXTRA_LINES").toInt());
    int index = viewportPosition;
    while (index < max) {
        index = doLayout(index, l.isEmpty() ? 0 : &l);
        Q_ASSERT(index == max || document->readCharacter(index - 1) == '\n');
        Q_ASSERT(!textLayouts.isEmpty());
        const int y = int(textLayouts.last()->boundingRect().bottom());
        if (y >= height) {
            if (visibleLines == -1) {
                visibleLines = lines.size();
                lastVisibleCharacter = index;
            } else if (lines.size() >= visibleLines + extraLines) {
                break;
            }
        }
    }
    if (visibleLines == -1) {
        visibleLines = lines.size();
        lastVisibleCharacter = index;
    }


    layoutEnd = qMin(index, max);
    qDeleteAll(unusedTextLayouts);
    unusedTextLayouts.clear();
    Q_ASSERT(viewportPosition < layoutEnd ||
             (viewportPosition == layoutEnd && viewportPosition == document->documentSize()));
}

void TextLayout::relayoutByPosition(int size)
{
    if (!layoutDirty)
        return;

    QList<TextSection*> l = relayoutCommon();

    const int max = viewportPosition + qMin(size, buffer.size() - bufferOffset());
    Q_ASSERT(viewportPosition == 0 || bufferReadCharacter(viewportPosition - 1) == '\n');
    int index = viewportPosition;
    while (index < max) {
        index = doLayout(index, l.isEmpty() ? 0 : &l);
    }
    layoutEnd = index;

    qDeleteAll(unusedTextLayouts);
    unusedTextLayouts.clear();
    Q_ASSERT(viewportPosition < layoutEnd ||
             (viewportPosition == layoutEnd && viewportPosition == document->documentSize()));
}

QTextLayout *TextLayout::layoutForPosition(int pos, int *offset, int *index) const
{
    if (offset)
        *offset = -1;
    if (index)
        *index = -1;

    if (textLayouts.isEmpty() || pos < viewportPosition || pos > layoutEnd) {
        return 0;
    }

    int textLayoutOffset = viewportPosition;
    int i = 0;

    foreach(QTextLayout *l, textLayouts) {
        if (pos >= textLayoutOffset && pos <= l->text().size() + textLayoutOffset) {
            if (offset)
                *offset = pos - textLayoutOffset;
            if (index)
                *index = i;
            return l;
        }
        ++i;
        textLayoutOffset += l->text().size() + 1;
    }
    return 0;
}

QTextLine TextLayout::lineForPosition(int pos, int *offsetInLine, int *lineIndex) const
{
    if (offsetInLine)
        *offsetInLine = -1;
    if (lineIndex)
        *lineIndex = -1;

    if (pos < viewportPosition || pos >= layoutEnd || textLayouts.isEmpty() || lines.isEmpty()) {
        return QTextLine();
    }
    for (int i=0; i<lines.size(); ++i) {
        const QPair<int, QTextLine> &line = lines.at(i);
        const int lineEnd = line.first + line.second.textLength() + 1; // 1 is for newline characteru
        if (pos < lineEnd) {
            if (offsetInLine) {
                *offsetInLine = pos - line.first;
                Q_ASSERT(*offsetInLine >= 0);
                Q_ASSERT(*offsetInLine < lineEnd + pos);
            }
            if (lineIndex) {
                *lineIndex = i;
            }
            return line.second;
        }
    }
    qWarning() << "Couldn't find a line for" << pos << "viewportPosition" << viewportPosition
               << "layoutEnd" << layoutEnd;
    Q_ASSERT(0);
    return QTextLine();
}

// pos is not necessarily on a newline. Finds closest newline in the
// right direction and sets viewportPosition to that. Updates
// scrollbars if this is a TextEditPrivate

void TextLayout::updatePosition(int pos, Direction direction)
{
    if (document->documentSize() == 0) {
        viewportPosition = 0;
    } else {
        Q_ASSERT(document->documentSize() > 0);
        int index = document->find('\n', qMax(0, pos + (direction == Backward ? -1 : 0)),
                                   TextDocument::FindMode(direction)).position();
        if (index == -1) {
            if (direction == Backward) {
                index = 0;
            } else {
                index = document->find('\n', document->documentSize() - 1, TextDocument::FindBackward).position() + 1;
                // position after last newline in document
            }
        } else {
            ++index;
        }
        Q_ASSERT(index != -1);
        viewportPosition = index;

        if (viewportPosition != 0 && document->read(viewportPosition - 1, 1) != QString("\n"))
            qWarning() << "viewportPosition" << viewportPosition << document->read(viewportPosition - 1, 10) << this;
        Q_ASSERT(viewportPosition == 0 || document->read(viewportPosition - 1, 1) == QString("\n"));
    }
    dirty(viewportWidth());

    if (textEdit) {
        TextEditPrivate *p = static_cast<TextEditPrivate*>(this);
        p->pendingScrollBarUpdate = true;
        p->updateCursorPosition(p->lastHoverPos);
        if (!textEdit->verticalScrollBar()->isSliderDown()) {
            p->updateScrollBar();
        } // sliderReleased is connected to updateScrollBar()
    }
}

#ifndef QT_NO_DEBUG_STREAM
QDebug &operator<<(QDebug &str, const QTextLine &line)
{
    if (!line.isValid()) {
        str << "QTextLine() (invalid)";
        return str;
    }
    str.space() << "lineNumber" << line.lineNumber()
                << "textStart" << line.textStart()
                << "textLength" << line.textLength()
                << "position" << line.position();
    return str;
}

#endif
