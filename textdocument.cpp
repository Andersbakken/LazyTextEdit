#include "textdocument.h"
#include "textcursor.h"
#include "textcursor_p.h"
#include "textdocument_p.h"
#include <QDebug>
#include <QBuffer>
#include <QObject>
#include <QString>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QTextCharFormat>
#include <QVariant>

Section::~Section()
{
    if (d.document)
        d.document->takeSection(this);
}

QString Section::text() const
{
    Q_ASSERT(d.document);
    return d.document->read(d.position, d.size);
}

void Section::setFormat(const QTextCharFormat &format)
{
    Q_ASSERT(d.document);
    d.format = format;
    emit SectionManager::instance()->sectionFormatChanged(this);
}

//#define DEBUG_CACHE_HITS

TextDocument::TextDocument(QObject *parent)
    : QObject(parent), d(new TextDocumentPrivate(this))
{
}

TextDocument::~TextDocument()
{
    foreach(TextCursorSharedPrivate *cursor, d->textCursors) {
        cursor->document = 0;
    }
    Chunk *c = d->first;
    while (c) {
        Chunk *tmp = c;
        c = c->next;
        delete tmp;
    }
    if (d->ownDevice)
        delete d->device;

    delete d;
}

bool TextDocument::load(QIODevice *device, DeviceMode mode)
{
    Q_ASSERT(device);
    if (!device->isReadable())
        return false;

    if (d->documentSize > 0) {
        emit charactersRemoved(0, d->documentSize);
    }

    Chunk *c = d->first;
    while (c) {
        Chunk *tmp = c;
        c = c->next;
        delete tmp;
    }

    d->documentSize = device->size();
    d->first = d->last = 0;

    if (d->device) {
        disconnect(d->device, SIGNAL(destroyed(QObject*)), d, SLOT(onDeviceDestroyed(QObject*)));
        if (d->ownDevice)
            delete d->device;
    }

    connect(device, SIGNAL(destroyed(QObject*)), d, SLOT(onDeviceDestroyed(QObject*)));
    d->ownDevice = false;
    d->device = device;
    d->deviceMode = mode;
#ifndef NO_TEXTDOCUMENT_CACHE
    d->cachedChunk = 0;
    d->cachedChunkPos = -1;
    d->cachedChunkData.clear();
    d->cachePos = -1;
    d->cache.clear();
#endif

    switch (d->deviceMode) {
    case LoadAll: {
        device->seek(0);
        Chunk *current = 0;
        do {
            Chunk *c = new Chunk;
            c->data = device->read(d->chunkSize);
            if (current) {
                current->next = c;
                c->previous = current;
            } else {
                d->first = c;
            }
            current = c;
        } while (!device->atEnd());

        d->last = current;
        break; }

    case Sparse: {
        int index = 0;
        Chunk *current = 0;
        do {
            Chunk *chunk = new Chunk;
            chunk->from = index;
            chunk->length = qMin<int>(d->documentSize - index, d->chunkSize);
            if (!current) {
                d->first = chunk;
            } else {
                chunk->previous = current;
                current->next = chunk;
            }
            current = chunk;
            index += chunk->length;
        } while (index < d->documentSize);

        d->last = current;
        break; }
    }
    emit charactersAdded(0, d->documentSize);
    emit documentSizeChanged(d->documentSize);
    return true;
}

bool TextDocument::load(const QString &fileName, DeviceMode mode)
{
    if (mode == LoadAll) {
        QFile from(fileName);
        return from.open(QIODevice::ReadOnly) && load(&from, mode);
    } else {
        QFile *file = new QFile(fileName);
        if (file->open(QIODevice::ReadOnly) && load(file, mode)) {
            d->ownDevice = true;
            return true;
        } else {
            delete file;
            d->device = 0;
            d->ownDevice = false;
            return false;
        }
    }
}

void TextDocument::clear()
{
    setText(QString());
}

