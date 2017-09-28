#include <QtDebug>
#include <QtWidgets>
#include <QUrl>
#include "vfilelist.h"
#include "vconfigmanager.h"
#include "dialog/vnewfiledialog.h"
#include "dialog/vfileinfodialog.h"
#include "vnote.h"
#include "veditarea.h"
#include "utils/vutils.h"
#include "vnotefile.h"
#include "vconfigmanager.h"
#include "vmdedit.h"
#include "vmdtab.h"
#include "dialog/vconfirmdeletiondialog.h"
#include "dialog/vsortdialog.h"
#include "vmainwindow.h"

extern VConfigManager *g_config;
extern VNote *g_vnote;
extern VMainWindow *g_mainWin;

const QString VFileList::c_infoShortcutSequence = "F2";
const QString VFileList::c_copyShortcutSequence = "Ctrl+C";
const QString VFileList::c_cutShortcutSequence = "Ctrl+X";
const QString VFileList::c_pasteShortcutSequence = "Ctrl+V";

VFileList::VFileList(QWidget *parent)
    : QWidget(parent), VNavigationMode()
{
    setupUI();
    initShortcuts();
    initActions();
}

void VFileList::setupUI()
{
    fileList = new QListWidget(this);
    fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fileList->setObjectName("FileList");

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fileList);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    connect(fileList, &QListWidget::customContextMenuRequested,
            this, &VFileList::contextMenuRequested);
    connect(fileList, &QListWidget::itemClicked,
            this, &VFileList::handleItemClicked);

    setLayout(mainLayout);
}

void VFileList::initShortcuts()
{
    QShortcut *infoShortcut = new QShortcut(QKeySequence(c_infoShortcutSequence), this);
    infoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(infoShortcut, &QShortcut::activated,
            this, [this](){
                fileInfo();
            });

    QShortcut *copyShortcut = new QShortcut(QKeySequence(c_copyShortcutSequence), this);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut, &QShortcut::activated,
            this, [this](){
                copySelectedFiles();
            });

    QShortcut *cutShortcut = new QShortcut(QKeySequence(c_cutShortcutSequence), this);
    cutShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cutShortcut, &QShortcut::activated,
            this, [this](){
                cutSelectedFiles();
            });

    QShortcut *pasteShortcut = new QShortcut(QKeySequence(c_pasteShortcutSequence), this);
    pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteShortcut, &QShortcut::activated,
            this, [this](){
                pasteFilesFromClipboard();
            });
}

