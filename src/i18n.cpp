/*
 * Copyright (C) 2014 AnthonOS Open Source Community
 * Copyright (C) 2014 - 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QTextCodec>
#include <QLocale>
#include <QDebug>

#include "i18n.h"

I18N::I18N(const QString & path, const QString & encoding)
{
    m_path = path; m_encoding = encoding;
}

QString I18N::getEncoding() const { return m_encoding; }
void I18N::setEncoding(const QString & encoding) { m_encoding = encoding; }

QString I18N::getPath() const { return m_path; }
void I18N::setPath(const QString & path) { m_path = path; }

void I18N::translate()
{
    m_apTranslator.load(":/" + m_path + "/" + QLocale::system().name() + ".qm");

    m_qtTranslator.load("qt_" + QLocale::system().name(), 
        QLibraryInfo::location(QLibraryInfo::TranslationsPath));

    QCoreApplication::installTranslator(&m_apTranslator);
    QCoreApplication::installTranslator(&m_qtTranslator);
}
