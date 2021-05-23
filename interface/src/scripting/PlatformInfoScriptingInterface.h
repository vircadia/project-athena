//
//  Created by Nissim Hadar on 2018/12/28
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PlatformInfoScriptingInterface_h
#define hifi_PlatformInfoScriptingInterface_h

#include <platform/Profiler.h>
#include <QtCore/QObject>

/*@jsdoc
 * The <code>PlatformInfo</code> API provides information about the hardware platform being used.
 *
 * @namespace PlatformInfo
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */
class PlatformInfoScriptingInterface : public QObject {
    Q_OBJECT


public:
    PlatformInfoScriptingInterface();
    virtual ~PlatformInfoScriptingInterface();

    /*@jsdoc
     * <p>The platform tier of a computer is an indication of its rendering capability.</p>
     * <table>
     *   <thead>
     *     <tr><th>Value</th><th>Name</th><th>Description</th></tr>
     *   </thead>
     *   <tbody>
     *     <tr><td><code>0</code></td><td>UNKNOWN</td><td>Unknown rendering capability.</td></tr>
     *     <tr><td><code>1</code></td><td>LOW</td><td>Low-end PC, capable of rendering low-quality graphics.</td></tr>
     *     <tr><td><code>2</code></td><td>MID</td><td>Business-class PC, capable of rendering medium-quality graphics.</td></tr>
     *     <tr><td><code>3</code></td><td>HIGH</td><td>High-end PC, capable of rendering high-quality graphics.</td></tr>
     *   </tbody>
     * </table>
     * @typedef {number} PlatformInfo.PlatformTier
     */
    // Platform tier enum type
    enum PlatformTier {
        UNKNOWN = platform::Profiler::Tier::UNKNOWN,
        LOW = platform::Profiler::Tier::LOW,
        MID = platform::Profiler::Tier::MID,
        HIGH = platform::Profiler::Tier::HIGH,
    };
    Q_ENUM(PlatformTier);

public slots:
    /*@jsdoc
     * @function PlatformInfo.getInstance
     * @deprecated This function is deprecated and will be removed.
     */
    static PlatformInfoScriptingInterface* getInstance();

    /*@jsdoc
     * Gets the operating system type.
     * @function PlatformInfo.getOperatingSystemType
     * @returns {string} The operating system type: <code>"WINDOWS"</code>, <code>"MACOS"</code>, or <code>"UNKNOWN"</code>.
     * @deprecated This function is deprecated and will be removed.
     *     Use <code>JSON.parse({@link PlatformInfo.getComputer|PlatformInfo.getComputer()}).OS</code> instead.
     */
    QString getOperatingSystemType();

    /*@jsdoc
     * Gets information on the CPU model.
     * @function PlatformInfo.getCPUBrand
     * @returns {string} Information on the CPU.
     * @deprecated This function is deprecated and will be removed.
     *     Use <code>JSON.parse({@link PlatformInfo.getCPU|PlatformInfo.getCPU(0)}).model</code> instead.
     */
    QString getCPUBrand();

    /*@jsdoc
     * Gets the number of logical CPU cores.
     * @function PlatformInfo.getNumLogicalCores
     * @returns {number} The number of logical CPU cores.
     * @deprecated This function is deprecated and will be removed.
     *     Use <code>JSON.parse({@link PlatformInfo.getCPU|PlatformInfo.getCPU(0)}).numCores</code> instead.
     */
    unsigned int getNumLogicalCores();

    /*@jsdoc
     * Gets the total amount of usable physical memory, in MB.
     * @function PlatformInfo.getTotalSystemMemoryMB
     * @returns {number} The total system memory in megabytes.
     * @deprecated This function is deprecated and will be removed.
     *     Use <code>JSON.parse({@link PlatformInfo.getMemory|PlatformInfo.getMemory()}).memTotal</code> instead.
     */
    int getTotalSystemMemoryMB();

    /*@jsdoc
     * Gets the model of the graphics card currently being used.
     * @function PlatformInfo.getGraphicsCardType
     * @returns {string} The model of the graphics card currently being used.
     * @deprecated This function is deprecated and will be removed.
     *     Use <code>JSON.parse({@link PlatformInfo.getGPU|PlatformInfo.getGPU(} 
     *     {@link PlatformInfo.getMasterGPU|PlatformInfo.getMasterGPU() )}).model</code> 
     *     instead.
     */
    QString getGraphicsCardType();

    /*@jsdoc
     * Checks whether Oculus Touch controllers are connected.
     * @function PlatformInfo.hasRiftControllers
     * @returns {boolean} <code>true</code> if Oculus Touch controllers are connected, <code>false</code> if they aren't.
     */
    bool hasRiftControllers();

    /*@jsdoc
     * Checks whether Vive controllers are connected.
     * @function PlatformInfo.hasViveControllers
     * @returns {boolean} <code>true</code> if Vive controllers are connected, <code>false</code> if they aren't.
     */
    bool hasViveControllers();

    /*@jsdoc
     * Checks whether HTML on 3D surfaces (e.g., Web entities) is supported.
     * @function PlatformInfo.has3DHTML
     * @returns {boolean} <code>true</code> if the current display supports HTML on 3D surfaces, <code>false</code> if it 
     *     doesn't.
     */
    bool has3DHTML();

    /*@jsdoc
     * Checks whether Interface is running on a stand-alone HMD device (CPU incorporated into the HMD display).
     * @function PlatformInfo.isStandalone
     * @returns {boolean} <code>true</code> if Interface is running on a stand-alone HMD device, <code>false</code> if it isn't.
     */
    bool isStandalone();

