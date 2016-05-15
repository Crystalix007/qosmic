/***************************************************************************
 *   Copyright (C) 2007-2016 by David Bitseff                              *
 *   bitsed@gmail.com                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include <QFile>
#include <QSettings>
#include <QKeyEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QAction>
#include <QFontDatabase>
#include "luaeditor.h"
#include "logger.h"

namespace Lua {

LuaEditor::LuaEditor(QWidget* pa) : QTextEdit(pa), parent(pa)
{
	highlighter = new Highlighter(document());

	QStringList names = QFontDatabase().families();
	QStringList prefs;
	prefs << "DejaVu Sans Mono" << "Bitstream Vera Sans Mono" << "Terminus"
			<< "Fixed";
	foreach (QString p, prefs)
		if (names.contains(p, Qt::CaseInsensitive))
		{
			logInfo(QString("LuaEditor::LuaEditor : found font %1").arg(p));
			setFontFamily(p);
			break;
		}

	QAction* a = new QAction(tr("Open a file"), this);
	a->setShortcut(tr("Ctrl+1"));
	a->setShortcutContext(Qt::WidgetShortcut);
	addAction(a);
	connect(a, SIGNAL(triggered()), this, SLOT(load()));

	a = new QAction(tr("Save file"), this);
	a->setShortcut(tr("Ctrl+2"));
	a->setShortcutContext(Qt::WidgetShortcut);
	addAction(a);
	connect(a, SIGNAL(triggered()), this, SLOT(save()));

	a = new QAction(tr("Save to file"), this);
	a->setShortcut(tr("Ctrl+3"));
	a->setShortcutContext(Qt::WidgetShortcut);
	addAction(a);
	connect(a, SIGNAL(triggered()), this, SLOT(saveAs()));
}


LuaEditor::~LuaEditor()
{
	delete highlighter;
}

void LuaEditor::keyPressEvent(QKeyEvent* e)
{
	if ((e->key() == Qt::Key_L || e->key() == Qt::Key_Slash
		 || e->key() == Qt::Key_Question)
			&& e->modifiers() & Qt::ControlModifier)
	{
		if (textCursor().hasSelection())
		{
			QString str = textCursor().selectedText();
			QStringList slist = str.split(QChar(0x2029));
			QMutableStringListIterator it(slist);
			if (e->modifiers() & Qt::ShiftModifier)
				while (it.hasNext())
					it.next().replace(QRegExp("^\\s*-- ?"), "");
			else
				while (it.hasNext())
					it.next().prepend("-- ");

			textCursor().insertText(slist.join("\n"));
		}
		else
		{
			QTextCursor c = textCursor();
			if (e->modifiers() & Qt::ShiftModifier)
			{
				c.clearSelection();
				int pos = c.position();
				c.movePosition(QTextCursor::StartOfLine);
				c.setPosition(pos, QTextCursor::KeepAnchor);
				QString str = c.selectedText();
				str.replace(QRegExp("^\\s*-- ?"), "");
				c.insertText(str);
			}
			else
			{
				int pos = c.position();
				c.movePosition(QTextCursor::StartOfLine);
				c.insertText("-- ");
				c.setPosition(pos);
			}
		}
	}
	else if ( (e->key() == Qt::Key_Tab
			|| e->key() == Qt::Key_Underscore
			|| e->key() == Qt::Key_5)
			&& textCursor().hasSelection())
		// a simple block indenting key
	{
		QTextCursor c(textCursor());
		if (e->modifiers() & Qt::ControlModifier)
		{
			int start = textCursor().selectionEnd();
			int anchor = textCursor().selectionStart();
			c.beginEditBlock();
			c.setPosition(start);
			c.movePosition(QTextCursor::StartOfBlock);
			while (c.position() + c.block().length() > anchor)
			{
				QString str = c.block().text();
				if (str.startsWith(QChar('\t')))
					c.deleteChar();
				else if (str.contains(QRegExp("^\\s+.*")))
				{
// 					c.deleteChar();c.deleteChar();c.deleteChar();c.deleteChar();
				}
				c.movePosition(QTextCursor::Up);
				c.movePosition(QTextCursor::StartOfBlock);
			}
			c.endEditBlock();
		}
		else
		{
			int anchor = textCursor().selectionStart();
			int pos = textCursor().selectionEnd();
			c.beginEditBlock();
			c.setPosition(pos);
			c.movePosition(QTextCursor::StartOfBlock);
			while (c.position() + c.block().length() > anchor)
			{
				c.insertText("\t");
				c.movePosition(QTextCursor::Up);
				c.movePosition(QTextCursor::StartOfBlock);
			}
			c.endEditBlock();
		}
	}
	else if (e->key() == Qt::Key_Return)
	{
		int pos = textCursor().position();
		QTextCursor c(textCursor());
		c.select(QTextCursor::LineUnderCursor);
		QString text(c.selectedText());
		c.clearSelection();
		QRegExp re("^(\\s+)");
		if ( re.indexIn(text) > -1 )
		{
			QString ws = re.cap(0);
			c.beginEditBlock();
			c.setPosition(pos);
			c.insertText("\n" + ws);
			c.endEditBlock();
			ensureCursorVisible();
		}
		else
			QTextEdit::keyPressEvent(e);
	}
	else
		QTextEdit::keyPressEvent(e);
}

bool LuaEditor::load(QString filename)
{
	if (filename.isEmpty())
	{
		filename = QString(".lua");
		filename = QFileDialog::getOpenFileName(this, tr("Open a script"),
				script_filename, tr("lua source (*.lua);;All files (*)"));
		if (filename.isEmpty())
			return false;
	}

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, tr("Application error"),
									tr("Cannot read file %1\n").arg(filename));
		return false;
	}
	QTextStream os(&file);
	QString p = os.readAll();
	file.close();
	setPlainText(p);
	script_filename = filename;
	emit scriptLoaded();
	return true;
}


bool LuaEditor::saveAs(QString filename)
{
	if (filename.isEmpty())
	{
		QFileDialog dialog(this, tr("Save a script"), script_filename,
		tr("lua source (*.lua);;All files (*)"));
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		dialog.setDefaultSuffix("lua");
		dialog.selectFile(script_filename);
		if (dialog.exec())
			filename = dialog.selectedFiles().first();
		else
			return false;
	}

	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		QMessageBox::warning(this, tr("Application error"),
									tr("Cannot write file %1\n").arg(filename));
		return false;
	}
	QTextStream os(&file);
	os << toPlainText();
	file.close();
	script_filename = filename;
	emit scriptSaved();
	return true;
}

bool LuaEditor::save()
{
	return saveAs(script_filename);
}


QString LuaEditor::scriptFile() const
{
	return script_filename;
}

}

void Lua::LuaEditor::setCurrentFont(const QFont& f)
{
	QString text(toPlainText());
	QTextEdit::setCurrentFont(f);
	setPlainText(text);
}
