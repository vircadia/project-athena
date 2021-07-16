//
//  ScriptContextHelper.cpp
//  shared/src
//
//  Created by Dale Glass on 04/04/2021.
//  Copyright 2021 Vircadia Contributors
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptContextHelper.h"

QThreadStorage<QList<QStringList>> ScriptContextHelper::_context;

void  ScriptContextHelper::push(QStringList context) {
    QList<QStringList> data = _context.localData();

    data.append(context);

    _context.setLocalData(data);
}

QStringList ScriptContextHelper::get() {
    QList<QStringList> data = _context.localData();

    if ( data.isEmpty() ) {
        return QStringList();
    } else {
        return data.last();
    }

    //return _context.localData();
}

void ScriptContextHelper::pop() {
    QList<QStringList> data = _context.localData();
    
    if (!data.isEmpty()) {
        data.removeLast();
    }

    _context.setLocalData(data);
}

