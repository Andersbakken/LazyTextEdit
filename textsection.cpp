#include "textsection.h"
#include "textdocument.h"
#include "textdocument_p.h"

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
