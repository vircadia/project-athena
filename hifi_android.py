import hifi_utils
import json
import os
import platform
import re
import shutil
import xml.etree.ElementTree as ET
import functools
import zipfile

print = functools.partial(print, flush=True)

ANDROID_PACKAGE_URL = 'https://cdn-1.vircadia.com/eu-c-1/vircadia-public/dependencies/android/'

ANDROID_PACKAGES = {
    'qt' : {
        'file': 'qt-5.11.1_linux_armv8-libcpp_openssl_patched.tgz',
        'checksum': 'aa449d4bfa963f3bc9a9dfe558ba29df',
    },
    'bullet': {
        'file': 'bullet-2.88_armv8-libcpp.tgz',
        'checksum': '81642779ccb110f8c7338e8739ac38a0',
    },            
    'draco': {
        'file': 'draco_armv8-libcpp.tgz',
        'checksum': '617a80d213a5ec69fbfa21a1f2f738cd',
    },
    'glad': {
        'file': 'glad_armv8-libcpp.zip',
        'checksum': 'a8ee8584cf1ccd34766c7ddd9d5e5449',
    },
    'gvr': {
        'file': 'gvrsdk_v1.101.0.tgz',
        'checksum': '57fd02baa069176ba18597a29b6b4fc7',
    },
    'nvtt': {
        'file': 'nvtt_armv8-libcpp.zip',
        'checksum': 'eb46d0b683e66987190ed124aabf8910',
        'sharedLibFolder': 'lib',
        'includeLibs': ['libnvtt.so', 'libnvmath.so', 'libnvimage.so', 'libnvcore.so']
    },
    'ovr_sdk_mobile_1.37.0': {
        'file': 'ovr_sdk_mobile_1.37.0.zip',
        'checksum': '6040e1966f335a3e5015295154cd7383',
        'sharedLibFolder': 'VrApi/Libs/Android/arm64-v8a/Release',
        'includeLibs': ['libvrapi.so']
    },
    'ovr_platform_sdk_23.0.0': {
        'file': 'ovr_platform_sdk_23.0.0.zip',
        'checksum': '29d02b560f60d0fa7b8a64cd965dd55b',
        'sharedLibFolder': 'Android/libs/arm64-v8a',
        'includeLibs': ['libovrplatformloader.so']
    },
    'openssl': {
        'file': 'openssl-1.1.0g_armv8.tgz',
        'checksum': 'cabb681fbccd79594f65fcc266e02f32'
    },
    'polyvox': {
        'file': 'polyvox_armv8-libcpp.tgz',
        'checksum': 'dba88b3a098747af4bb169e9eb9af57e',
        'sharedLibFolder': 'lib',
        'includeLibs': ['Release/libPolyVoxCore.so', 'libPolyVoxUtil.so'],
    },
    'tbb': {
        'file': 'tbb-2018_U1_armv8_libcpp.tgz',
        'checksum': '20768f298f53b195e71b414b0ae240c4',
        'sharedLibFolder': 'lib/release',
        'includeLibs': ['libtbb.so', 'libtbbmalloc.so'],
    },
    'etc2comp': {
        'file': 'etc2comp-patched-armv8-libcpp.tgz',
        'checksum': '14b02795d774457a33bbc60e00a786bc'
    },
    'breakpad': {
        'file': 'breakpad.tgz',
        'checksum': 'ddcb23df336b08017042ba4786db1d9e',
        'sharedLibFolder': 'lib',
        'includeLibs': {'libbreakpad_client.a'}
    },
    'webrtc': {
        'file': 'webrtc-20190626-android.tar.gz',
        'checksum': 'e2dccd3d8efdcba6d428c87ba7fb2a53'
    }
}

ANDROID_PLATFORM_PACKAGES = {
    'Darwin' : {
        'qt': {
            'file': 'qt-5.11.1_osx_armv8-libcpp_openssl_patched.tgz',
            'checksum': 'c83cc477c08a892e00c71764dca051a0'
        },
    },
    'Windows' : {
        'qt': {
            'file': 'qt-5.11.1_win_armv8-libcpp_openssl_patched.tgz',
            'checksum': '0582191cc55431aa4f660848a542883e'
        },
    }
}

QT5_DEPS = [
    'Qt5Concurrent',
    'Qt5Core',
    'Qt5Gui',
    'Qt5Multimedia',
    'Qt5Network',
    'Qt5OpenGL',
    'Qt5Qml',
    'Qt5Quick',
    'Qt5QuickControls2',
    'Qt5QuickTemplates2',
    'Qt5Script',
    'Qt5ScriptTools',
    'Qt5Svg',
    'Qt5WebChannel',
    'Qt5WebSockets',
    'Qt5Widgets',
    'Qt5XmlPatterns',
    # Android specific
    'Qt5AndroidExtras',
    'Qt5WebView',
]

