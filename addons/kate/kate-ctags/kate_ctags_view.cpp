/* Description : Kate CTags plugin
 *
 * Copyright (C) 2008-2011 by Kare Sars <kare.sars@iki.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kate_ctags_view.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QKeyEvent>

#include <KXMLGUIFactory>
#include <KActionCollection>
#include <KConfigGroup>
#include <QMenu>

#include <klocalizedstring.h>
#include <kstringhandler.h>
#include <kmessagebox.h>
#include <QStandardPaths>

/******************************************************************/
KateCTagsView::KateCTagsView(KTextEditor::Plugin *plugin, KTextEditor::MainWindow *mainWin)
: QObject(mainWin)
, m_proc(0)
{
    KXMLGUIClient::setComponentName (QLatin1String("katectags"), i18n ("Kate CTag"));
    setXMLFile( QLatin1String("ui.rc") );

    m_toolView = mainWin->createToolView(plugin, QLatin1String("kate_plugin_katectagsplugin"),
                                        KTextEditor::MainWindow::Bottom,
                                        QIcon::fromTheme(QStringLiteral("application-x-ms-dos-executable")),
                                        i18n("CTags"));
    m_mWin = mainWin;

    QAction *back = actionCollection()->addAction(QLatin1String("ctags_return_step"));
    back->setText(i18n("Jump back one step"));
    connect(back, SIGNAL(triggered(bool)), this, SLOT(stepBack()));

    QAction *decl = actionCollection()->addAction(QLatin1String("ctags_lookup_current_as_declaration"));
    decl->setText(i18n("Go to Declaration"));
    connect(decl, SIGNAL(triggered(bool)), this, SLOT(gotoDeclaration()));

    QAction *defin = actionCollection()->addAction(QLatin1String("ctags_lookup_current_as_definition"));
    defin->setText(i18n("Go to Definition"));
    connect(defin, SIGNAL(triggered(bool)), this, SLOT(gotoDefinition()));

    QAction *lookup = actionCollection()->addAction(QLatin1String("ctags_lookup_current"));
    lookup->setText(i18n("Lookup Current Text"));
    connect(lookup, SIGNAL(triggered(bool)), this, SLOT(lookupTag()));

    // popup menu
    m_menu = new KActionMenu(i18n("CTags"), this);
    actionCollection()->addAction(QLatin1String("popup_ctags"), m_menu);

    m_gotoDec=m_menu->menu()->addAction(i18n("Go to Declaration: %1",QString()), this, SLOT(gotoDeclaration()));
    m_gotoDef=m_menu->menu()->addAction(i18n("Go to Definition: %1",QString()), this, SLOT(gotoDefinition()));
    m_lookup=m_menu->menu()->addAction(i18n("Lookup: %1",QString()), this, SLOT(lookupTag()));

    connect(m_menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));

    QWidget *ctagsWidget = new QWidget(m_toolView);
    m_ctagsUi.setupUi(ctagsWidget);
    m_ctagsUi.cmdEdit->setText(DEFAULT_CTAGS_CMD);

    m_ctagsUi.addButton->setToolTip(i18n("Add a directory to index."));
    m_ctagsUi.addButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));

    m_ctagsUi.delButton->setToolTip(i18n("Remove a directory."));
    m_ctagsUi.delButton->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));

    m_ctagsUi.updateButton->setToolTip(i18n("(Re-)generate the session specific CTags database."));
    m_ctagsUi.updateButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));

    m_ctagsUi.updateButton2->setToolTip(i18n("(Re-)generate the session specific CTags database."));
    m_ctagsUi.updateButton2->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));

    m_ctagsUi.resetCMD->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));

    m_ctagsUi.tagsFile->setToolTip(i18n("Select new or existing database file."));

    connect(m_ctagsUi.resetCMD, SIGNAL(clicked()), this, SLOT(resetCMD()));
    connect(m_ctagsUi.addButton, SIGNAL(clicked()), this, SLOT(addTagTarget()));
    connect(m_ctagsUi.delButton, SIGNAL(clicked()), this, SLOT(delTagTarget()));
    connect(m_ctagsUi.updateButton,  SIGNAL(clicked()), this, SLOT(updateSessionDB()));
    connect(m_ctagsUi.updateButton2,  SIGNAL(clicked()), this, SLOT(updateSessionDB()));
    connect(&m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), 
            this, SLOT(updateDone(int,QProcess::ExitStatus)));

    connect(m_ctagsUi.inputEdit, SIGNAL(textChanged(QString)), this, SLOT(startEditTmr()));

    m_editTimer.setSingleShot(true);
    connect(&m_editTimer, SIGNAL(timeout()), this, SLOT(editLookUp()));

    connect(m_ctagsUi.tagTreeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)),
            SLOT(tagHitClicked(QTreeWidgetItem*)));

    connect(m_mWin, SIGNAL(unhandledShortcutOverride(QEvent*)),
            this, SLOT(handleEsc(QEvent*)));

    m_toolView->installEventFilter(this);

    m_mWin->guiFactory()->addClient(this);

    m_commonDB = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/katectags/common_db");
}


