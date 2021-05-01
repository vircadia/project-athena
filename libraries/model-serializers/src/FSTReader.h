//
//  FSTReader.h
//
//
//  Created by Clement on 3/26/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_FSTReader_h
#define hifi_FSTReader_h

#include <QBuffer>
#include <QVariantHash>

static const unsigned int FST_VERSION = 1;
static const QString FST_VERSION_FIELD = "version";
static const QString NAME_FIELD = "name";
static const QString TYPE_FIELD = "type";
static const QString FILENAME_FIELD = "filename";
static const QString TEXDIR_FIELD = "texdir";
static const QString LOD_FIELD = "lod";
static const QString JOINT_INDEX_FIELD = "jointIndex";
static const QString SCALE_FIELD = "scale";
static const QString TRANSLATION_X_FIELD = "tx";
static const QString TRANSLATION_Y_FIELD = "ty";
static const QString TRANSLATION_Z_FIELD = "tz";
static const QString JOINT_FIELD = "joint";
static const QString BLENDSHAPE_FIELD = "bs";
static const QString SCRIPT_FIELD = "script";
static const QString JOINT_NAME_MAPPING_FIELD = "jointMap";
static const QString MATERIAL_MAPPING_FIELD = "materialMap";
static const QString COMMENT_FIELD = "comment";

class FSTReader {
public:

    enum ModelType {
        ENTITY_MODEL,
        HEAD_MODEL,
        BODY_ONLY_MODEL,
        HEAD_AND_BODY_MODEL,
        ATTACHMENT_MODEL
    };

    /// Reads an FST mapping from the supplied data.
    static QVariantHash readMapping(const QByteArray& data);

    /// Writes an FST mapping to a byte array.
    static QByteArray writeMapping(const QVariantHash& mapping);

    /// Predicts the type of model by examining the mapping 
    static ModelType predictModelType(const QVariantHash& mapping);

    static QVector<QString> getScripts(const QUrl& fstUrl, const QVariantHash& mapping = QVariantHash());

    static QString getNameFromType(ModelType modelType);
    static FSTReader::ModelType getTypeFromName(const QString& name);
    static QVariantHash downloadMapping(const QString& url);

private:
    static void writeVariant(QBuffer& buffer, QVariantHash::const_iterator& it);
    static QVariantHash parseMapping(QIODevice* device);

    static QHash<FSTReader::ModelType, QString> _typesToNames;
    static QHash<QString, FSTReader::ModelType> _namesToTypes;
};

#endif // hifi_FSTReader_h