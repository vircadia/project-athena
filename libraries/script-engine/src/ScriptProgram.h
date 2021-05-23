//
//  ScriptProgram.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/2/21.
//  Copyright 2021 Vircadia contributors.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptProgram_h
#define hifi_ScriptProgram_h

#include <QtCore/QSharedPointer>

class ScriptProgram;
class ScriptSyntaxCheckResult;
using ScriptProgramPointer = QSharedPointer<ScriptProgram>;
using ScriptSyntaxCheckResultPointer = QSharedPointer<ScriptSyntaxCheckResult>;

class ScriptProgram {
public:
    virtual ScriptSyntaxCheckResultPointer checkSyntax() const = 0;
    virtual QString fileName() const = 0;
    virtual QString sourceCode() const = 0;
};

class ScriptSyntaxCheckResult {
public:
    enum State
    {
        Error = 0,
        Intermediate = 1,
        Valid = 2
    };

public:
    virtual int errorColumnNumber() const = 0;
    virtual int errorLineNumber() const = 0;
    virtual QString errorMessage() const = 0;
    virtual State state() const = 0;
};

#endif // hifi_ScriptProgram_h