/******************************************************************/
KateCTagsView::~KateCTagsView()
{
    m_mWin->guiFactory()->removeClient( this );

    delete m_toolView;
}

/******************************************************************/
void KateCTagsView::aboutToShow()
{
    QString currWord = currentWord();
    if (currWord.isEmpty()) {
        return;
    }

    if (Tags::hasTag(currWord)) {
        QString squeezed = KStringHandler::csqueeze(currWord, 30);

        m_gotoDec->setText(i18n("Go to Declaration: %1",squeezed));
        m_gotoDef->setText(i18n("Go to Definition: %1",squeezed));
        m_lookup->setText(i18n("Lookup: %1",squeezed));
    }
}


/******************************************************************/
void KateCTagsView::readSessionConfig (const KConfigGroup& cg)
{
    m_ctagsUi.cmdEdit->setText(cg.readEntry("TagsGenCMD", DEFAULT_CTAGS_CMD));

    int numEntries = cg.readEntry("SessionNumTargets", 0);
    QString nr;
    QString target;
    for (int i=0; i<numEntries; i++) {
        nr = QStringLiteral("%1").arg(i,3);
        target = cg.readEntry(QStringLiteral("SessionTarget_%1").arg(nr), QString());
        if (!listContains(target)) {
            new QListWidgetItem(target, m_ctagsUi.targetList);
        }
    }
    
    QString sessionDB = cg.readEntry("SessionDatabase", QString());
    m_ctagsUi.tagsFile->setText(sessionDB);

}

/******************************************************************/
void KateCTagsView::writeSessionConfig (KConfigGroup& cg)
{
    cg.writeEntry("TagsGenCMD", m_ctagsUi.cmdEdit->text());
    cg.writeEntry("SessionNumTargets", m_ctagsUi.targetList->count());
    
    QString nr;
    for (int i=0; i<m_ctagsUi.targetList->count(); i++) {
        nr = QStringLiteral("%1").arg(i,3);
        cg.writeEntry(QStringLiteral("SessionTarget_%1").arg(nr), m_ctagsUi.targetList->item(i)->text());
    }

    cg.writeEntry("SessionDatabase", m_ctagsUi.tagsFile->text());

    cg.sync();
}


/******************************************************************/
void KateCTagsView::stepBack()
{
    if (m_jumpStack.isEmpty()) {
        return;
    }

    TagJump back;
    back = m_jumpStack.pop();

    m_mWin->openUrl(back.url);
    m_mWin->activeView()->setCursorPosition(back.cursor);
    m_mWin->activeView()->setFocus();

}


/******************************************************************/
void KateCTagsView::lookupTag( )
{
    QString currWord = currentWord();
    if (currWord.isEmpty()) {
        return;
    }

    setNewLookupText(currWord);
    Tags::TagList list = Tags::getExactMatches(m_ctagsUi.tagsFile->text(), currWord);
    if (list.size() == 0) list = Tags::getExactMatches(m_commonDB, currWord);
    displayHits(list);

    // activate the hits tab
    m_ctagsUi.tabWidget->setCurrentIndex(0);
    m_mWin->showToolView(m_toolView);
}

