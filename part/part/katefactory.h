/* This file is part of the KDE libraries
   Copyright (C) 2001-2003 Christoph Cullmann <cullmann@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __KATE_FACTORY_H__
#define __KATE_FACTORY_H__

#include <kparts/factory.h>

#include <ktrader.h>
#include <kinstance.h>
#include <kaboutdata.h>

class KateCmd;
class KateFileTypeManager;
class KateSchemaManager;

class KDirWatch;

class KatePartPluginInfo
{
  public:
    bool load;
    KService::Ptr service;
};

class KateFactory
{
  private:
    KateFactory ();
    
  public:
    ~KateFactory ();

    static KateFactory *self ();
    
    KParts::Part *createPartObject ( QWidget *parentWidget, const char *widgetName, QObject *parent, const char *name, const char *classname, const QStringList &args );

    inline KInstance *instance () { return &m_instance; };

    void registerDocument ( class KateDocument *doc );
    void deregisterDocument ( class KateDocument *doc );

    void registerView ( class KateView *view );
    void deregisterView ( class KateView *view );

    void registerRenderer ( class KateRenderer  *renderer );
    void deregisterRenderer ( class KateRenderer  *renderer );

    inline QPtrList<class KateDocument> *documents () { return &m_documents; };

    inline QPtrList<class KateView> *views () { return &m_views; };

    inline QPtrList<class KateRenderer> *renderers () { return &m_renderers; };
    
    inline QMemArray<KatePartPluginInfo *> *plugins () { return &m_plugins; };

    inline KDirWatch *dirWatch () { return m_dirWatch; };

    inline KateFileTypeManager *fileTypeManager () { return m_fileTypeManager; };

    inline KateSchemaManager *schemaManager () { return m_schemaManager; };

  private:
    static KateFactory *s_self;
    
    KAboutData m_aboutData;
    KInstance m_instance;
    
    QPtrList<class KateDocument> m_documents;
    QPtrList<class KateView> m_views;
    QPtrList<class KateRenderer> m_renderers;
    
    QMemArray<KatePartPluginInfo *> m_plugins;
    
    KDirWatch *m_dirWatch;  
  
    KateFileTypeManager *m_fileTypeManager;
    KateSchemaManager *m_schemaManager;
};

#endif

// kate: space-indent on; indent-width 2; replace-tabs on;
