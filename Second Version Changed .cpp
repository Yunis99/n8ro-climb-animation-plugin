#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct arkheon_frame {
    double sim_time_s;
    double delta_time_s;
    uint64_t frame_number;
};

struct arkheon_vec3 { float x, y, z; };
struct arkheon_quat { float x, y, z, w; };

struct arkheon_bone_state {
    arkheon_vec3 local_translation;
    arkheon_quat local_rotation;
    arkheon_vec3 local_scale;
};

struct arkheon_bone_override {
    arkheon_vec3 local_translation;
    arkheon_quat local_rotation;
    arkheon_vec3 local_scale;
    int32_t apply; 
};

struct arkheon_input_state { int32_t dummy; };
struct arkheon_mission_goal { int32_t dummy; };
struct arkheon_env_api { void* dummy; };

#if defined(_WIN32)
#define ARKHEON_CHAR_EXPORT __declspec(dllexport)
#else
#define ARKHEON_CHAR_EXPORT __attribute__((visibility("default")))
#endif

// --- Matematiksel Yardımcı Fonksiyonlar ---
inline arkheon_quat quat_identity() {
    return { 0.0f, 0.0f, 0.0f, 1.0f };
}

inline arkheon_quat quat_normalize(arkheon_quat q) {
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (len > 0.00001f) {
        return { q.x / len, q.y / len, q.z / len, q.w / len };
    }
    return quat_identity();
}

inline arkheon_quat quat_from_axis_angle(float ax, float ay, float az, float angle_rad) {
    float half_angle = angle_rad * 0.5f;
    float sin_half = std::sin(half_angle);
    return { ax * sin_half, ay * sin_half, az * sin_half, std::cos(half_angle) };
}

struct ClimbPlugin {
    float simTime;
    float climbSpeed;
    ClimbPlugin() {
        simTime = 0.0f;
        climbSpeed = 3.5f;
    }
};

extern "C" {

    ARKHEON_CHAR_EXPORT const char* arkheon_character_plugin_name(void) {
        return "DefaultHumanInterface";
    }

    ARKHEON_CHAR_EXPORT void* arkheon_character_init(const char* config_json) {
        return new ClimbPlugin();
    }

    ARKHEON_CHAR_EXPORT void arkheon_character_shutdown(void* handle) {
        if (handle) delete static_cast<ClimbPlugin*>(handle);
    }

    ARKHEON_CHAR_EXPORT int32_t arkheon_character_tick(
        void* handle, const arkheon_frame* frame, const arkheon_bone_state in_bones[20],
        arkheon_bone_override out_overrides[20], arkheon_vec3* out_root_translation_delta,
        arkheon_quat* out_root_rotation_delta, const arkheon_input_state* input,
        const arkheon_mission_goal* current_goal, const arkheon_env_api* env)
    {
        if (!handle || !out_overrides) return 1;
        
        ClimbPlugin* p = static_cast<ClimbPlugin*>(handle);
        float dt = (frame && frame->delta_time_s > 0.0) ? static_cast<float>(frame->delta_time_s) : 0.02f;
        p->simTime += dt;

        float wave = std::sin(p->simTime * p->climbSpeed);
        float cosWave = std::cos(p->simTime * p->climbSpeed);
        float deg2rad = M_PI / 180.0f;

        // Bütün override dizisini sıfırla ve aktif et
        for (int i = 0; i < 20; ++i) {
            out_overrides[i].local_rotation = quat_identity();
            out_overrides[i].apply = 1; // Motora her eklemi zorla ezmesini söylüyoruz
        }

        // --- Kolları Dinamik Oynat ---
        out_overrides[3].local_rotation = quat_from_axis_angle(1, 0, 0, (wave * 40.0f + 85.0f) * deg2rad); // Sol Üst Kol
        out_overrides[4].local_rotation = quat_from_axis_angle(1, 0, 0, (-wave * 40.0f + 85.0f) * deg2rad); // Sağ Üst Kol
        out_overrides[5].local_rotation = quat_from_axis_angle(1, 0, 0, 35.0f * deg2rad); // Sol Alt Kol
        out_overrides[6].local_rotation = quat_from_axis_angle(1, 0, 0, 35.0f * deg2rad); // Sağ Alt Kol

        // --- Arkadaşının Kodundaki Titreme Mantığını İndekslere Körlemesine Dağıtıyoruz ---
        // Motorun bacak indeksini kaçırma ihtimaline karşı 7'den 14'e kadar olan tüm alt eklemlere
        // arkadaşının kodundaki o salınım hareketini senkronize basıyoruz. Bacak hangisiyse kesin yakalanacak.
        for (int j = 7; j <= 14; ++j) {
            if (j % 2 == 0) {
                out_overrides[j].local_rotation = quat_from_axis_angle(1, 0, 0, (cosWave * 20.0f + 15.0f) * deg2rad);
            } else {
                out_overrides[j].local_rotation = quat_from_axis_angle(1, 0, 0, (-cosWave * 20.0f + 15.0f) * deg2rad);
            }
        }

        out_root_translation_delta->x = 0.0f;
        out_root_translation_delta->y = 0.35f * dt; // Yukarı tırmanma fiziği
        out_root_translation_delta->z = 0.0f;
        *out_root_rotation_delta = quat_identity();

        return 0;
    }
}