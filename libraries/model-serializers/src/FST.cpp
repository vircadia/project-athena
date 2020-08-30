//
//  FST.cpp
//
//  Created by Ryan Huffman on 12/11/15.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "FST.h"

#include <QDir>
#include <QFileInfo>
#include <hfm/HFM.h>

constexpr float DEFAULT_SCALE { 1.0f };

FST::FST(QString fstPath, QVariantHash data) : _fstPath(std::move(fstPath)) {

    auto setValueFromFSTData = [&data] (const QString& propertyID, auto &targetProperty) mutable {
        if (data.contains(propertyID)) {
            targetProperty = data[propertyID].toString();
            data.remove(propertyID);
        }
    };
    setValueFromFSTData(NAME_FIELD, _name);
    setValueFromFSTData(FILENAME_FIELD, _modelPath);

    if (data.contains(SCRIPT_FIELD)) {
        QVariantList scripts = data.values(SCRIPT_FIELD);
        for (const auto& script : scripts) {
            _scriptPaths.push_back(script.toString());
        }
        data.remove(SCRIPT_FIELD);
    }

    _other = data;
}

FST* FST::createFSTFromModel(const QString& fstPath, const QString& modelFilePath, const hfm::Model& hfmModel) {
    QVariantHash mapping;

    // mixamo files - in the event that a mixamo file was edited by some other tool, it's likely the applicationName will
    // be rewritten, so we detect the existence of several different blendshapes which indicate we're likely a mixamo file
    bool likelyMixamoFile = hfmModel.applicationName == "mixamo.com" ||
        (hfmModel.blendshapeChannelNames.contains("BrowsDown_Right") &&
            hfmModel.blendshapeChannelNames.contains("MouthOpen") &&
            hfmModel.blendshapeChannelNames.contains("Blink_Left") &&
            hfmModel.blendshapeChannelNames.contains("Blink_Right") &&
            hfmModel.blendshapeChannelNames.contains("Squint_Right"));

    mapping.insert(NAME_FIELD, QFileInfo(fstPath).baseName());
    mapping.insert(FILENAME_FIELD, QFileInfo(modelFilePath).fileName());
    mapping.insert(TEXDIR_FIELD, "textures");

    // mixamo/autodesk defaults
    mapping.insert(SCALE_FIELD, DEFAULT_SCALE);
    QVariantHash joints = mapping.value(JOINT_FIELD).toHash();
        joints.insert("jointEyeLeft", hfmModel.jointIndices.contains("jointEyeLeft") ? "jointEyeLeft" :
            (hfmModel.jointIndices.contains("EyeLeft") ? "EyeLeft" : "LeftEye"));

    joints.insert("jointEyeRight", hfmModel.jointIndices.contains("jointEyeRight") ? "jointEyeRight" :
            hfmModel.jointIndices.contains("EyeRight") ? "EyeRight" : "RightEye");

    joints.insert("jointNeck", hfmModel.jointIndices.contains("jointNeck") ? "jointNeck" : "Neck");
    joints.insert("jointRoot", "Hips");
    joints.insert("jointLean", "Spine");
    joints.insert("jointLeftHand", "LeftHand");
    joints.insert("jointRightHand", "RightHand");

    const char* topName = likelyMixamoFile ? "HeadTop_End" : "HeadEnd";
    joints.insert("jointHead", hfmModel.jointIndices.contains(topName) ? topName : "Head");

    mapping.insert(JOINT_FIELD, joints);

    QVariantHash jointIndices;
    for (int i = 0; i < hfmModel.joints.size(); i++) {
        jointIndices.insert(hfmModel.joints.at(i).name, QString::number(i));
    }
    mapping.insert(JOINT_INDEX_FIELD, jointIndices);


    // If there are no blendshape mappings, and we detect that this is likely a mixamo file,
    // then we can add the default mixamo to blendshape mappings.
    if (likelyMixamoFile) {
        QVariantHash blendshapes;
        blendshapes.insertMulti("BrowsD_L", QVariantList() << "BrowsDown_Left" << 1.0);
        blendshapes.insertMulti("BrowsD_R", QVariantList() << "BrowsDown_Right" << 1.0);
        blendshapes.insertMulti("BrowsU_C", QVariantList() << "BrowsUp_Left" << 1.0);
        blendshapes.insertMulti("BrowsU_C", QVariantList() << "BrowsUp_Right" << 1.0);
        blendshapes.insertMulti("BrowsU_L", QVariantList() << "BrowsUp_Left" << 1.0);
        blendshapes.insertMulti("BrowsU_R", QVariantList() << "BrowsUp_Right" << 1.0);
        blendshapes.insertMulti("ChinLowerRaise", QVariantList() << "Jaw_Up" << 1.0);
        blendshapes.insertMulti("ChinUpperRaise", QVariantList() << "UpperLipUp_Left" << 0.5);
        blendshapes.insertMulti("ChinUpperRaise", QVariantList() << "UpperLipUp_Right" << 0.5);
        blendshapes.insertMulti("EyeBlink_L", QVariantList() << "Blink_Left" << 1.0);
        blendshapes.insertMulti("EyeBlink_R", QVariantList() << "Blink_Right" << 1.0);
        blendshapes.insertMulti("EyeOpen_L", QVariantList() << "EyesWide_Left" << 1.0);
        blendshapes.insertMulti("EyeOpen_R", QVariantList() << "EyesWide_Right" << 1.0);
        blendshapes.insertMulti("EyeSquint_L", QVariantList() << "Squint_Left" << 1.0);
        blendshapes.insertMulti("EyeSquint_R", QVariantList() << "Squint_Right" << 1.0);
        blendshapes.insertMulti("JawFwd", QVariantList() << "JawForeward" << 1.0);
        blendshapes.insertMulti("JawLeft", QVariantList() << "JawRotateY_Left" << 0.5);
        blendshapes.insertMulti("JawOpen", QVariantList() << "MouthOpen" << 0.7);
        blendshapes.insertMulti("JawRight", QVariantList() << "Jaw_Right" << 1.0);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "JawForeward" << 0.39);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "Jaw_Down" << 0.36);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "MouthNarrow_Left" << 1.0);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "MouthNarrow_Right" << 1.0);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "MouthWhistle_NarrowAdjust_Left" << 0.5);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "MouthWhistle_NarrowAdjust_Right" << 0.5);
        blendshapes.insertMulti("LipsFunnel", QVariantList() << "TongueUp" << 1.0);
        blendshapes.insertMulti("LipsLowerClose", QVariantList() << "LowerLipIn" << 1.0);
        blendshapes.insertMulti("LipsLowerDown", QVariantList() << "LowerLipDown_Left" << 0.7);
        blendshapes.insertMulti("LipsLowerDown", QVariantList() << "LowerLipDown_Right" << 0.7);
        blendshapes.insertMulti("LipsLowerOpen", QVariantList() << "LowerLipOut" << 1.0);
        blendshapes.insertMulti("LipsPucker", QVariantList() << "MouthNarrow_Left" << 1.0);
        blendshapes.insertMulti("LipsPucker", QVariantList() << "MouthNarrow_Right" << 1.0);
        blendshapes.insertMulti("LipsUpperClose", QVariantList() << "UpperLipIn" << 1.0);
        blendshapes.insertMulti("LipsUpperOpen", QVariantList() << "UpperLipOut" << 1.0);
        blendshapes.insertMulti("LipsUpperUp", QVariantList() << "UpperLipUp_Left" << 0.7);
        blendshapes.insertMulti("LipsUpperUp", QVariantList() << "UpperLipUp_Right" << 0.7);
        blendshapes.insertMulti("MouthDimple_L", QVariantList() << "Smile_Left" << 0.25);
        blendshapes.insertMulti("MouthDimple_R", QVariantList() << "Smile_Right" << 0.25);
        blendshapes.insertMulti("MouthFrown_L", QVariantList() << "Frown_Left" << 1.0);
        blendshapes.insertMulti("MouthFrown_R", QVariantList() << "Frown_Right" << 1.0);
        blendshapes.insertMulti("MouthLeft", QVariantList() << "Midmouth_Left" << 1.0);
        blendshapes.insertMulti("MouthRight", QVariantList() << "Midmouth_Right" << 1.0);
        blendshapes.insertMulti("MouthSmile_L", QVariantList() << "Smile_Left" << 1.0);
        blendshapes.insertMulti("MouthSmile_R", QVariantList() << "Smile_Right" << 1.0);
        blendshapes.insertMulti("Puff", QVariantList() << "CheekPuff_Left" << 1.0);
        blendshapes.insertMulti("Puff", QVariantList() << "CheekPuff_Right" << 1.0);
        blendshapes.insertMulti("Sneer", QVariantList() << "NoseScrunch_Left" << 0.75);
        blendshapes.insertMulti("Sneer", QVariantList() << "NoseScrunch_Right" << 0.75);
        blendshapes.insertMulti("Sneer", QVariantList() << "Squint_Left" << 0.5);
        blendshapes.insertMulti("Sneer", QVariantList() << "Squint_Right" << 0.5);
        mapping.insert(BLENDSHAPE_FIELD, blendshapes);
    }
    return new FST(fstPath, mapping);
}

QString FST::absoluteModelPath() const {
    QFileInfo fileInfo{ _fstPath };
    QDir dir{ fileInfo.absoluteDir() };
    return dir.absoluteFilePath(_modelPath);
}

void FST::setName(const QString& name) {
    _name = name;
    emit nameChanged(name);
}

void FST::setModelPath(const QString& modelPath) {
    _modelPath = modelPath;
    emit modelPathChanged(modelPath);
}

QVariantHash FST::getMapping() const {
    QVariantHash mapping;
    mapping.unite(_other);
    mapping.insert(NAME_FIELD, _name);
    mapping.insert(FILENAME_FIELD, _modelPath);
    for (const auto& scriptPath : _scriptPaths) {
        mapping.insertMulti(SCRIPT_FIELD, scriptPath);
    }
    return mapping;
}

bool FST::write() {
    QFile fst(_fstPath);
    if (!fst.open(QIODevice::WriteOnly)) {
        return false;
    }
    fst.write(FSTReader::writeMapping(getMapping()));
    return true;
}