void VFileList::initActions()
{
    newFileAct = new QAction(QIcon(":/resources/icons/create_note.svg"),
                             tr("&New Note"), this);
    QString shortcutStr = VUtils::getShortcutText(g_config->getShortcutKeySequence("NewNote"));
    if (!shortcutStr.isEmpty()) {
        newFileAct->setText(tr("&New Note\t%1").arg(shortcutStr));
    }

    newFileAct->setToolTip(tr("Create a note in current folder"));
    connect(newFileAct, SIGNAL(triggered(bool)),
            this, SLOT(newFile()));

    m_openInReadAct = new QAction(QIcon(":/resources/icons/reading.svg"),
                                  tr("&Open In Read Mode"), this);
    m_openInReadAct->setToolTip(tr("Open current note in read mode"));
    connect(m_openInReadAct, &QAction::triggered,
            this, [this]() {
                QListWidgetItem *item = fileList->currentItem();
                if (item) {
                    emit fileClicked(getVFile(item), OpenFileMode::Read);
                }
            });

    m_openInEditAct = new QAction(QIcon(":/resources/icons/editing.svg"),
                                  tr("Open In &Edit Mode"), this);
    m_openInEditAct->setToolTip(tr("Open current note in edit mode"));
    connect(m_openInEditAct, &QAction::triggered,
            this, [this]() {
                QListWidgetItem *item = fileList->currentItem();
                if (item) {
                    emit fileClicked(getVFile(item), OpenFileMode::Edit);
                }
            });

    m_openExternalAct = new QAction(tr("Open Via External Program"), this);
    m_openExternalAct->setToolTip(tr("Open current note via external program"));
    connect(m_openExternalAct, &QAction::triggered,
            this, [this]() {
                QListWidgetItem *item = fileList->currentItem();
                if (item) {
                    VNoteFile *file = getVFile(item);
                    if (file
                        && (!editArea->isFileOpened(file) || editArea->closeFile(file, false))) {
                        QUrl url = QUrl::fromLocalFile(file->fetchPath());
                        QDesktopServices::openUrl(url);
                    }
                }
            });

    deleteFileAct = new QAction(QIcon(":/resources/icons/delete_note.svg"),
                                tr("&Delete"), this);
    deleteFileAct->setToolTip(tr("Delete selected note"));
    connect(deleteFileAct, SIGNAL(triggered(bool)),
            this, SLOT(deleteSelectedFiles()));

    fileInfoAct = new QAction(QIcon(":/resources/icons/note_info.svg"),
                              tr("&Info\t%1").arg(VUtils::getShortcutText(c_infoShortcutSequence)), this);
    fileInfoAct->setToolTip(tr("View and edit current note's information"));
    connect(fileInfoAct, SIGNAL(triggered(bool)),
            this, SLOT(fileInfo()));

    copyAct = new QAction(QIcon(":/resources/icons/copy.svg"),
                          tr("&Copy\t%1").arg(VUtils::getShortcutText(c_copyShortcutSequence)), this);
    copyAct->setToolTip(tr("Copy selected notes"));
    connect(copyAct, &QAction::triggered,
            this, &VFileList::copySelectedFiles);

    cutAct = new QAction(QIcon(":/resources/icons/cut.svg"),
                         tr("C&ut\t%1").arg(VUtils::getShortcutText(c_cutShortcutSequence)), this);
    cutAct->setToolTip(tr("Cut selected notes"));
    connect(cutAct, &QAction::triggered,
            this, &VFileList::cutSelectedFiles);

    pasteAct = new QAction(QIcon(":/resources/icons/paste.svg"),
                           tr("&Paste\t%1").arg(VUtils::getShortcutText(c_pasteShortcutSequence)), this);
    pasteAct->setToolTip(tr("Paste notes in current folder"));
    connect(pasteAct, &QAction::triggered,
            this, &VFileList::pasteFilesFromClipboard);

    m_openLocationAct = new QAction(tr("&Open Note Location"), this);
    m_openLocationAct->setToolTip(tr("Open the folder containing this note in operating system"));
    connect(m_openLocationAct, &QAction::triggered,
            this, &VFileList::openFileLocation);

    m_sortAct = new QAction(QIcon(":/resources/icons/sort.svg"),
                            tr("&Sort"),
                            this);
    m_sortAct->setToolTip(tr("Sort notes in this folder manually"));
    connect(m_sortAct, &QAction::triggered,
            this, &VFileList::sortItems);
}

void VFileList::setDirectory(VDirectory *p_directory)
{
    // QPointer will be set to NULL automatically once the directory was deleted.
    // If the last directory is deleted, m_directory and p_directory will both
    // be NULL.
    if (m_directory == p_directory) {
        if (!m_directory) {
            fileList->clear();
        }

        return;
    }

    m_directory = p_directory;
    if (!m_directory) {
        fileList->clear();
        return;
    }

    updateFileList();
}

void VFileList::updateFileList()
{
    fileList->clear();
    if (!m_directory->open()) {
        return;
    }

    const QVector<VNoteFile *> &files = m_directory->getFiles();
    for (int i = 0; i < files.size(); ++i) {
        VNoteFile *file = files[i];
        insertFileListItem(file);
    }
}

void VFileList::fileInfo()
{
    QList<QListWidgetItem *> items = fileList->selectedItems();
    if (items.size() == 1) {
        fileInfo(getVFile(items[0]));
    }
}