def getPlatformPackages():
    result = ANDROID_PACKAGES.copy()
    system = platform.system()
    if system in ANDROID_PLATFORM_PACKAGES:
        platformPackages = ANDROID_PLATFORM_PACKAGES[system]
        result = { **result, **platformPackages }
    return result

def getPackageUrl(package):
    url = ANDROID_PACKAGE_URL
    if 'baseUrl' in package:
        url = package['baseUrl']
    url += package['file']
    if 'versionId' in package:
        url += '?versionId=' + package['versionId']
    return url

def copyAndroidLibs(packagePath, appPath):
    androidPackages = getPlatformPackages()
    jniPath = os.path.join(appPath, 'src/main/jniLibs/arm64-v8a')
    if not os.path.isdir(jniPath):
        os.makedirs(jniPath)
    for packageName in androidPackages:
        package = androidPackages[packageName]
        if 'sharedLibFolder' in package:
            sharedLibFolder = os.path.join(packagePath, packageName, package['sharedLibFolder'])
            if 'includeLibs' in package:
                for lib in package['includeLibs']:
                    sourceFile = os.path.join(sharedLibFolder, lib)
                    destFile = os.path.join(jniPath, os.path.split(lib)[1])
                    if not os.path.isfile(destFile):
                        print("Copying {}".format(lib))
                        shutil.copy(sourceFile, destFile)

    gvrLibFolder = os.path.join(packagePath, 'gvr/gvr-android-sdk-1.101.0/libraries')
    audioSoOut = os.path.join(gvrLibFolder, 'libgvr_audio.so')
    if not os.path.isfile(audioSoOut):
        audioAar = os.path.join(gvrLibFolder, 'sdk-audio-1.101.0.aar')
        with zipfile.ZipFile(audioAar) as z:
            with z.open('jni/arm64-v8a/libgvr_audio.so') as f:
                with open(audioSoOut, 'wb') as of:
                    shutil.copyfileobj(f, of)

    audioSoOut2 = os.path.join(jniPath, 'libgvr_audio.so')
    if not os.path.isfile(audioSoOut2):
        shutil.copy(audioSoOut, audioSoOut2)

    baseSoOut = os.path.join(gvrLibFolder, 'libgvr.so')
    if not os.path.isfile(baseSoOut):
        baseAar = os.path.join(gvrLibFolder, 'sdk-base-1.101.0.aar')
        with zipfile.ZipFile(baseAar) as z:
            with z.open('jni/arm64-v8a/libgvr.so') as f:
                with open(baseSoOut, 'wb') as of:
                    shutil.copyfileobj(f, of)

    baseSoOut2 = os.path.join(jniPath, 'libgvr.so')
    if not os.path.isfile(baseSoOut2):
        shutil.copy(baseSoOut, baseSoOut2)