QString TextDocument::read(int pos, int size) const
{
    Q_ASSERT(size >= 0);
    if (size == 0 || pos == d->documentSize) {
        return QString();
    }

#ifndef NO_TEXTDOCUMENT_CACHE
#ifdef DEBUG_CACHE_HITS
    static int hits = 0;
    static int misses = 0;
#endif
    if (d->cachePos != -1 && pos >= d->cachePos && d->cache.size() - (pos - d->cachePos) >= size) {
#ifdef DEBUG_CACHE_HITS
        qDebug() << "read hits" << ++hits << "misses" << misses;
#endif
        return d->cache.mid(pos - d->cachePos, size);
    }
#ifdef DEBUG_CACHE_HITS
    qDebug() << "read hits" << hits << "misses" << ++misses;
#endif
#endif

    QString ret(size, '\0');
    int written = 0;
    int offset;
    Chunk *c = d->chunkAt(pos, &offset);
    Q_ASSERT(c);
    int chunkPos = pos - offset;

    while (written < size && c) {
        const int max = qMin(size - written, c->size() - offset);
        const QString data = d->chunkData(c, chunkPos);
        chunkPos += data.size();
        ret.replace(written, max, data.constData() + offset, max);
        written += max;
        offset = 0;
        c = c->next;
    }

    if (written < size) {
        ret.truncate(written);
    }
    Q_ASSERT(!c || written == size);
#ifndef NO_TEXTDOCUMENT_CACHE
    d->cachePos = pos;
    d->cache = ret;
#endif
    return ret;
}

bool TextDocument::save(const QString &file)
{
    QFile from(file);
    return from.open(QIODevice::WriteOnly) && save(&from);
}

static bool isSameFile(const QIODevice *left, const QIODevice *right)
{
    if (const QFile *lf = qobject_cast<const QFile *>(left)) {
        if (const QFile *rf = qobject_cast<const QFile *>(right)) {
            return QFileInfo(*lf) == QFileInfo(*rf);
        }
    }
    return false;
}

bool TextDocument::save(QIODevice *device)
{
    Q_ASSERT(device);
    if (d->device == device || (d->device && isSameFile(d->device, device))) {
        qWarning() << "This might not work when saving to the same file. Need to do that in a clever way somehow";
        return false;
    }
    Q_ASSERT(device);
    if (!device->isWritable() || !d->first) {
        return false;
    }
    d->saveState = TextDocumentPrivate::Saving;
    const Chunk *c = d->first;
    emit saveProgress(0);
    int written = 0;
    QTextStream ts(device);
    while (c) {
        ts << c->data;
        written += c->data.size();
        if (c != d->last) {
            const double part = qreal(written) / double(d->documentSize);
            emit saveProgress(int(part * 100.0));
        }
        if (d->saveState == TextDocumentPrivate::AbortSave) {
            d->saveState = TextDocumentPrivate::NotSaving;
            return false;
        }
        c = c->next;
    }
    d->saveState = TextDocumentPrivate::NotSaving;
    emit saveProgress(100);
    return true;
}
int TextDocument::documentSize() const
{
    return d->documentSize;
}

TextDocument::DeviceMode TextDocument::deviceMode() const
{
    return d->deviceMode;
}

TextCursor TextDocument::find(const QRegExp &rx, int pos, FindMode flags) const
{
    if (flags & FindWholeWords) {
        qWarning("FindWholeWords doesn't work with regexps. Instead use RegExp for this");
    }

    QRegExp regexp = rx;
    if ((rx.caseSensitivity() == Qt::CaseSensitive) != (flags & FindCaseSensitively))
        regexp.setCaseSensitivity(flags & FindCaseSensitively ? Qt::CaseSensitive : Qt::CaseInsensitive);

    const TextDocumentIterator::Direction direction = (flags & FindBackward
                                                       ? TextDocumentIterator::Left
                                                       : TextDocumentIterator::Right);
    TextDocumentIterator it(d, pos);
    const QLatin1Char newline('\n');
    int last = pos;
    bool ok = true;
    do {
        while ((it.nextPrev(direction, ok) != newline) && ok) ;
        int from = qMin(it.position(), last);
        int to = qMax(it.position(), last);
        if (!ok) {
            if (direction == TextDocumentIterator::Right)
                ++to;
        } else if (direction == TextDocumentIterator::Left) {
            ++from;
        }
        const QString line = read(from, to - from);
        last = it.position() + 1;
        const int index = regexp.indexIn(line);
        if (index != -1) {
            TextCursor cursor(this);
            cursor.setPosition(from + index);
            return cursor;
        }
    } while (ok);
    return TextCursor();
}

