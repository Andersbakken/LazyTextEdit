#ifndef TEXTEDIT_P_H
#define TEXTEDIT_P_H

#include <QBasicTimer>
#include <QPoint>
#include <QAction>
#include "textlayout_p.h"
#include "textdocument_p.h"
#include "textcursor.h"
#include "textedit.h"

struct DocumentCommand;
struct CursorData {
    int position, anchor;
};

class TextEditPrivate : public QObject, public TextLayout
{
    Q_OBJECT
public:
    TextEditPrivate(TextEdit *qptr)
        : requestedScrollBarPosition(-1), lastRequestedScrollBarPosition(-1), cursorWidth(1),
        sectionCount(0), maximumSizeCopy(50000), pendingTimeOut(-1), autoScrollLines(-1),
        readOnly(false), cursorVisible(false), blockScrollBarUpdate(false), updateScrollBarPageStepPending(true),
        ensureCursorVisiblePending(false), sectionPressed(0), sectionHovered(0), pendingScrollBarUpdate(false)
    {
        textEdit = qptr;
        connect(SectionManager::instance(), SIGNAL(sectionFormatChanged(Section *)),
                this, SLOT(onSectionFormatChanged(Section *)));
    }

    void updateHorizontalPosition();
    void updateScrollBarPosition();
    void updateScrollBarPageStep();
    void scrollLines(int lines);
    void timerEvent(QTimerEvent *e);
    void updateCursorPosition(const QPoint &pos);
    bool atBeginning() const { return viewportPosition == 0; }
    bool atEnd() const { return textEdit->verticalScrollBar()->value() == textEdit->verticalScrollBar()->maximum(); }

    int requestedScrollBarPosition, lastRequestedScrollBarPosition, cursorWidth, sectionCount,
        maximumSizeCopy, pendingTimeOut, autoScrollLines;
    bool readOnly, cursorVisible, blockScrollBarUpdate, updateScrollBarPageStepPending,
        ensureCursorVisiblePending;
    QBasicTimer autoScrollTimer, cursorBlinkTimer;
    QAction *actions[TextEdit::SelectAllAction];
    Section *sectionPressed, *sectionHovered;
    TextCursor textCursor;
    QBasicTimer tripleClickTimer;
    bool pendingScrollBarUpdate;
    QPoint lastHoverPos, lastMouseMove;
    QHash<DocumentCommand *, QPair<CursorData, CursorData> > undoRedoCommands;
public slots:
    void onSectionAdded();
    void onSectionRemoved(Section *section);
    void onSectionFormatChanged(Section *section);
    void updateScrollBar();
    void updateCopyAndCutEnabled();
    void onDocumentDestroyed();
    void onDocumentSizeChanged(int size);
    void onDocumentCommandInserted(DocumentCommand *cmd);
    void onDocumentCommandFinished(DocumentCommand *cmd);
    void onDocumentCommandRemoved(DocumentCommand *cmd);
    void onDocumentCommandTriggered(DocumentCommand *cmd, bool undo);
    void onScrollBarValueChanged(int value);
    void onScrollBarActionTriggered(int action);
    void onCharactersAddedOrRemoved(int index, int count);
};

class DebugWindow : public QWidget
{
public:
    DebugWindow(TextEditPrivate *p)
        : priv(p)
    {
    }

    void paintEvent(QPaintEvent *)
    {
        if (priv->lines.isEmpty())
            return;

        QPainter p(this);
        p.fillRect(QRect(0, pixels(priv->viewportPosition), width(),
                         pixels(priv->lines.last().first +
                                priv->lines.last().second.textLength())),
                   Qt::black);
        p.fillRect(QRect(0, pixels(priv->viewportPosition), width(), pixels(priv->layoutEnd)), Qt::red);
    }

    int pixels(int pos) const
    {
        double fraction = double(pos) / double(priv->document->documentSize());
        return int(double(height()) * fraction);
    }
private:
    TextEditPrivate *priv;
};

#endif