void VFileList::openFileLocation() const
{
    QList<QListWidgetItem *> items = fileList->selectedItems();
    if (items.size() == 1) {
        QUrl url = QUrl::fromLocalFile(getVFile(items[0])->fetchBasePath());
        QDesktopServices::openUrl(url);
    }
}

void VFileList::fileInfo(VNoteFile *p_file)
{
    if (!p_file) {
        return;
    }

    VDirectory *dir = p_file->getDirectory();
    QString curName = p_file->getName();
    VFileInfoDialog dialog(tr("Note Information"), "", dir, p_file, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getNameInput();
        if (name == curName) {
            return;
        }

        if (!p_file->rename(name)) {
            VUtils::showMessage(QMessageBox::Warning, tr("Warning"),
                                tr("Fail to rename note <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle).arg(curName), "",
                                QMessageBox::Ok, QMessageBox::Ok, this);
            return;
        }

        QListWidgetItem *item = findItem(p_file);
        if (item) {
            fillItem(item, p_file);
        }

        emit fileUpdated(p_file);
    }
}

void VFileList::fillItem(QListWidgetItem *p_item, const VNoteFile *p_file)
{
    unsigned long long ptr = (long long)p_file;
    p_item->setData(Qt::UserRole, ptr);
    p_item->setToolTip(p_file->getName());
    p_item->setText(p_file->getName());

    V_ASSERT(sizeof(p_file) <= sizeof(ptr));
}

QListWidgetItem* VFileList::insertFileListItem(VNoteFile *file, bool atFront)
{
    V_ASSERT(file);
    QListWidgetItem *item = new QListWidgetItem();
    fillItem(item, file);

    if (atFront) {
        fileList->insertItem(0, item);
    } else {
        fileList->addItem(item);
    }

    // Qt seems not to update the QListWidget correctly. Manually force it to repaint.
    fileList->update();
    return item;
}

void VFileList::removeFileListItem(VNoteFile *p_file)
{
    if (!p_file) {
        return;
    }

    QListWidgetItem *item = findItem(p_file);
    if (!item) {
        return;
    }

    int row = fileList->row(item);
    Q_ASSERT(row >= 0);

    fileList->takeItem(row);
    delete item;

    // Qt seems not to update the QListWidget correctly. Manually force it to repaint.
    fileList->update();
}

void VFileList::newFile()
{
    if (!m_directory) {
        return;
    }

    QList<QString> suffixes = g_config->getDocSuffixes()[(int)DocType::Markdown];
    QString defaultSuf;
    QString suffixStr;
    for (auto const & suf : suffixes) {
        suffixStr += (suffixStr.isEmpty() ? suf : "/" + suf);
        if (defaultSuf.isEmpty() || suf == "md") {
            defaultSuf = suf;
        }
    }

    QString info = tr("Create a note in <span style=\"%1\">%2</span>.")
                     .arg(g_config->c_dataTextStyle).arg(m_directory->getName());
    info = info + "<br>" + tr("Note with name ending with \"%1\" will be treated as Markdown type.")
                             .arg(suffixStr);
    QString defaultName = QString("new_note.%1").arg(defaultSuf);
    defaultName = VUtils::getFileNameWithSequence(m_directory->fetchPath(), defaultName);
    VNewFileDialog dialog(tr("Create Note"), info, defaultName, m_directory, this);
    if (dialog.exec() == QDialog::Accepted) {
        VNoteFile *file = m_directory->createFile(dialog.getNameInput());
        if (!file) {
            VUtils::showMessage(QMessageBox::Warning, tr("Warning"),
                                tr("Fail to create note <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle).arg(dialog.getNameInput()), "",
                                QMessageBox::Ok, QMessageBox::Ok, this);
            return;
        }

        // Write title if needed.
        bool contentInserted = false;
        if (dialog.getInsertTitleInput() && file->getDocType() == DocType::Markdown) {
            if (!file->open()) {
                qWarning() << "fail to open newly-created note" << file->getName();
            } else {
                Q_ASSERT(file->getContent().isEmpty());
                QString content = QString("# %1\n").arg(QFileInfo(file->getName()).baseName());
                file->setContent(content);
                if (!file->save()) {
                    qWarning() << "fail to write to newly-created note" << file->getName();
                } else {
                    contentInserted = true;
                }

                file->close();
            }
        }

        QVector<QListWidgetItem *> items = updateFileListAdded();
        Q_ASSERT(items.size() == 1);
        fileList->setCurrentItem(items[0], QItemSelectionModel::ClearAndSelect);
        // Qt seems not to update the QListWidget correctly. Manually force it to repaint.
        fileList->update();

        // Open it in edit mode
        emit fileCreated(file, OpenFileMode::Edit);

        // Move cursor down if content has been inserted.
        if (contentInserted) {
            const VMdTab *tab = dynamic_cast<VMdTab *>(editArea->currentEditTab());
            if (tab) {
                VMdEdit *edit = dynamic_cast<VMdEdit *>(tab->getEditor());
                if (edit && edit->getFile() == file) {
                    QTextCursor cursor = edit->textCursor();
                    cursor.movePosition(QTextCursor::End);
                    edit->setTextCursor(cursor);
                }
            }
        }
    }
}

QVector<QListWidgetItem *> VFileList::updateFileListAdded()
{
    QVector<QListWidgetItem *> ret;
    const QVector<VNoteFile *> &files = m_directory->getFiles();
    for (int i = 0; i < files.size(); ++i) {
        VNoteFile *file = files[i];
        if (i >= fileList->count()) {
            QListWidgetItem *item = insertFileListItem(file, false);
            ret.append(item);
        } else {
            VNoteFile *itemFile = getVFile(fileList->item(i));
            if (itemFile != file) {
                QListWidgetItem *item = insertFileListItem(file, false);
                ret.append(item);
            }
        }
    }

    return ret;
}

void VFileList::deleteSelectedFiles()
{
    QList<QListWidgetItem *> items = fileList->selectedItems();
    Q_ASSERT(!items.isEmpty());

    QVector<VNoteFile *> files;
    for (auto const & item : items) {
        files.push_back(getVFile(item));
    }

    deleteFiles(files);
}

// @p_file may or may not be listed in VFileList
void VFileList::deleteFile(VNoteFile *p_file)
{
    if (!p_file) {
        return;
    }

    QVector<VNoteFile *> files(1, p_file);
    deleteFiles(files);
}

void VFileList::deleteFiles(const QVector<VNoteFile *> &p_files)
{
    if (p_files.isEmpty()) {
        return;
    }

    QVector<ConfirmItemInfo> items;
    for (auto const & file : p_files) {
        items.push_back(ConfirmItemInfo(file->getName(),
                                        file->fetchPath(),
                                        file->fetchPath(),
                                        (void *)file));
    }

    QString text = tr("Are you sure to delete these notes?");

    QString info = tr("<span style=\"%1\">WARNING</span>: "
                      "VNote will delete notes as well as all "
                      "their images and attachments managed by VNote. "
                      "You could find deleted files in the recycle "
                      "bin of these notes.<br>"
                      "Click \"Cancel\" to leave them untouched.<br>"
                      "The operation is IRREVERSIBLE!")
                     .arg(g_config->c_warningTextStyle);

    VConfirmDeletionDialog dialog(tr("Confirm Deleting Notes"),
                                  text,
                                  info,
                                  items,
                                  false,
                                  false,
                                  false,
                                  this);
    if (dialog.exec()) {
        items = dialog.getConfirmedItems();
        QVector<VNoteFile *> files;
        for (auto const & item : items) {
            files.push_back((VNoteFile *)item.m_data);
        }

        int nrDeleted = 0;
        for (auto file : files) {
            editArea->closeFile(file, true);

            // Remove the item before deleting it totally, or file will be invalid.
            removeFileListItem(file);

            QString errMsg;
            QString fileName = file->getName();
            QString filePath = file->fetchPath();
            if (!VNoteFile::deleteFile(file, &errMsg)) {
                VUtils::showMessage(QMessageBox::Warning,
                                    tr("Warning"),
                                    tr("Fail to delete note <span style=\"%1\">%2</span>.<br>"
                                       "Please check <span style=\"%1\">%3</span> and manually delete it.")
                                      .arg(g_config->c_dataTextStyle)
                                      .arg(fileName)
                                      .arg(filePath),
                                    errMsg,
                                    QMessageBox::Ok,
                                    QMessageBox::Ok,
                                    this);
            } else {
                Q_ASSERT(errMsg.isEmpty());
                ++nrDeleted;
            }
        }

        if (nrDeleted > 0) {
            g_mainWin->showStatusMessage(tr("%1 %2 deleted")
                                           .arg(nrDeleted)
                                           .arg(nrDeleted > 1 ? tr("notes") : tr("note")));
        }
    }
}

void VFileList::contextMenuRequested(QPoint pos)
{
    QListWidgetItem *item = fileList->itemAt(pos);
    QMenu menu(this);
    menu.setToolTipsVisible(true);

    if (!m_directory) {
        return;
    }

    if (item && fileList->selectedItems().size() == 1) {
        VNoteFile *file = getVFile(item);
        if (file) {
            if (file->getDocType() == DocType::Markdown) {
                menu.addAction(m_openInReadAct);
                menu.addAction(m_openInEditAct);
            }

            menu.addAction(m_openExternalAct);
            menu.addSeparator();
        }
    }

    menu.addAction(newFileAct);

    if (fileList->count() > 1) {
        menu.addAction(m_sortAct);
    }

    if (item) {
        menu.addSeparator();
        menu.addAction(deleteFileAct);
        menu.addAction(copyAct);
        menu.addAction(cutAct);
    }

    if (pasteAvailable()) {
        if (!item) {
            menu.addSeparator();
        }

        menu.addAction(pasteAct);
    }

    if (item) {
        menu.addSeparator();
        menu.addAction(m_openLocationAct);

        if (fileList->selectedItems().size() == 1) {
            menu.addAction(fileInfoAct);
        }
    }

    menu.exec(fileList->mapToGlobal(pos));
}

QListWidgetItem* VFileList::findItem(const VNoteFile *p_file)
{
    if (!p_file || p_file->getDirectory() != m_directory) {
        return NULL;
    }

    int nrChild = fileList->count();
    for (int i = 0; i < nrChild; ++i) {
        QListWidgetItem *item = fileList->item(i);
        if (p_file == getVFile(item)) {
            return item;
        }
    }

    return NULL;
}

void VFileList::handleItemClicked(QListWidgetItem *currentItem)
{
    Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    if (modifiers != Qt::NoModifier) {
        return;
    }

    if (!currentItem) {
        emit fileClicked(NULL);
        return;
    }

    // Qt seems not to update the QListWidget correctly. Manually force it to repaint.
    fileList->update();
    emit fileClicked(getVFile(currentItem), g_config->getNoteOpenMode());
}

bool VFileList::importFiles(const QStringList &p_files, QString *p_errMsg)
{
    if (p_files.isEmpty()) {
        return false;
    }

    bool ret = true;
    Q_ASSERT(m_directory && m_directory->isOpened());
    QString dirPath = m_directory->fetchPath();
    QDir dir(dirPath);

    int nrImported = 0;
    for (int i = 0; i < p_files.size(); ++i) {
        const QString &file = p_files[i];

        QFileInfo fi(file);
        if (!fi.exists() || !fi.isFile()) {
            VUtils::addErrMsg(p_errMsg, tr("Skip importing non-exist file %1.")
                                          .arg(file));
            ret = false;
            continue;
        }

        QString name = VUtils::fileNameFromPath(file);
        Q_ASSERT(!name.isEmpty());
        name = VUtils::getFileNameWithSequence(dirPath, name);
        QString targetFilePath = dir.filePath(name);
        bool ret = VUtils::copyFile(file, targetFilePath, false);
        if (!ret) {
            VUtils::addErrMsg(p_errMsg, tr("Fail to copy file %1 as %1.")
                                          .arg(file)
                                          .arg(targetFilePath));
            ret = false;
            continue;
        }

        VNoteFile *destFile = m_directory->addFile(name, -1);
        if (destFile) {
            ++nrImported;
            qDebug() << "imported" << file << "as" << targetFilePath;
        } else {
            VUtils::addErrMsg(p_errMsg, tr("Fail to add the note %1 to target folder's configuration.")
                                          .arg(file));
            ret = false;
            continue;
        }
    }

    qDebug() << "imported" << nrImported << "files";

    updateFileList();

    return ret;
}

void VFileList::copySelectedFiles(bool p_isCut)
{
    QList<QListWidgetItem *> items = fileList->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    QJsonArray files;
    for (int i = 0; i < items.size(); ++i) {
        VNoteFile *file = getVFile(items[i]);
        files.append(file->fetchPath());
    }

    QJsonObject clip;
    clip[ClipboardConfig::c_magic] = getNewMagic();
    clip[ClipboardConfig::c_type] = (int)ClipboardOpType::CopyFile;
    clip[ClipboardConfig::c_isCut] = p_isCut;
    clip[ClipboardConfig::c_files] = files;

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QJsonDocument(clip).toJson(QJsonDocument::Compact));

    qDebug() << "copied files info" << clipboard->text();

    int cnt = files.size();
    g_mainWin->showStatusMessage(tr("%1 %2 %3")
                                   .arg(cnt)
                                   .arg(cnt > 1 ? tr("notes") : tr("note"))
                                   .arg(p_isCut ? tr("cut") : tr("copied")));
}

void VFileList::cutSelectedFiles()
{
    copySelectedFiles(true);
}

void VFileList::pasteFilesFromClipboard()
{
    if (!pasteAvailable()) {
        return;
    }

    QJsonObject obj = VUtils::clipboardToJson();
    QJsonArray files = obj[ClipboardConfig::c_files].toArray();
    bool isCut = obj[ClipboardConfig::c_isCut].toBool();
    QVector<QString> filesToPaste(files.size());
    for (int i = 0; i < files.size(); ++i) {
        filesToPaste[i] = files[i].toString();
    }

    pasteFiles(m_directory, filesToPaste, isCut);
}

void VFileList::pasteFiles(VDirectory *p_destDir,
                           const QVector<QString> &p_files,
                           bool p_isCut)
{
    QClipboard *clipboard = QApplication::clipboard();
    if (!p_destDir || p_files.isEmpty()) {
        clipboard->clear();
        return;
    }

    int nrPasted = 0;
    for (int i = 0; i < p_files.size(); ++i) {
        VNoteFile *file = g_vnote->getInternalFile(p_files[i]);
        if (!file) {
            qWarning() << "Copied file is not an internal note" << p_files[i];
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to copy note <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(p_files[i]),
                                tr("VNote could not find this note in any notebook."),
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);

            continue;
        }

        QString fileName = file->getName();
        if (file->getDirectory() == p_destDir) {
            if (p_isCut) {
                qDebug() << "skip one note to cut and paste in the same folder" << fileName;
                continue;
            }

            // Copy and paste in the same folder.
            // We do not allow this if the note contains local images.
            if (file->getDocType() == DocType::Markdown) {
                QVector<ImageLink> images = VUtils::fetchImagesFromMarkdownFile(file,
                                                                                ImageLink::LocalRelativeInternal);
                if (!images.isEmpty()) {
                    qDebug() << "skip one note with internal images to copy and paste in the same folder"
                             << fileName;
                    VUtils::showMessage(QMessageBox::Warning,
                                        tr("Warning"),
                                        tr("Fail to copy note <span style=\"%1\">%2</span>.")
                                          .arg(g_config->c_dataTextStyle)
                                          .arg(p_files[i]),
                                        tr("VNote does not allow copy and paste notes with internal images "
                                           "in the same folder."),
                                        QMessageBox::Ok,
                                        QMessageBox::Ok,
                                        this);
                    continue;
                }
            }

            // Rename it to xxx_copy.md.
            fileName = VUtils::generateCopiedFileName(file->fetchBasePath(), fileName);
        } else {
            // Rename it to xxx_copy.md if needed.
            fileName = VUtils::generateCopiedFileName(p_destDir->fetchPath(), fileName);
        }

        QString msg;
        VNoteFile *destFile = NULL;
        bool ret = VNoteFile::copyFile(p_destDir,
                                       fileName,
                                       file,
                                       p_isCut,
                                       &destFile,
                                       &msg);
        if (!ret) {
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to copy note <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(p_files[i]),
                                msg,
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);
        }

        if (destFile) {
            ++nrPasted;
            emit fileUpdated(destFile);
        }
    }

    qDebug() << "copy" << nrPasted << "files";
    if (nrPasted > 0) {
        g_mainWin->showStatusMessage(tr("%1 %2 pasted")
                                       .arg(nrPasted)
                                       .arg(nrPasted > 1 ? tr("notes") : tr("note")));
    }

    updateFileList();
    clipboard->clear();
    getNewMagic();
}