/******************************************************************/
void KateCTagsView::editLookUp()
{
    Tags::TagList list = Tags::getPartialMatches(m_ctagsUi.tagsFile->text(), m_ctagsUi.inputEdit->text());
    if (list.size() == 0) list = Tags::getPartialMatches(m_commonDB, m_ctagsUi.inputEdit->text());
    displayHits(list);
}

/******************************************************************/
void KateCTagsView::gotoDefinition( )
{
    QString currWord = currentWord();
    if (currWord.isEmpty()) {
        return;
    }

    QStringList types;
    types << QStringLiteral("S") << QStringLiteral("d") << QStringLiteral("f") << QStringLiteral("t") << QStringLiteral("v");
    gotoTagForTypes(currWord, types);
}

/******************************************************************/
void KateCTagsView::gotoDeclaration( )
{
    QString currWord = currentWord();
    if (currWord.isEmpty()) {
        return;
    }

    QStringList types;
    types << QStringLiteral("L")
    << QStringLiteral("c")
    << QStringLiteral("e")
    << QStringLiteral("g")
    << QStringLiteral("m")
    << QStringLiteral("n")
    << QStringLiteral("p")
    << QStringLiteral("s")
    << QStringLiteral("u")
    << QStringLiteral("x");
    gotoTagForTypes(currWord, types);
}

/******************************************************************/
void KateCTagsView::gotoTagForTypes(const QString &word, const QStringList &types)
{
    Tags::TagList list = Tags::getMatches(m_ctagsUi.tagsFile->text(), word, false, types);
    if (list.size() == 0) list = Tags::getMatches(m_commonDB, word, false, types);
 
    //qDebug() << "found" << list.count() << word << types;
    setNewLookupText(word);
    
    if ( list.count() < 1) {
        m_ctagsUi.tagTreeWidget->clear();
        new QTreeWidgetItem(m_ctagsUi.tagTreeWidget, QStringList(i18n("No hits found")));
        m_ctagsUi.tabWidget->setCurrentIndex(0);
        m_mWin->showToolView(m_toolView);
        return;
    }

    displayHits(list);

    if (list.count() == 1) {
        Tags::TagEntry tag = list.first();
        jumpToTag(tag.file, tag.pattern, word);
    }
    else {
        Tags::TagEntry tag = list.first();
        jumpToTag(tag.file, tag.pattern, word);
        m_ctagsUi.tabWidget->setCurrentIndex(0);
        m_mWin->showToolView(m_toolView);
    }
}

/******************************************************************/
void KateCTagsView::setNewLookupText(const QString &newString)
{
    m_ctagsUi.inputEdit->blockSignals( true );
    m_ctagsUi.inputEdit->setText(newString);
    m_ctagsUi.inputEdit->blockSignals( false );
}

/******************************************************************/
void KateCTagsView::displayHits(const Tags::TagList &list)
{
    m_ctagsUi.tagTreeWidget->clear();
    if (list.isEmpty()) {
        new QTreeWidgetItem(m_ctagsUi.tagTreeWidget, QStringList(i18n("No hits found")));
        return;
    }
    m_ctagsUi.tagTreeWidget->setSortingEnabled(false);

    for (int i=0; i<list.size(); i++) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_ctagsUi.tagTreeWidget);
        item->setText(0, list[i].tag);
        item->setText(1, list[i].type);
        item->setText(2, list[i].file);
        item->setData(0, Qt::UserRole, list[i].pattern);

        QString pattern = list[i].pattern;
        pattern.replace( QStringLiteral("\\/"), QStringLiteral("/"));
        pattern = pattern.mid(2, pattern.length() - 4);
        pattern = pattern.trimmed();

        item->setData(0, Qt::ToolTipRole, pattern);
        item->setData(1, Qt::ToolTipRole, pattern);
        item->setData(2, Qt::ToolTipRole, pattern);
    }
    m_ctagsUi.tagTreeWidget->setSortingEnabled(true);
}

