#include "textdocument_find_p.h"
#include "textdocument.h"

CharNeedle::CharNeedle(uint flags, const QChar &ch)
    : Needle(flags)
{
    d.ch = !(flags & TextDocument::FindCaseSensitively) ? ch.toLower() : ch;
}
bool CharNeedle::addChar(const QChar &ch)
{
    return ch == d.ch;
}

int CharNeedle::matchedLength() const
{
    return 1;
}

StringNeedle::StringNeedle(uint flags, const QString &string)
    : Needle(flags)
{
    d.string = !(flags & TextDocument::FindCaseSensitively) ? string.toLower() : string;
    d.index = 0;
}

bool StringNeedle::addChar(const QChar &ch)
{
}

int StringNeedle::matchedLength() const
{
    return d.string.size();
}


RegExpNeedle::RegExpNeedle(uint flags, const QRegExp &regExp)
    : Needle(flags)
{
    d.regExp = regExp;
}

bool RegExpNeedle::addChar(const QChar &ch)
{

}

int RegExpNeedle::matchedLength() const
{
    return d.regExp.size();
}