void VFileList::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    int modifiers = event->modifiers();
    switch (key) {
    case Qt::Key_Return:
    {
        QListWidgetItem *item = fileList->currentItem();
        if (item) {
            handleItemClicked(item);
        }

        break;
    }


    case Qt::Key_J:
    {
        if (modifiers == Qt::ControlModifier) {
            event->accept();
            QKeyEvent *downEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down,
                                                 Qt::NoModifier);
            QCoreApplication::postEvent(fileList, downEvent);
            return;
        }
        break;
    }

    case Qt::Key_K:
    {
        if (modifiers == Qt::ControlModifier) {
            event->accept();
            QKeyEvent *upEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up,
                                               Qt::NoModifier);
            QCoreApplication::postEvent(fileList, upEvent);
            return;
        }
        break;
    }

    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void VFileList::focusInEvent(QFocusEvent * /* p_event */)
{
    fileList->setFocus();
}

bool VFileList::locateFile(const VNoteFile *p_file)
{
    if (p_file) {
        if (p_file->getDirectory() != m_directory) {
            return false;
        }

        QListWidgetItem *item = findItem(p_file);
        if (item) {
            fileList->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
            return true;
        }
    }

    return false;
}

void VFileList::registerNavigation(QChar p_majorKey)
{
    m_majorKey = p_majorKey;
    V_ASSERT(m_keyMap.empty());
    V_ASSERT(m_naviLabels.empty());
}

