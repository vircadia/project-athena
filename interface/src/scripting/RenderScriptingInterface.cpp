//
//  Created by Sam Gondelman on 5/16/19
//  Copyright 2013-2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "RenderScriptingInterface.h"

#include "LightingModel.h"
#include "AntialiasingEffect.h"


RenderScriptingInterface* RenderScriptingInterface::getInstance() {
    static RenderScriptingInterface sharedInstance;
    return &sharedInstance;
}

std::once_flag RenderScriptingInterface::registry_flag;

RenderScriptingInterface::RenderScriptingInterface() {
    std::call_once(registry_flag, [] {
        qmlRegisterType<RenderScriptingInterface>("RenderEnums", 1, 0, "RenderEnums");
    });
}

void RenderScriptingInterface::loadSettings() {
    _renderSettingLock.withReadLock([&] {
        _renderMethod = (_renderMethodSetting.get());
        _shadowsEnabled = (_shadowsEnabledSetting.get());
        _ambientOcclusionEnabled = (_ambientOcclusionEnabledSetting.get());
        _antialiasingEnabled = (_antialiasingEnabledSetting.get());
        _viewportResolutionScale = (_viewportResolutionScaleSetting.get());
    });
    forceRenderMethod((RenderMethod)_renderMethod);
    forceShadowsEnabled(_shadowsEnabled);
    forceAmbientOcclusionEnabled(_ambientOcclusionEnabled);
    forceAntialiasingEnabled(_antialiasingEnabled);
    forceViewportResolutionScale(_viewportResolutionScale);
}

RenderScriptingInterface::RenderMethod RenderScriptingInterface::getRenderMethod() const {
    return (RenderMethod) _renderMethod;
}

void RenderScriptingInterface::setRenderMethod(RenderMethod renderMethod) {
    if (isValidRenderMethod(renderMethod) && (_renderMethod != (int) renderMethod)) {
        forceRenderMethod(renderMethod);
        emit settingsChanged();
    }
}
void RenderScriptingInterface::forceRenderMethod(RenderMethod renderMethod) {
    _renderSettingLock.withWriteLock([&] {
        _renderMethod = (int)renderMethod;
        _renderMethodSetting.set((int)renderMethod);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        auto config = dynamic_cast<render::SwitchConfig*>(renderConfig->getConfig("RenderMainView.DeferredForwardSwitch"));
        if (config) {
            config->setBranch((int)renderMethod);
        }

        auto secondaryConfig = dynamic_cast<render::SwitchConfig*>(renderConfig->getConfig("RenderSecondView.DeferredForwardSwitch"));
        if (secondaryConfig) {
            secondaryConfig->setBranch((int)renderMethod);
        }
    });
}

QStringList RenderScriptingInterface::getRenderMethodNames() const {
    static const QStringList refrenderMethodNames = { "DEFERRED", "FORWARD" };
    return refrenderMethodNames;
}

bool RenderScriptingInterface::getShadowsEnabled() const {
    return _shadowsEnabled;
}

