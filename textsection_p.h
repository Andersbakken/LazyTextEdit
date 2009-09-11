#ifndef TEXTSECTION_P_H
#define TEXTSECTION_P_H

#include <QObject>
#include <QCoreApplication>
class TextSection;
class TextSectionManager : public QObject
{
    Q_OBJECT
public:
    static TextSectionManager *instance() { static TextSectionManager *inst = new TextSectionManager; return inst; }
signals:
    void sectionFormatChanged(TextSection *section);
    void sectionCursorChanged(TextSection *section);
private:
    TextSectionManager() : QObject(QCoreApplication::instance()) {}
    friend class TextSection;
};

#endif