void VFileList::showNavigation()
{
    // Clean up.
    m_keyMap.clear();
    for (auto label : m_naviLabels) {
        delete label;
    }
    m_naviLabels.clear();

    if (!isVisible()) {
        return;
    }

    // Generate labels for visible items.
    auto items = getVisibleItems();
    int itemWidth = rect().width();
    for (int i = 0; i < 26 && i < items.size(); ++i) {
        QChar key('a' + i);
        m_keyMap[key] = items[i];

        QString str = QString(m_majorKey) + key;
        QLabel *label = new QLabel(str, this);
        label->setStyleSheet(g_vnote->getNavigationLabelStyle(str));
        label->show();
        QRect rect = fileList->visualItemRect(items[i]);
        // Display the label at the end to show the file name.
        label->move(rect.x() + itemWidth - label->width(), rect.y());
        m_naviLabels.append(label);
    }
}

void VFileList::hideNavigation()
{
    m_keyMap.clear();
    for (auto label : m_naviLabels) {
        delete label;
    }
    m_naviLabels.clear();
}

bool VFileList::handleKeyNavigation(int p_key, bool &p_succeed)
{
    static bool secondKey = false;
    bool ret = false;
    p_succeed = false;
    QChar keyChar = VUtils::keyToChar(p_key);
    if (secondKey && !keyChar.isNull()) {
        secondKey = false;
        p_succeed = true;
        ret = true;
        auto it = m_keyMap.find(keyChar);
        if (it != m_keyMap.end()) {
            fileList->setCurrentItem(it.value(), QItemSelectionModel::ClearAndSelect);
            fileList->setFocus();
        }
    } else if (keyChar == m_majorKey) {
        // Major key pressed.
        // Need second key if m_keyMap is not empty.
        if (m_keyMap.isEmpty()) {
            p_succeed = true;
        } else {
            secondKey = true;
        }
        ret = true;
    }
    return ret;
}

