#include "textsection.h"
#include "textdocument.h"
#include "textdocument_p.h"

TextSection::~TextSection()
{
    if (d.document)
        d.document->takeTextSection(this);
}

QString TextSection::text() const
{
    Q_ASSERT(d.document);
    return d.document->read(d.position, d.size);
}

void TextSection::setFormat(const QTextCharFormat &format)
{
    Q_ASSERT(d.document);
    d.format = format;
    emit TextSectionManager::instance()->sectionFormatChanged(this);
}

QCursor TextSection::cursor() const
{
    return d.cursor;
}

void TextSection::setCursor(const QCursor &cursor)
{
    d.cursor = cursor;
    d.hasCursor = true;
}

void TextSection::resetCursor()
{
    d.hasCursor = false;
    d.cursor = QCursor();
}

bool TextSection::hasCursor() const
{
    return d.hasCursor;
}