TextCursor TextDocument::find(const QString &in, int pos, FindMode flags) const
{
    Q_ASSERT(pos >= 0 && pos <= d->documentSize);
    if (in.isEmpty())
        return TextCursor();


    const bool reverse = flags & FindBackward;
    if (pos == d->documentSize) {
        if (!reverse)
            return TextCursor();
        --pos;
    }

    const bool caseSensitive = flags & FindCaseSensitively;
    const bool wholeWords = flags & FindWholeWords;

    // ### what if one searches for a string with non-word characters in it and FindWholeWords?
    const TextDocumentIterator::Direction direction = (reverse ? TextDocumentIterator::Left : TextDocumentIterator::Right);
    QString word = caseSensitive ? in : in.toLower();
    if (reverse) {
        QString tmp = word;
        for (int i=0; i<word.size(); ++i) {
            word[i] = tmp.at(word.size() - i - 1);
        }
    }
    TextDocumentIterator it(d, pos);
    if (!caseSensitive)
        it.setConvertToLowerCase(true);

    bool ok = true;
    QChar ch = it.current();
    int wordIndex = 0;
    do {
        if (ch == word.at(wordIndex)) {
            if (++wordIndex == word.size()) {
                break;
            }
        } else if (wordIndex != 0) {
            wordIndex = 0;
            continue;
        }
        ch = it.nextPrev(direction, ok);
    } while (ok);

    if (ok) {
        int pos = it.position() - (reverse ? 0 : word.size() - 1);
        // the iterator reads one past the last matched character so we have to account for that here

        if (wholeWords &&
            ((pos != 0 && TextDocumentPrivate::isWord(readCharacter(pos - 1)))
             || (pos + word.size() < d->documentSize
                 && TextDocumentPrivate::isWord(readCharacter(pos + word.size()))))) {
            // checking if the characters before and after are word characters
            pos += reverse ? -1 : 1;
            if (pos < 0 || pos >= d->documentSize)
                return TextCursor();
            return find(word, pos, flags);
        }

        TextCursor cursor(this, pos);
        return cursor;
    }

    return TextCursor();
}

TextCursor TextDocument::find(const QChar &chIn, int pos, FindMode flags) const
{
    if (flags & FindWholeWords) {
        qWarning("FindWholeWords makes not sense searching for characters");
    }

    Q_ASSERT(pos >= 0 && pos <= d->documentSize);

    const bool caseSensitive = flags & FindCaseSensitively;
    const QChar ch = (caseSensitive ? chIn : chIn.toLower());
    TextDocumentIterator it(d, pos);
    const TextDocumentIterator::Direction dir = (flags & FindBackward
                                                 ? TextDocumentIterator::Left
                                                 : TextDocumentIterator::Right);

    QChar c = it.current();
    bool ok = true;
    do {
        if ((caseSensitive ? c : c.toLower()) == ch) {
            TextCursor cursor(this);
            cursor.setPosition(it.position());
            return cursor;
        }
        c = it.nextPrev(dir, ok);
    } while (ok);

    return TextCursor();
}

bool TextDocument::insert(int pos, const QString &ba)
{
    Q_ASSERT(pos >= 0 && pos <= d->documentSize);
    if (ba.isEmpty())
        return false;

    if (sectionAt(pos))
        return false;

    const bool undoAvailable = isUndoAvailable();
    DocumentCommand *cmd = 0;
    if (!d->ignoreUndoRedo && d->undoRedoEnabled) {
        d->clearRedo();
        if (!d->undoRedoStack.isEmpty()
            && d->undoRedoStack.last()->type == DocumentCommand::Inserted
            && d->undoRedoStack.last()->position + d->undoRedoStack.last()->text.size() == pos) {
            d->undoRedoStack.last()->text += ba;
        } else {
            cmd = new DocumentCommand(DocumentCommand::Inserted, pos, ba);
            emit d->undoRedoCommandInserted(cmd);
            d->undoRedoStack.append(cmd);
            ++d->undoRedoStackCurrent;
            Q_ASSERT(d->undoRedoStackCurrent == d->undoRedoStack.size());
        }
    }

    Chunk *c;
    int offset;
    c = d->chunkAt(pos, &offset);
    d->instantiateChunk(c);
    c->data.insert(offset, ba);
    d->documentSize += ba.size();
#ifndef NO_TEXTDOCUMENT_CACHE
    if (pos <= d->cachePos) {
        d->cachePos += ba.size();
    } else if (pos < d->cachePos + d->cache.size()) {
        d->cachePos = -1;
        d->cache.clear();
    }
    if (d->cachedChunk && pos <= d->cachedChunkPos) {
        d->cachedChunkPos += ba.size();
    }
#endif
    foreach(TextCursorSharedPrivate *cursor, d->textCursors) {
        if (cursor->position >= pos)
            cursor->position += ba.size();
        if (cursor->anchor >= pos)
            cursor->anchor += ba.size();
    }
    emit charactersAdded(pos, ba.size());
    emit documentSizeChanged(d->documentSize);
    if (isUndoAvailable() != undoAvailable) {
        emit undoAvailableChanged(!undoAvailable);
    }
    if (cmd)
        emit d->undoRedoCommandFinished(cmd);
    return true;
}