    /*@jsdoc
     * Gets the number of CPUs.
     * @function PlatformInfo.getNumCPUs
     * @returns {number} The number of CPUs.
     */
    int getNumCPUs();

    /*@jsdoc
     * Gets the index number of the master CPU.
     * @function PlatformInfo.getMasterCPU
     * @returns {number} The index of the master CPU.
     */
    int getMasterCPU();

    /*@jsdoc
     * Gets the platform description of a CPU.
     * @function PlatformInfo.getCPU
     * @param {number} index - The index number of the CPU.
     * @returns {string} The CPU's {@link PlatformInfo.CPUDescription|CPUDescription} information as a JSON string.
     * @example <caption>Report details of the computer's CPUs.</caption>
     * var numCPUs = PlatformInfo.getNumCPUs();
     * print("Number of CPUs: " + numCPUs);
     * for (var i = 0; i < numCPUs; i++) {
     *     var cpuDescription = PlatformInfo.getCPU(i);
     *     print("CPU " + i + ": " + cpuDescription);
     * }
     */
    QString getCPU(int index);

    /*@jsdoc
     * Gets the number of GPUs.
     * @function PlatformInfo.getNumGPUs
     * @returns {number} The number of GPUs.
     */
    int getNumGPUs();

    /*@jsdoc
     * Gets the index number of the master GPU.
     * @function PlatformInfo.getMasterGPU
     * @returns {number} The index of the master GPU.
     */
    int getMasterGPU();

    /*@jsdoc
     * Gets the platform description of a GPU.
     * @param {number} index - The index number of the GPU.
     * @function PlatformInfo.getGPU
     * @returns {string} The GPU's {@link PlatformInfo.GPUDescription|GPUDescription} information as a JSON string.
     * @example <caption>Report details of the computer's GPUs.</caption>
     * var numGPUs = PlatformInfo.getNumGPUs();
     * print("Number of GPUs: " + numGPUs);
     * for (var i = 0; i < numGPUs; i++) {
     *     var gpuDescription = PlatformInfo.getGPU(i);
     *     print("GPU " + i + ": " + gpuDescription);
     * }
     */
    QString getGPU(int index);

    /*@jsdoc
     * Gets the number of displays.
     * @function PlatformInfo.getNumDisplays
     * @returns {number} The number of displays.
     */
    int getNumDisplays();

    /*@jsdoc
     * Gets the index number of the master display.
     * @function PlatformInfo.getMasterDisplay
     * @returns {number} The index of the master display.
     */
    int getMasterDisplay();

    /*@jsdoc
     * Gets the platform description of a display.
     * @param {number} index - The index number of the display.
     * @function PlatformInfo.getDisplay
     * @returns {string} The display's {@link PlatformInfo.DisplayDescription|DisplayDescription} information as a JSON string.
     * @example <caption>Report details of the systems's displays.</caption>
     * var numDisplays = PlatformInfo.getNumDisplays();
     * print("Number of displays: " + numDisplays);
     * for (var i = 0; i < numDisplays; i++) {
     *     var displayDescription = PlatformInfo.getDisplay(i);
     *     print("Display " + i + ": " + displayDescription);
     * }
     */
    QString getDisplay(int index);

    /*@jsdoc
     * Gets the platform description of computer memory.
     * @function PlatformInfo.getMemory
     * @returns {string} The computer's {@link PlatformInfo.MemoryDescription|MemoryDescription} information as a JSON string.
     * @example <caption>Report details of the computer's memory.</caption>
     * print("Memory: " + PlatformInfo.getMemory());
     */
    QString getMemory();

    /*@jsdoc
     * Gets the platform description of the computer.
     * @function PlatformInfo.getComputer
     * @returns {string} The {@link PlatformInfo.ComputerDescription|ComputerDescription} information as a JSON string.
     */
    QString getComputer();

    /*@jsdoc
     * Gets the complete description of the computer as a whole.
     * @function PlatformInfo.getPlatform
     * @returns {string} The {@link PlatformInfo.PlatformDescription|PlatformDescription} information as a JSON string.
     */
    QString getPlatform();

    /*@jsdoc
     * Gets the platform tier of the computer, profiled at Interface start-up.
     * @function PlatformInfo.getTierProfiled
     * @returns {PlatformInfo.PlatformTier} The platform tier of the computer.
     * @example <caption>Report the platform tier of the computer.</caption>
     * var platformTier = PlatformInfo.getTierProfiled();
     * var platformTierName = PlatformInfo.getPlatformTierNames()[platformTier];
     * print("Platform tier: " + platformTier + ", " + platformTierName);
     */
    PlatformTier getTierProfiled();

    /*@jsdoc
     * Gets the names of the possible platform tiers, per {@link PlatformInfo.PlatformTier}.
     * @function PlatformInfo.getPlatformTierNames
     * @returns {string[]} The names of the possible platform tiers.
     */
    QStringList getPlatformTierNames();

    /*@jsdoc
     * Gets whether the current hardware can use deferred rendering.
     * @function PlatformInfo.isRenderMethodDeferredCapable
     * @returns {boolean} <code>true</code> if the current hardware can use deferred rendering, <code>false</code> if it can't. 
     */
    bool isRenderMethodDeferredCapable();
};

#endif  // hifi_PlatformInfoScriptingInterface_h