void RenderScriptingInterface::setShadowsEnabled(bool enabled) {
    if (_shadowsEnabled != enabled) {
        forceShadowsEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceShadowsEnabled(bool enabled) {
    _renderSettingLock.withWriteLock([&] {
        _shadowsEnabled = (enabled);
        _shadowsEnabledSetting.set(enabled);
        Menu::getInstance()->setIsOptionChecked(MenuOption::Shadows, enabled);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        assert(renderConfig);
        auto lightingModelConfig = renderConfig->getConfig<MakeLightingModel>("RenderMainView.LightingModel");
        if (lightingModelConfig) {
            lightingModelConfig->setShadow(enabled);
        }
        auto secondaryLightingModelConfig = renderConfig->getConfig<MakeLightingModel>("RenderSecondView.LightingModel");
        if (secondaryLightingModelConfig) {
            secondaryLightingModelConfig->setShadow(enabled);
        }
    });
}

bool RenderScriptingInterface::getAmbientOcclusionEnabled() const {
    return _ambientOcclusionEnabled;
}

void RenderScriptingInterface::setAmbientOcclusionEnabled(bool enabled) {
    if (_ambientOcclusionEnabled != enabled) {
        forceAmbientOcclusionEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceAmbientOcclusionEnabled(bool enabled) {
    _renderSettingLock.withWriteLock([&] {
        _ambientOcclusionEnabled = (enabled);
        _ambientOcclusionEnabledSetting.set(enabled);
        Menu::getInstance()->setIsOptionChecked(MenuOption::AmbientOcclusion, enabled);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        auto lightingModelConfig = renderConfig->getConfig<MakeLightingModel>("RenderMainView.LightingModel");
        if (lightingModelConfig) {
            lightingModelConfig->setAmbientOcclusion(enabled);
        }

        auto secondaryLightingModelConfig = renderConfig->getConfig<MakeLightingModel>("RenderSecondView.LightingModel");
        if (secondaryLightingModelConfig) {
            secondaryLightingModelConfig->setAmbientOcclusion(enabled);
        }
    });
}

bool RenderScriptingInterface::getAntialiasingEnabled() const {
    return _antialiasingEnabled;
}

void RenderScriptingInterface::setAntialiasingEnabled(bool enabled) {
    if (_antialiasingEnabled != enabled) {
        forceAntialiasingEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceAntialiasingEnabled(bool enabled) {
    _renderSettingLock.withWriteLock([&] {
        _antialiasingEnabled = (enabled);
        _antialiasingEnabledSetting.set(enabled);
        Menu::getInstance()->setIsOptionChecked(MenuOption::AntiAliasing, enabled);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        auto mainViewAntialiasingSetupConfig  = renderConfig->getConfig<AntialiasingSetup>("RenderMainView.AntialiasingSetup");
        auto mainViewAntialiasingConfig = renderConfig->getConfig<Antialiasing>("RenderMainView.Antialiasing");
        if (mainViewAntialiasingSetupConfig && mainViewAntialiasingConfig) {
            if (enabled) {
                mainViewAntialiasingSetupConfig ->play();
                mainViewAntialiasingConfig->setDebugFXAA(false);
            } else {
                mainViewAntialiasingSetupConfig ->none();
                mainViewAntialiasingConfig->setDebugFXAA(true);
            }
        }

        auto secondViewAntialiasingSetupConfig  = renderConfig->getConfig<AntialiasingSetup>("RenderSecondView.AntialiasingSetup");
        auto secondViewAntialiasingConfig = renderConfig->getConfig<Antialiasing>("RenderSecondView.Antialiasing");
        if (secondViewAntialiasingSetupConfig && secondViewAntialiasingConfig) {
            if (enabled) {
                secondViewAntialiasingSetupConfig ->play();
                secondViewAntialiasingConfig->setDebugFXAA(false);
            } else {
                secondViewAntialiasingSetupConfig ->none();
                secondViewAntialiasingConfig->setDebugFXAA(true);
            }
        }
    });
}


float RenderScriptingInterface::getViewportResolutionScale() const {
    return _viewportResolutionScale;
}

void RenderScriptingInterface::setViewportResolutionScale(float scale) {
    if (_viewportResolutionScale != scale) {
        forceViewportResolutionScale(scale);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceViewportResolutionScale(float scale) {
    // just not negative values or zero
    if (scale <= 0.f) {
        return;
    }
    _renderSettingLock.withWriteLock([&] {
        _viewportResolutionScale = scale;
        _viewportResolutionScaleSetting.set(scale);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        assert(renderConfig);
        auto deferredView = renderConfig->getConfig("RenderMainView.RenderDeferredTask");
        // mainView can be null if we're rendering in forward mode
        if (deferredView) {
            deferredView->setProperty("resolutionScale", scale);
        }
        auto forwardView = renderConfig->getConfig("RenderMainView.RenderForwardTask");
        // mainView can be null if we're rendering in forward mode
        if (forwardView) {
            forwardView->setProperty("resolutionScale", scale);
        }

        auto deferredSecondView = renderConfig->getConfig("RenderSecondView.RenderDeferredTask");
        // mainView can be null if we're rendering in forward mode
        if (deferredSecondView) {
            deferredSecondView->setProperty("resolutionScale", scale);
        }
        auto forwardSecondView = renderConfig->getConfig("RenderMainView.RenderForwardTask");
        // mainView can be null if we're rendering in forward mode
        if (forwardSecondView) {
            forwardSecondView->setProperty("resolutionScale", scale);
        }
    });
}