void TextDocument::remove(int pos, int size)
{
    Q_ASSERT(pos >= 0 && pos + size <= d->documentSize);
    Q_ASSERT(size >= 0);

    if (size == 0)
        return;

    const QList<Section*> lnks = sections(pos, size, IncludePartial);

    bool changed = false;
    foreach(Section *l, lnks) {
        if (l->position() < pos) {
            size += (pos - l->position());
            pos = l->position();
            changed = true;
        }
        if (l->position() + l->size() > pos + size) {
            size += (l->position() + l->size()) - (pos + size);
            changed = true;
        }
    }
    if (changed) {
        remove(pos, size);
        return;
    }
    qDeleteAll(lnks); // presumably I need to be able to undo this

    DocumentCommand *cmd = 0;
    const bool undoAvailable = isUndoAvailable();
    if (!d->ignoreUndoRedo && d->undoRedoEnabled) {
        d->clearRedo();
        if (!d->undoRedoStack.isEmpty()
            && d->undoRedoStack.last()->type == DocumentCommand::Removed
            && d->undoRedoStack.last()->position == pos + size) {
            d->undoRedoStack.last()->text.prepend(read(pos, size));
            d->undoRedoStack.last()->position -= size;
        } else {
            cmd = new DocumentCommand(DocumentCommand::Removed, pos, read(pos, size));
            emit d->undoRedoCommandInserted(cmd);
            d->undoRedoStack.append(cmd);
            ++d->undoRedoStackCurrent;
            Q_ASSERT(d->undoRedoStackCurrent == d->undoRedoStack.size());
        }
    }


    foreach(TextCursorSharedPrivate *cursor, d->textCursors) {
        if (cursor->position >= pos)
            cursor->position -= qMin(size, cursor->position - pos);
        if (cursor->anchor >= pos)
            cursor->anchor -= qMin(size, cursor->anchor - pos);
    }

    const int s = size;
    while (size > 0) {
        int offset;
        Chunk *c = d->chunkAt(pos, &offset);
        if (offset == 0 && size >= c->size()) {
            size -= c->size();
            d->removeChunk(c);
        } else {
            d->instantiateChunk(c);
            const int removed = qMin(size, c->size() - offset);
            c->data.remove(offset, removed);
            size -= removed;
        }
    }
    if (!d->first) {
        d->first = d->last = new Chunk;
    }

    d->documentSize -= s;
    Q_ASSERT(size == 0);

#ifndef NO_TEXTDOCUMENT_CACHE
    if (pos + size < d->cachePos) {
        d->cachePos -= size;
    } else if (pos <= d->cachePos + d->cache.size()) {
        d->cachePos = -1;
        d->cache.clear();
    }
    if (pos + size < d->cachedChunkPos) {
        d->cachedChunkPos -= size;
    }
#endif
    foreach(Section *section, sections(pos, -1)) {
        section->d.position -= size;
    }

    emit charactersRemoved(pos, size);
    emit documentSizeChanged(d->documentSize);
    if (isUndoAvailable() != undoAvailable) {
        emit undoAvailableChanged(!undoAvailable);
    }
    if (cmd)
        emit d->undoRedoCommandFinished(cmd);
}

