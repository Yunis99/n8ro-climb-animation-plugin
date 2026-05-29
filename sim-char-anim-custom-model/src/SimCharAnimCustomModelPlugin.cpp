// Arkheon Simulation Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Simulation Technologies. All rights reserved.

#include "SimCharAnimCustomModelPlugin.h"

#include <model/AnimationModel.h>
#include <model/IModel.h>
#include <model/ModelFactoryRegistry.h>
#include <plugin/IModelPluginService.h>
#include <plugin/PluginContext.h>
#include <plugin/IPluginServices.h>

#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>

namespace arkheon::sample::simcharanimcustommodel {
namespace {

class CustomAnimationModel final : public arkheon::astsim::IModel, public arkheon::astsim::IAnimationModel {
public:
    [[nodiscard]] std::string getTypeName() const override {
        // Platformun eski önbelleği (cache) baypas etmesi için yeni model tipi tanımlandı
        return "animationModelClimbV2";
    }
    [[nodiscard]] std::unique_ptr<arkheon::astsim::IModel> clone() const override {
        return std::make_unique<CustomAnimationModel>(*this);
    }

    [[nodiscard]] bool evaluate(
        const arkheon::astsim::AnimationModelInput& input,
        arkheon::astsim::AnimationModelOutput& output) override {
        
        // Giriş kodunu zorlayarak T-pose veya default kol sallama kilidini devre dışı bırakıyoruz
        output.clearExistingJointOverrides = true;

        // Kinematik Zaman ve Hız Döngüsü
        const double t = input.simulationTimeSeconds * 4.0;
        
        // Kolları 180 derece savurmak yerine gerçekçi tırmanma açısına daraltıyoruz (Genlik: 0.35)
        const double armCycle = std::sin(t) * 0.35;
        
        // Bacakları kollara tam zıt fazda (Kosinüs dalgası ile) merdiven adımına zorluyoruz
        const double legCycle = std::cos(t) * 0.30;

        // ------------------ ÜST GÖVDE (Kolların Dar Açılı Tırmanışı) ------------------
        output.jointOverrides.push_back({"leftShoulder", armCycle, 0.0, 0.0});
        output.jointOverrides.push_back({"rightShoulder", -armCycle, 0.0, 0.0});
        output.jointOverrides.push_back({"leftElbow", std::abs(armCycle) * 0.6, 0.0, 0.0});
        output.jointOverrides.push_back({"rightElbow", std::abs(-armCycle) * 0.6, 0.0, 0.0});

        // ------------------ ALT GÖVDE (Orijinal Hiyerarşik İsimler) ------------------
        output.jointOverrides.push_back({"leftThigh", legCycle, 0.0, 0.0});
        output.jointOverrides.push_back({"rightThigh", -legCycle, 0.0, 0.0});
        output.jointOverrides.push_back({"leftCalf", std::abs(legCycle) * 0.7, 0.0, 0.0});
        output.jointOverrides.push_back({"rightCalf", std::abs(-legCycle) * 0.7, 0.0, 0.0});

        // ------------------ OMURGA VE DURUŞ (Öne Eğilme Dengesi) ------------------
        output.jointOverrides.push_back({"spine", 0.15, 0.0, 0.0});

        return true; 
    }

private:
    [[nodiscard]] static bool hasJoint(
        const std::unordered_set<std::string>& availableJointIds,
        const char* jointId) {
        if (!jointId || *jointId == '\0') return false;
        if (availableJointIds.empty()) return true;
        return availableJointIds.find(jointId) != availableJointIds.end();
    }
};

} // namespace

int SimCharAnimCustomModelPlugin::getInterfaceVersion() const { return 1; }

arkheon::astlib::PluginMetadata SimCharAnimCustomModelPlugin::getMetadata() const {
    arkheon::astlib::PluginMetadata metadata;
    // Benzersiz eklenti kimliği (Plugin ID) ile sistem hafızası sıfırlanıyor
    metadata.setPluginId("sim-char-anim-climb-v2");
    metadata.setVersion("1.0.0");
    metadata.setAuthor("Arkheon Sample");
    return metadata;
}

void SimCharAnimCustomModelPlugin::initialize(arkheon::astlib::PluginContext& context) {
    initialized_ = true;
    shutdown_ = false;
    modelRegistered_ = false;
    modelType_ = "animationModelClimbV2";
    modelFactoryRegistry_ = nullptr;

    if (context.services) {
        auto* rawService = context.services->getService(arkheon::astsim::IModelPluginService::kPluginServiceId);
        auto* service = static_cast<arkheon::astsim::IModelPluginService*>(rawService);
        modelFactoryRegistry_ = service ? &service->modelFactoryRegistry() : nullptr;
    }
    if (!modelFactoryRegistry_) return;

    modelRegistered_ = modelFactoryRegistry_->registerFactory(
        modelType_,
        std::make_unique<CustomAnimationModel>());
}

void SimCharAnimCustomModelPlugin::tick(double dt) { static_cast<void>(dt); }

void SimCharAnimCustomModelPlugin::shutdown() {
    if (modelFactoryRegistry_ && modelRegistered_) {
        static_cast<void>(modelFactoryRegistry_->unregisterFactory(modelType_));
    }
    modelRegistered_ = false; 
    shutdown_ = true;
    modelFactoryRegistry_ = nullptr;
}

} // namespace arkheon::sample::simcharanimcustommodel

extern "C" {
ARKHEON_ASTLIB_API arkheon::astlib::IPlugin* create_plugin() {
    return new arkheon::sample::simcharanimcustommodel::SimCharAnimCustomModelPlugin();
}
ARKHEON_ASTLIB_API void destroy_plugin(arkheon::astlib::IPlugin* plugin) {
    if (plugin) delete plugin;
}
ARKHEON_ASTLIB_API const char* get_plugin_signature() {
    return "ARKHEON_PLUGIN_V1";
}
}