class QtPackager:
    def __init__(self, appPath, qtRootPath):
        self.appPath = appPath
        self.qtRootPath = qtRootPath
        self.jniPath = os.path.join(self.appPath, 'src/main/jniLibs/arm64-v8a')
        self.assetPath = os.path.join(self.appPath, 'src/main/assets')
        self.qtAssetPath = os.path.join(self.assetPath, '--Added-by-androiddeployqt--')
        self.qtAssetCacheList = os.path.join(self.qtAssetPath, 'qt_cache_pregenerated_file_list')
        # Jars go into the qt library
        self.jarPath = os.path.realpath(os.path.join(self.appPath, '../../libraries/qt/libs'))
        self.xmlFile = os.path.join(self.appPath, 'src/main/res/values/libs.xml')
        self.files = []
        self.features = []
        self.permissions = []

    def copyQtDeps(self):
        for lib in QT5_DEPS:
            libfile = os.path.join(self.qtRootPath, "lib/lib{}.so".format(lib))
            if not os.path.exists(libfile):
                continue
            self.files.append(libfile)
            androidDeps = os.path.join(self.qtRootPath, "lib/{}-android-dependencies.xml".format(lib))
            if not os.path.exists(androidDeps):
                continue

            tree = ET.parse(androidDeps)
            root = tree.getroot()                
            for item in root.findall('./dependencies/lib/depends/*'):
                if (item.tag == 'lib') or (item.tag == 'bundled'):
                    relativeFilename = item.attrib['file']
                    if (relativeFilename.startswith('qml')):
                        continue
                    filename = os.path.join(self.qtRootPath, relativeFilename)
                    self.files.extend(hifi_utils.recursiveFileList(filename, excludeNamePattern=r"^\."))
                elif item.tag == 'jar' and 'bundling' in item.attrib and item.attrib['bundling'] == "1":
                    self.files.append(os.path.join(self.qtRootPath, item.attrib['file']))
                elif item.tag == 'permission':
                    self.permissions.append(item.attrib['name'])
                elif item.tag == 'feature':
                    self.features.append(item.attrib['name'])

    def scanQmlImports(self):
        qmlImportCommandFile = os.path.join(self.qtRootPath, 'bin/qmlimportscanner')
        system = platform.system()
        if 'Windows' == system:
            qmlImportCommandFile += ".exe"
        if not os.path.isfile(qmlImportCommandFile):
            raise RuntimeError("Couldn't find qml import scanner")
        qmlRootPath = hifi_utils.scriptRelative('interface/resources/qml')
        qmlImportPath = os.path.join(self.qtRootPath, 'qml')
        commandResult = hifi_utils.executeSubprocessCapture([
            qmlImportCommandFile, 
            '-rootPath', qmlRootPath, 
            '-importPath', qmlImportPath
        ])
        qmlImportResults = json.loads(commandResult)
        for item in qmlImportResults:
            if 'path' not in item:
                continue
            path = os.path.realpath(item['path'])
            if not os.path.exists(path):
                continue
            basePath = path
            if os.path.isfile(basePath):
                basePath = os.path.dirname(basePath)
            basePath = os.path.normcase(basePath)
            if basePath.startswith(qmlRootPath):
                continue
            self.files.extend(hifi_utils.recursiveFileList(path, excludeNamePattern=r"^\."))

    def processFiles(self):
        self.files = list(set(self.files))
        self.files.sort()
        libsXmlRoot = ET.Element('resources')
        qtLibsNode = ET.SubElement(libsXmlRoot, 'array', {'name':'qt_libs'})
        bundledLibsNode = ET.SubElement(libsXmlRoot, 'array', {'name':'bundled_in_lib'})
        bundledAssetsNode = ET.SubElement(libsXmlRoot, 'array', {'name':'bundled_in_assets'})
        libPrefix = 'lib'
        for sourceFile in self.files:
            if not os.path.isfile(sourceFile):
                raise RuntimeError("Unable to find dependency file " + sourceFile)
            relativePath = os.path.relpath(sourceFile, self.qtRootPath).replace('\\', '/')
            destinationFile = None
            if relativePath.endswith('.so'):
                garbledFileName = None
                if relativePath.startswith(libPrefix):
                    garbledFileName = relativePath[4:]
                    p = re.compile(r'lib(Qt5.*).so')
                    m = p.search(garbledFileName)
                    if not m:
                        raise RuntimeError("Huh?")
                    libName = m.group(1)
                    ET.SubElement(qtLibsNode, 'item').text = libName
                else:
                    garbledFileName = 'lib' + relativePath.replace('/', '_'[0])
                    value = "{}:{}".format(garbledFileName, relativePath).replace('\\', '/')
                    ET.SubElement(bundledLibsNode, 'item').text = value
                destinationFile = os.path.join(self.jniPath, garbledFileName)
            elif relativePath.startswith('jar'):
                destinationFile = os.path.join(self.jarPath, relativePath[4:])
            else:
                value = "--Added-by-androiddeployqt--/{}:{}".format(relativePath,relativePath).replace('\\', '/')
                ET.SubElement(bundledAssetsNode, 'item').text = value
                destinationFile = os.path.join(self.qtAssetPath, relativePath)

            destinationParent = os.path.realpath(os.path.dirname(destinationFile))
            if not os.path.isdir(destinationParent):
                os.makedirs(destinationParent)
            if not os.path.isfile(destinationFile):
                shutil.copy(sourceFile, destinationFile)

        tree = ET.ElementTree(libsXmlRoot)
        tree.write(self.xmlFile, 'UTF-8', True)

    def generateAssetsFileList(self):
        print("Implement asset file list")
        # outputFilename = os.path.join(self.qtAssetPath, "qt_cache_pregenerated_file_list")
        # fileList = hifi_utils.recursiveFileList(self.qtAssetPath)
        # fileMap = {}
        # for fileName in fileList:
        #     relativeFileName = os.path.relpath(fileName, self.assetPath)
        #     dirName, localFileName = os.path.split(relativeFileName)
        #     if not dirName in fileMap:
        #         fileMap[dirName] = []
        #     fileMap[dirName].append(localFileName)

        # for dirName in fileMap:
        #     for localFileName in fileMap[dirName]:
        #         ????

        #
        # Gradle version
        #
        # DataOutputStream fos = new DataOutputStream(new FileOutputStream(outputFile));
        # for (Map.Entry<String, List<String>> e: directoryContents.entrySet()) {
        #     def entryList = e.getValue()
        #     fos.writeInt(e.key.length()*2); // 2 bytes per char
        #     fos.writeChars(e.key);
        #     fos.writeInt(entryList.size());
        #     for (String entry: entryList) {
        #         fos.writeInt(entry.length()*2);
        #         fos.writeChars(entry);
        #     }
        # }

    def bundle(self):
        if not os.path.isfile(self.xmlFile):
            print("Bundling Qt info into {}".format(self.xmlFile))
            self.copyQtDeps()
            self.scanQmlImports()
            self.processFiles()
        # if not os.path.isfile(self.qtAssetCacheList):
        #     self.generateAssetsFileList()


