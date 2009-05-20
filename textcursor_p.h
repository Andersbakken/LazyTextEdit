#ifndef TEXTCURSOR_P_H
#define TEXTCURSOR_P_H

#include <QPointer>
#include <QSize>
#include <QList>
#include <QObject>
#include <QHash>
#include <QAtomicInt>
#include <QCoreApplication>
#include "textlayout_p.h"
#include "textedit.h"
#include "textedit_p.h"

static inline bool match(const TextCursor &cursor, const TextLayout *layout, int lines)
{
    Q_ASSERT(cursor.document() == layout->document);
    int index = -1;
    if (!layout->layoutForPosition(cursor.position(), 0, &index)) {
        // ### need an interface for this if I am going to have a mode that doesn't break lines
        return false;
    }

    if (index < lines && layout->viewportPosition > 0) {
        return false;  // need more margin before the cursor
    } else if (layout->textLayouts.size() - index - 1 < lines && layout->layoutEnd < layout->document->documentSize()) {
        return false; // need more margin after cursor
    }
    return true;
}

class TextLayoutCacheManager : public QObject
{
    Q_OBJECT
public:
    static TextLayoutCacheManager *instance()
    {
        static TextLayoutCacheManager *inst = new TextLayoutCacheManager(QCoreApplication::instance());
        return inst;
    }
    // ### this class doesn't react to Sections added or removed. I
    // ### don't think it needs to since it's only being used for
    // ### cursor movement which shouldn't be impacted by these things

    static TextLayout *requestLayout(const TextCursor &cursor, int margin)
    {
        Q_ASSERT(cursor.document());
        if (cursor.textEdit && match(cursor, cursor.textEdit->d, margin)) {
            return cursor.textEdit->d;
        }

        TextDocument *doc = cursor.document();
        Q_ASSERT(doc);
        QList<TextLayout*> &layouts = instance()->cache[doc];
        Q_ASSERT(cursor.document());
        foreach(TextLayout *l, layouts) {
            if (match(cursor, l, margin))
                return l;
        }
        if (layouts.size() < instance()->maxLayouts) {
            if (layouts.isEmpty()) {
                connect(cursor.document(), SIGNAL(charactersAdded(int, int)),
                        instance(), SLOT(onCharactersAddedOrRemoved(int)));
                connect(cursor.document(), SIGNAL(charactersRemoved(int, int)),
                        instance(), SLOT(onCharactersAddedOrRemoved(int)));
                connect(cursor.document(), SIGNAL(destroyed(QObject*)),
                        instance(), SLOT(onDocumentDestroyed(QObject*)));

            }
            layouts.append(new TextLayout(doc));
        }
        TextLayout *l = layouts.last();
        l->viewport = cursor.viewportWidth();
        int startPos = (cursor.position() == 0
                        ? 0
                        : qMax(0, doc->find(QLatin1Char('\n'), cursor.position() - 1, TextDocument::FindBackward).position()));
        // We start at the beginning of the current line
        int linesAbove = margin;
        if (startPos > 0) {
            while (linesAbove > 0) {
                const TextCursor c = doc->find(QLatin1Char('\n'), startPos - 1, TextDocument::FindBackward);
                if (c.isNull()) {
                    startPos = 0;
                    break;
                }
                startPos = c.position();
                ASSUME(c.position() == 0 || c.cursorCharacter() == QLatin1Char('\n'));
                ASSUME(c.position() == 0 || doc->readCharacter(c.position()) == QLatin1Char('\n'));
                ASSUME(c.cursorCharacter() == doc->readCharacter(c.position()));

                --linesAbove;
            }
        }

        int linesBelow = margin;
        int endPos = cursor.position();
        if (endPos < doc->documentSize()) {
            while (linesBelow > 0) {
                const TextCursor c = doc->find(QLatin1Char('\n'), endPos + 1);
                if (c.isNull()) {
                    endPos = doc->documentSize();
                    break;
                }
                endPos = c.position();
                --linesBelow;
            }
        }
        if (startPos > 0)
            ++startPos; // in this case startPos points to the newline before it
        l->viewportPosition = startPos;
        l->dirty(cursor.viewportWidth());
        ASSUME(l->viewportPosition == 0 || doc->readCharacter(l->viewportPosition - 1) == QLatin1Char('\n'));
        l->relayoutByPosition(endPos - startPos + 100); // ### fudged a couple of lines likely
        ASSUME(l->viewportPosition < l->layoutEnd
               || (l->viewportPosition == l->layoutEnd && l->viewportPosition == doc->documentSize()));
        ASSUME(l->textLayouts.size() > margin * 2 || l->viewportPosition == 0 || l->layoutEnd == doc->documentSize());
        return l;
    }
private slots:
    void onDocumentDestroyed(QObject *o)
    {
        qDeleteAll(cache.take(static_cast<TextDocument*>(o)));
    }

    void onCharactersAddedOrRemoved(int pos)
    {
        QList<TextLayout*> &layouts = cache[qobject_cast<TextDocument*>(sender())];
        ASSUME(!layouts.isEmpty());
        for (int i=layouts.size() - 1; i>=0; --i) {
            TextLayout *l = layouts.at(i);
            if (pos <= l->layoutEnd) {
                delete l;
                layouts.removeAt(i);
            }
        }
        if (layouts.isEmpty()) {
            disconnect(sender(), 0, this, 0);
            cache.remove(qobject_cast<TextDocument*>(sender()));
        }
    }
private:
    TextLayoutCacheManager(QObject *parent)
        : QObject(parent)
    {
        maxLayouts = qMax(1, qgetenv("TEXTCURSOR_MAX_CACHED_TEXTLAYOUTS").toInt());
    }

    int maxLayouts;
    QHash<TextDocument*, QList<TextLayout*> > cache;
};

class TextLayout;
struct TextCursorSharedPrivate
{
public:
    TextCursorSharedPrivate() : ref(1), position(-1), anchor(-1),
        overrideColumn(-1), viewportWidth(5000), // ### is this wise? essentially makes for no line breaks
        document(0)
    {}

    ~TextCursorSharedPrivate()
    {
        ASSUME(ref == 0);
    }

    void flipSelection(TextCursor::MoveOperation op)
    {
        ASSUME(ref == 1);
        const int min = qMin(anchor, position);
        const int max = qMax(anchor, position);
        if (min == max)
            return;

        switch (op) {
        case TextCursor::NoMove:
            break;

        case TextCursor::Start:
        case TextCursor::Up:
        case TextCursor::StartOfLine:
        case TextCursor::StartOfBlock:
        case TextCursor::StartOfWord:
        case TextCursor::PreviousBlock:
        case TextCursor::PreviousCharacter:
        case TextCursor::PreviousWord:
        case TextCursor::Left:
        case TextCursor::WordLeft:
            anchor = max;
            position = min;
            break;

        case TextCursor::End:
        case TextCursor::Down:
        case TextCursor::EndOfLine:
        case TextCursor::EndOfWord:
        case TextCursor::EndOfBlock:
        case TextCursor::NextBlock:
        case TextCursor::NextCharacter:
        case TextCursor::NextWord:
        case TextCursor::Right:
        case TextCursor::WordRight:
            anchor = min;
            position = max;
        };
    }

    mutable QAtomicInt ref;
    int position, anchor, overrideColumn, viewportWidth;

    TextDocument *document;
};


#endif
