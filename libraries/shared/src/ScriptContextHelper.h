//
//  ScriptContextHelper.h
//  shared/src
//
//  Created by Dale Glass on 04/04/2021.
//  Copyright 2021 Vircadia Contributors
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef vircadia_ScriptContextHelper_h
#define vircadia_ScriptContextHelper_h

#include <QObject>
#include <QStringList>
#include <QThreadStorage>
#include <QList>


class ScriptContextHelper {
public:
    static void push(QStringList context);
    static QStringList get();
    static void pop();

private:
    static QThreadStorage<QList<QStringList>> _context;
};

#endif