typedef QList<Section*>::iterator SectionIterator;
static inline bool compareSection(const Section *left, const Section *right)
{
    // don't make this compare document. Look at ::sections()
    return left->position() < right->position();
}

static inline bool match(int pos, int left, int size)
{
    return pos >= left && pos < left + size;
}

static inline bool match(int pos, int size, const Section *section, TextDocument::SectionOptions flags)
{
    const int sectionPos = section->position();
    const int sectionSize = section->size();

    if (::match(sectionPos, pos, size) && ::match(sectionPos + sectionSize - 1, pos, size)) {
        return true;
    } else if (flags & TextDocument::IncludePartial) {
        const int boundaries[] = { pos, pos + size - 1 };
        for (int i=0; i<2; ++i) {
            if (::match(boundaries[i], sectionPos, sectionSize))
                return true;
        }
    }
    return false;
}


void TextDocument::takeSection(Section *section)
{
    Q_ASSERT(section);
    Q_ASSERT(section->document() == this);
    const SectionIterator it = qBinaryFind(d->sections.begin(), d->sections.end(), section, compareSection);
    Q_ASSERT(it != d->sections.end());
    emit sectionRemoved(section);
    d->sections.erase(it);
    remove(section->position(), section->size());
}

QList<Section*> TextDocument::sections(int pos, int size, SectionOptions flags) const
{
    if (size == -1)
        size = d->documentSize - pos;
    if (pos == 0 && size == d->documentSize)
        return d->sections;
    // binary search. Sections are sorted in order of position
    QList<Section*> ret;
    if (d->sections.isEmpty()) {
        return ret;
    }

    const Section tmp(pos, size, 0);
    SectionIterator it = qLowerBound<SectionIterator>(d->sections.begin(), d->sections.end(), &tmp, compareSection);
    if (flags & IncludePartial && it != d->sections.begin()) {
        SectionIterator prev = it;
        do {
            if (::match(pos, size, *--prev, flags))
                ret.append(*prev);
        } while (prev != d->sections.begin());
    }
    while (it != d->sections.end()) {
        if (::match(pos, size, *it, flags)) {
            ret.append(*it);
        } else {
            break;
        }
        ++it;
    }
    return ret;
}

void TextDocument::removeSection(Section *section)
{
    takeSection(section);
    delete section;
}

Section *TextDocument::insertSection(int pos, int size,
                                     const QTextCharFormat &format, const QVariant &data)
{
    Q_ASSERT(pos >= 0);
    Q_ASSERT(size >= 0);
    Q_ASSERT(pos < d->documentSize);

    Section *l = new Section(pos, size, this, format, data);
    SectionIterator it = qLowerBound<SectionIterator>(d->sections.begin(), d->sections.end(), l, compareSection);
    if (it != d->sections.begin()) {
        SectionIterator before = (it - 1);
        if ((*before)->position() + size > pos) {
            l->d.document = 0; // don't want it to call takeSection since it isn't in the list yet
            delete l;
            return 0;
            // no overlapping. Not all that awesome to construct the
            // Section first and then delete it but I might allow
            // overlapping soon enough
        }
    }
    if (it != d->sections.end() && pos + size > (*it)->position()) {
        l->d.document = 0; // don't want it to call takeSection since it isn't in the list yet
        delete l;
        return 0;
        // no overlapping. Not all that awesome to construct the
        // Section first and then delete it but I might allow
        // overlapping soon enough
    }
    d->sections.insert(it, l);
    emit sectionAdded(l);
    return l;
}

bool TextDocument::abortSave()
{
    if (d->saveState == TextDocumentPrivate::Saving) {
        d->saveState = TextDocumentPrivate::AbortSave;
        return true;
    }
    return false;
}

QChar TextDocument::readCharacter(int pos) const
{
    if (pos == d->documentSize)
        return QChar();

#ifndef NO_TEXTDOCUMENT_CACHE
#ifdef DEBUG_CACHE_HITS
    static int hits = 0;
    static int misses = 0;
#endif
    if (pos >= d->cachePos && pos < d->cachePos + d->cache.size()) {
#ifdef DEBUG_CACHE_HITS
        qDebug() << "readCharacter hits" << ++hits << "misses" << misses;
#endif
        return d->cache.at(pos - d->cachePos);
    }
#ifdef DEBUG_CACHE_HITS
    qDebug() << "readCharacter hits" << hits << "misses" << ++misses;
#endif
#endif

    int offset;
    Chunk *c = d->chunkAt(pos, &offset);
    return d->chunkData(c, pos - offset).at(offset);
}