QList<QListWidgetItem *> VFileList::getVisibleItems() const
{
    QList<QListWidgetItem *> items;
    for (int i = 0; i < fileList->count(); ++i) {
        QListWidgetItem *item = fileList->item(i);
        if (!item->isHidden()) {
            items.append(item);
        }
    }
    return items;
}

int VFileList::getNewMagic()
{
    m_magicForClipboard = (int)QDateTime::currentDateTime().toTime_t();
    m_magicForClipboard |= qrand();

    return m_magicForClipboard;
}

bool VFileList::checkMagic(int p_magic) const
{
    return m_magicForClipboard == p_magic;
}

bool VFileList::pasteAvailable() const
{
    QJsonObject obj = VUtils::clipboardToJson();
    if (obj.isEmpty()) {
        return false;
    }

    if (!obj.contains(ClipboardConfig::c_type)) {
        return false;
    }

    ClipboardOpType type = (ClipboardOpType)obj[ClipboardConfig::c_type].toInt();
    if (type != ClipboardOpType::CopyFile) {
        return false;
    }

    if (!obj.contains(ClipboardConfig::c_magic)
        || !obj.contains(ClipboardConfig::c_isCut)
        || !obj.contains(ClipboardConfig::c_files)) {
        return false;
    }

    int magic = obj[ClipboardConfig::c_magic].toInt();
    if (!checkMagic(magic)) {
        return false;
    }

    QJsonArray files = obj[ClipboardConfig::c_files].toArray();
    return !files.isEmpty();
}