/******************************************************************/
void KateCTagsView::tagHitClicked(QTreeWidgetItem *item)
{
    // get stuff
    const QString file = item->data(2, Qt::DisplayRole).toString();
    const QString pattern = item->data(0, Qt::UserRole).toString();
    const QString word = item->data(0, Qt::DisplayRole).toString();

    jumpToTag(file, pattern, word);
}

/******************************************************************/
QString KateCTagsView::currentWord( )
{
    KTextEditor::View *kv = m_mWin->activeView();
    if (!kv) {
        qDebug() << "no KTextEditor::View" << endl;
        return QString();
    }

    if (kv->selection()) {
        return kv->selectionText();
    }

    if (!kv->cursorPosition().isValid()) {
        qDebug() << "cursor not valid!" << endl;
        return QString();
    }

    int line = kv->cursorPosition().line();
    int col = kv->cursorPosition().column();
    bool includeColon = m_ctagsUi.cmdEdit->text().contains(QLatin1String("--extra=+q"));

    QString linestr = kv->document()->line(line);

    int startPos = qMax(qMin(col, linestr.length()-1), 0);
    int endPos = startPos;
    while (startPos >= 0 && (linestr[startPos].isLetterOrNumber() ||
        (linestr[startPos] == QLatin1Char(':') && includeColon) ||
        linestr[startPos] == QLatin1Char('_') ||
        linestr[startPos] == QLatin1Char('~')))
    {
        startPos--;
    }
    while (endPos < (int)linestr.length() && (linestr[endPos].isLetterOrNumber() ||
        (linestr[endPos] == QLatin1Char(':') && includeColon) ||
        linestr[endPos] == QLatin1Char('_'))) {
        endPos++;
    }
    if  (startPos == endPos) {
        qDebug() << "no word found!" << endl;
        return QString();
    }

    linestr = linestr.mid(startPos+1, endPos-startPos-1);

    while (linestr.endsWith(QLatin1Char(':'))) {
        linestr.remove(linestr.size()-1, 1);
    }

    while (linestr.startsWith(QLatin1Char(':'))) {
        linestr.remove(0, 1);
    }

    //qDebug() << linestr;
    return linestr;
}

/******************************************************************/
void KateCTagsView::jumpToTag(const QString &file, const QString &pattern, const QString &word)
{
    if (pattern.isEmpty()) return;

    // generate a regexp from the pattern
    // ctags interestingly escapes "/", but apparently nothing else. lets revert that
    QString unescaped = pattern;
    unescaped.replace( QStringLiteral("\\/"), QStringLiteral("/") );

    // most of the time, the ctags pattern has the form /^foo$/
    // but this isn't true for some macro definitions
    // where the form is only /^foo/
    // I have no idea if this is a ctags bug or not, but we have to deal with it

    QString reduced;
    QString escaped;
    QString re_string;

    if (unescaped.endsWith(QStringLiteral("$/"))) {
        reduced = unescaped.mid(2, unescaped.length() - 4);
        escaped = QRegExp::escape(reduced);
        re_string = QStringLiteral("^%1$").arg(escaped);
    }
    else {
        reduced = unescaped.mid( 2, unescaped.length() -3 );
        escaped = QRegExp::escape(reduced);
        re_string = QStringLiteral("^%1").arg(escaped);
    }

    QRegExp re(re_string);

    // save current location
    TagJump from;
    from.url    = m_mWin->activeView()->document()->url();
    from.cursor = m_mWin->activeView()->cursorPosition();
    m_jumpStack.push(from);

    // open/activate the new file
    QFileInfo fInfo(file);
    //qDebug() << pattern << file << fInfo.absoluteFilePath();
    m_mWin->openUrl(QUrl::fromLocalFile(fInfo.absoluteFilePath()));

    // any view active?
    if (!m_mWin->activeView()) {
        return;
    }

    // look for the line
    QString linestr;
    int line;
    for (line =0; line < m_mWin->activeView()->document()->lines(); line++) {
        linestr = m_mWin->activeView()->document()->line(line);
        if (linestr.indexOf(re) > -1) break;
    }

    // activate the line
    if (line != m_mWin->activeView()->document()->lines()) {
        // line found now look for the column
        int column = linestr.indexOf(word) + (word.length()/2);
        m_mWin->activeView()->setCursorPosition(KTextEditor::Cursor(line, column));
    }
    m_mWin->activeView()->setFocus();

}