void TextDocument::setText(const QString &text)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QTextStream ts(&buffer);
    ts << text;
    buffer.close();
    buffer.open(QIODevice::ReadOnly);
    const bool ret = load(&buffer, LoadAll);
    d->device = 0;
    Q_ASSERT(ret);
    Q_UNUSED(ret);
}

int TextDocument::chunkSize() const
{
    return d->chunkSize;
}

void TextDocument::setChunkSize(int size)
{
    Q_ASSERT(d->chunkSize > 0);
    d->chunkSize = size;
}

int TextDocument::currentMemoryUsage() const
{
    Chunk *c = d->first;
    int used = 0;
    while (c) {
        used += c->data.size() * sizeof(QChar);
        c = c->next;
    }
    return used;
}

void TextDocument::undo()
{
    if (!isUndoAvailable())
        return;
    d->undoRedo(true);
}

void TextDocument::redo()
{
    if (!isRedoAvailable())
        return;
    d->undoRedo(false);
}

bool TextDocument::isUndoRedoEnabled() const
{
    return d->undoRedoEnabled;
}

void TextDocument::setUndoRedoEnabled(bool enable)
{
    if (enable == d->undoRedoEnabled)
        return;
    d->undoRedoEnabled = enable;
    if (!enable) {
        qDeleteAll(d->undoRedoStack);
        d->undoRedoStack.clear();
        d->undoRedoStackCurrent = 0;
    }
}

bool TextDocument::isUndoAvailable() const
{
    return d->undoRedoStackCurrent > 0;
}

bool TextDocument::isRedoAvailable() const
{
    return d->undoRedoStackCurrent < d->undoRedoStack.size();
}

// --- TextDocumentPrivate ---

Chunk *TextDocumentPrivate::chunkAt(int p, int *offset) const
{
    Q_ASSERT(p <= documentSize);
    Q_ASSERT(p >= 0);
    Q_ASSERT(first);
    Q_ASSERT(last);
    if (p == documentSize) {
        if (offset)
            *offset = last->size();
        return last;
    }
#ifndef NO_TEXTDOCUMENT_CACHE
    Q_ASSERT(!cachedChunk || cachedChunkPos != -1);
    if (cachedChunk && p >= cachedChunkPos && p < cachedChunkPos + cachedChunkData.size()) {
        if (offset)
            *offset = p - cachedChunkPos;
        return cachedChunk;
    }
#endif
    int pos = p;
    Chunk *c = first;

    forever {
        const int size = c->size();
        if (pos < size) {
            break;
        }
        pos -= size;
        c = c->next;
        Q_ASSERT(c);
    }

    if (offset)
        *offset = pos;

    Q_ASSERT(c);
    return c;
}

void TextDocumentPrivate::clearRedo()
{
    const bool doEmit = undoRedoStackCurrent < undoRedoStack.size();
    while (undoRedoStackCurrent < undoRedoStack.size()) {
        DocumentCommand *cmd = undoRedoStack.takeLast();
        emit undoRedoCommandRemoved(cmd);
        delete cmd;
    }
    if (doEmit) {
        emit q->redoAvailableChanged(false);
    }
}

/* Evil double meaning of pos here. If it's -1 we don't cache it. */
QString TextDocumentPrivate::chunkData(const Chunk *chunk, int chunkPos) const
{
#ifndef NO_TEXTDOCUMENT_CACHE
#ifdef DEBUG_CACHE_HITS
    static int hits = 0;
    static int misses = 0;
#endif
    if (chunk == cachedChunk) {
#ifdef DEBUG_CACHE_HITS
        qDebug() << "chunkData hits" << ++hits << "misses" << misses;
#endif
        return cachedChunkData;
    } else
#endif
    if (chunk->from == -1) {
        return chunk->data;
    } else if (!device) {
        // Can only happen if the device gets deleted behind our back when in Sparse mode
        return QString().fill(QLatin1Char(' '), chunk->size());
    } else {
        QTextStream ts(device);
        ts.seek(chunk->from);
        const QString data = ts.read(chunk->length);
#ifndef NO_TEXTDOCUMENT_CACHE
#ifdef DEBUG_CACHE_HITS
        qDebug() << "chunkData hits" << hits << "misses" << ++misses;
#endif
        if (chunkPos != -1) {
            cachedChunk = const_cast<Chunk*>(chunk);
            cachedChunkData = data;
            cachedChunkPos = chunkPos;
        }
#endif
        return data;
    }
}

