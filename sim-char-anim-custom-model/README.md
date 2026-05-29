// Arkheon Simulation Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Simulation Technologies. All rights reserved.

# sim-char-anim-custom-model

This sample plugin demonstrates how to provide `animationModelCustom` for non-Nathan rigs by registering a model prototype.

## What This Sample Demonstrates

- Runtime plugin contract (`arkheon::astlib::IPlugin`)
- Model prototype registration with `IModelPluginService::modelFactoryRegistry().registerFactory(...)`
- Custom model type (`animationModelCustom`) and animation code (`Custom Idle`)
- Non-Nathan workflow where users define model profile/schema variants manually
- Initialize-time registration only (runtime hot registration is not supported)

## Manual UI/Schema Steps (Required)

1. Create or edit a **Model** profile in UI:
   - `type = animation`
   - `type/animation = animationModelCustom`
   - `type/animation/animationModelCustom/ecmRole = animation`
2. Add your custom animation metadata list under the custom model branch (for example `Custom Idle`).
3. In the target `componentAnimation` profile, bind this model profile in `modelList` (role resolves to `animation` via `ecmRole`).
4. Set `componentAnimation/config/defaultAnimation = Custom Idle`.
5. Use your non-Nathan skeleton/rig mapping data in your custom model schema branch.

## Build

```bat
open-solution.cmd
```

## Output

- Built DLL: `%N8RO_RELEASE%\dev\samples\sim\sim-char-anim-custom-model\bin\release\sim-char-anim-custom-model.dll`
- Auto-deployed DLL: `%N8RO_RELEASE_USER_SIM_PLUGINS%\sim-char-anim-custom-model.dll`