/******************************************************************/
void KateCTagsView::startEditTmr()
{
    if (m_ctagsUi.inputEdit->text().size() > 3) {
        m_editTimer.start(500);
    }
}


/******************************************************************/
void KateCTagsView::updateSessionDB()
{
    if (m_proc.state() != QProcess::NotRunning) {
        return;
    }

    QString targets;
    QString target;
    for (int i=0; i<m_ctagsUi.targetList->count(); i++) {
      target = m_ctagsUi.targetList->item(i)->text();
      if (target.endsWith(QLatin1Char('/')) || target.endsWith(QLatin1Char('\\'))) {
        target = target.left(target.size() - 1);
      }
      targets += target + QLatin1Char(' ');
    }

    QString pluginFolder = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("/katectags");
    QDir().mkpath(pluginFolder);

    if (m_ctagsUi.tagsFile->text().isEmpty()) {
        // FIXME we need a way to get the session name
        pluginFolder + QLatin1String("/session_db_");
        pluginFolder += QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
        m_ctagsUi.tagsFile->setText(pluginFolder);
    }

    if (targets.isEmpty()) {
        QFile::remove(m_ctagsUi.tagsFile->text());
        return;
    }


    QString command = QStringLiteral("%1 -f %2 %3").arg(m_ctagsUi.cmdEdit->text()).arg(m_ctagsUi.tagsFile->text()).arg(targets);

    m_proc.start(command);

    if(!m_proc.waitForStarted(500)) {
        KMessageBox::error(0, i18n("Failed to run \"%1\". exitStatus = %2", command, m_proc.exitStatus()));
        return;
    }
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    m_ctagsUi.updateButton->setDisabled(true);
    m_ctagsUi.updateButton2->setDisabled(true);
}


/******************************************************************/
void KateCTagsView::updateDone(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        KMessageBox::error(m_toolView, i18n("The CTags executable crashed."));
    } else if (exitCode != 0) {
        KMessageBox::error(m_toolView, i18n("The CTags program exited with code %1", exitCode));
    }

    m_ctagsUi.updateButton->setDisabled(false);
    m_ctagsUi.updateButton2->setDisabled(false);
    QApplication::restoreOverrideCursor();
}

/******************************************************************/
void KateCTagsView::addTagTarget()
{
    QFileDialog dialog;
    dialog.setDirectory(m_mWin->activeView()->document()->url().path());
    dialog.setFileMode(QFileDialog::Directory);

    // i18n("CTags Database Location"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList urls = dialog.selectedFiles();

    for (int i=0; i<urls.size(); i++) {
        if (!listContains(urls[i])) {
            new QListWidgetItem(urls[i], m_ctagsUi.targetList);
        }
    }
}

/******************************************************************/
void KateCTagsView::delTagTarget()
{
    delete m_ctagsUi.targetList->currentItem ();
}

/******************************************************************/
bool KateCTagsView::listContains(const QString &target)
{
    for (int i=0; i<m_ctagsUi.targetList->count(); i++) {
        if (m_ctagsUi.targetList->item(i)->text() == target) {
            return true;
        }
    }
    return false;
}

/******************************************************************/
bool KateCTagsView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if ((obj == m_toolView) && (ke->key() == Qt::Key_Escape)) {
            m_mWin->hideToolView(m_toolView);
            event->accept();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

/******************************************************************/
void KateCTagsView::resetCMD()
{
    m_ctagsUi.cmdEdit->setText(DEFAULT_CTAGS_CMD);
}

/******************************************************************/
void KateCTagsView::handleEsc(QEvent *e)
{
    if (!m_mWin) return;

    QKeyEvent *k = static_cast<QKeyEvent *>(e);
    if (k->key() == Qt::Key_Escape && k->modifiers() == Qt::NoModifier) {
        if (m_toolView->isVisible()) {
            m_mWin->hideToolView(m_toolView);
        }
    }
}

