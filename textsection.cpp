// Copyright 2010 Anders Bakken
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
    emit d.document->d->sectionFormatChanged(this);
}

QCursor TextSection::cursor() const
{
    return d.cursor;
}

void TextSection::setCursor(const QCursor &cursor)
{
    d.cursor = cursor;
    d.hasCursor = true;
    emit d.document->d->sectionCursorChanged(this);
}

void TextSection::resetCursor()
{
    d.hasCursor = false;
    d.cursor = QCursor();
    emit d.document->d->sectionCursorChanged(this);
}

bool TextSection::hasCursor() const
{
    return d.hasCursor;
}

void TextSection::setPriority(int priority)
{
    d.priority = priority;
    emit d.document->d->sectionFormatChanged(this); // ### it hasn't really but I to need it dirtied
}
