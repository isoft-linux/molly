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

#ifndef I18N_H
#define I18N_H

#include <QTranslator>

class I18N
{
public:
    I18N(const QString & path, const QString & encoding);

    QString getPath() const;
    void setPath(const QString & path);

    QString getEncoding() const;
    void setEncoding(const QString & encoding);

    void translate();

private:
    QString m_path, m_encoding;
    QTranslator m_apTranslator, m_qtTranslator;
};

#endif // I18N_H