void VFileList::sortItems()
{
    const QVector<VNoteFile *> &files = m_directory->getFiles();
    if (files.size() < 2) {
        return;
    }

    VSortDialog dialog(tr("Sort Notes"),
                       tr("Sort notes in folder <span style=\"%1\">%2</span> "
                          "in the configuration file.")
                         .arg(g_config->c_dataTextStyle)
                         .arg(m_directory->getName()),
                       this);
    QTreeWidget *tree = dialog.getTreeWidget();
    tree->clear();
    tree->setColumnCount(3);
    tree->header()->setStretchLastSection(true);
    QStringList headers;
    headers << tr("Name") << tr("Created Time") << tr("Modified Time");
    tree->setHeaderLabels(headers);

    for (int i = 0; i < files.size(); ++i) {
        const VNoteFile *file = files[i];
        QString createdTime = VUtils::displayDateTime(file->getCreatedTimeUtc().toLocalTime());
        QString modifiedTime = VUtils::displayDateTime(file->getModifiedTimeUtc().toLocalTime());
        QStringList cols;
        cols << file->getName() << createdTime << modifiedTime;
        QTreeWidgetItem *item = new QTreeWidgetItem(tree, cols);

        item->setData(0, Qt::UserRole, i);
    }

    dialog.treeUpdated();

    if (dialog.exec()) {
        QVector<QVariant> data = dialog.getSortedData();
        Q_ASSERT(data.size() == files.size());
        QVector<int> sortedIdx(data.size(), -1);
        for (int i = 0; i < data.size(); ++i) {
            sortedIdx[i] = data[i].toInt();
        }

        qDebug() << "sorted notes" << sortedIdx;
    }
}