void TextDocumentPrivate::instantiateChunk(Chunk *chunk)
{
    if (chunk->from == -1 || !device)
        return;
    chunk->data = chunkData(chunk, -1);
#ifndef NO_TEXTDOCUMENT_CACHE
    // Don't want to cache this chunk since it's going away. If it
    // already was cached then sure, but otherwise don't
    if (chunk == cachedChunk) {
        cachedChunk = 0;
        cachedChunkPos = -1;
        cachedChunkData.clear();
    }
#endif
    chunk->from = chunk->length = -1;
}

void TextDocumentPrivate::removeChunk(Chunk *c)
{
    Q_ASSERT(c);
    if (c == first) {
        first = c->next;
    } else {
        c->previous->next = c->next;
    }
    if (c == last) {
        last = c->previous;
    } else {
        c->next->previous = c->previous;
    }
#ifndef NO_TEXTDOCUMENT_CACHE
    if (c == cachedChunk) {
        cachedChunk = 0;
        cachedChunkPos = -1;
        cachedChunkData.clear();
    }
#endif
    delete c;
}

void TextDocumentPrivate::undoRedo(bool undo)
{
    const bool undoWasAvailable = q->isUndoAvailable();
    const bool redoWasAvailable = q->isRedoAvailable();
    DocumentCommand *cmd = undoRedoStack.at(undo
                                            ? --undoRedoStackCurrent
                                            : undoRedoStackCurrent++);
    const bool was = ignoreUndoRedo;
    ignoreUndoRedo = true;
    Q_ASSERT(cmd->type != DocumentCommand::None);
    if ((cmd->type == DocumentCommand::Inserted) == undo) {
        q->remove(cmd->position, cmd->text.size());
    } else {
        q->insert(cmd->position, cmd->text);
    }
    emit undoRedoCommandTriggered(cmd, undo);
    ignoreUndoRedo = was;

    if ((cmd->joined == DocumentCommand::Backward && undo)
        || (cmd->joined == DocumentCommand::Forward && !undo)) {
        undoRedo(undo);
    }
    if (undoWasAvailable != q->isUndoAvailable())
        emit q->undoAvailableChanged(!undoWasAvailable);
    if (redoWasAvailable != q->isRedoAvailable())
        emit q->redoAvailableChanged(!redoWasAvailable);
}

QString TextDocumentPrivate::wordAt(int position, int *start) const
{
    TextDocumentIterator from(this, position);
    if (!isWord(from.current())) {
        if (start)
            *start = -1;
        return QString();
    }

    while (from.hasPrevious()) {
        if (!isWord(from.previous())) {
            from.next();
            break;
        }
    }
    TextDocumentIterator to(this, position);
    while (to.hasNext()) {
        if (!isWord(to.next())) {
            to.previous();
            break;
        }
    }

    if (start)
        *start = from.position();
    return q->read(from.position(), to.position() - from.position());
}

QString TextDocumentPrivate::paragraphAt(int position, int *start) const
{
    const QLatin1Char newline('\n');
    TextDocumentIterator from(this, position);
    while (from.hasPrevious() && from.previous() != newline)
        ;
    TextDocumentIterator to(this, position);
    while (to.hasNext() && to.next() != newline)
        ;
    if (start)
        *start = from.position();
    return q->read(from.position(), to.position() - from.position());
}

void TextDocumentPrivate::joinLastTwoCommands()
{
    Q_ASSERT(!q->isRedoAvailable());
    Q_ASSERT(undoRedoStack.size() >= 2);
    undoRedoStack.last()->joined = DocumentCommand::Backward;
    undoRedoStack.at(undoRedoStack.size() - 2)->joined = DocumentCommand::Forward;
}

void TextDocumentPrivate::onDeviceDestroyed(QObject *o)
{
    Q_ASSERT(o == device || !device);
    device = 0;
}
